/**
 * @file debugger.cpp
 * @author Sina Karvandi (sina@rayanfam.com)
 * @brief Interpret general fields
 * @details
 * @version 0.1
 * @date 2020-05-27
 *
 * @copyright This project is released under the GNU Public License v3.
 *
 */
#include "pch.h"

//
// Global Variables
//
extern UINT64 g_EventTag;
extern LIST_ENTRY g_EventTrace;
extern BOOLEAN g_EventTraceInitialized;
extern BOOLEAN g_BreakPrintingOutput;
extern BOOLEAN g_AutoUnpause;

/**
 * @brief shows the error message
 *
 * @param Error
 * @return BOOLEAN
 */
BOOLEAN
ShowErrorMessage(UINT32 Error) {
  switch (Error) {
  case DEBUGEER_ERROR_TAG_NOT_EXISTS:
    ShowMessages("err, tag not found (%x)\n", Error);
    break;

  case DEBUGEER_ERROR_INVALID_ACTION_TYPE:
    ShowMessages("err, invalid action type (%x)\n", Error);
    break;

  case DEBUGEER_ERROR_ACTION_BUFFER_SIZE_IS_ZERO:
    ShowMessages("err, action buffer size is zero (%x)\n", Error);
    break;

  case DEBUGEER_ERROR_EVENT_TYPE_IS_INVALID:
    ShowMessages("err, event type is invalid (%x)\n", Error);
    break;

  case DEBUGEER_ERROR_UNABLE_TO_CREATE_EVENT:
    ShowMessages("err, unable to create event (%x)\n", Error);
    break;

  case DEBUGEER_ERROR_INVALID_ADDRESS:
    ShowMessages("err, invalid address (%x)\n", Error);
    break;

  case DEBUGEER_ERROR_INVALID_CORE_ID:
    ShowMessages("err, invalid core id (%x)\n", Error);
    break;

  case DEBUGEER_ERROR_EXCEPTION_INDEX_EXCEED_FIRST_32_ENTRIES:
    ShowMessages("err, exception index exceed first 32 entries (%x)\n", Error);
    break;

  case DEBUGEER_ERROR_INTERRUPT_INDEX_IS_NOT_VALID:
    ShowMessages("err, interrupt index is not valid (%x)\n", Error);
    break;

  case DEBUGEER_ERROR_UNABLE_TO_HIDE_OR_UNHIDE_DEBUGGER:
    ShowMessages("err, unable to hide or unhide debugger (%x)\n", Error);
    break;

  case DEBUGEER_ERROR_DEBUGGER_ALREADY_UHIDE:
    ShowMessages("err, debugger already unhide (%x)\n", Error);
    break;

  case DEBUGGER_ERROR_EDIT_MEMORY_STATUS_INVALID_PARAMETER:
    ShowMessages("err, edit memeory request has invalid parameters (%x)\n",
                 Error);
    break;

  case DEBUGGER_ERROR_EDIT_MEMORY_STATUS_INVALID_ADDRESS_BASED_ON_CURRENT_PROCESS:
    ShowMessages("err, edit memory request has invalid address based on "
                 "current process layout (%x)\n",
                 Error);
    break;

  case DEBUGGER_ERROR_EDIT_MEMORY_STATUS_INVALID_ADDRESS_BASED_ON_OTHER_PROCESS:
    ShowMessages("err, edit memory request has invalid address based on other "
                 "process layout (%x)\n",
                 Error);
    break;

  case DEBUGGER_ERROR_MODIFY_EVENTS_INVALID_TAG:
    ShowMessages("err, modify event with invalid tag (%x)\n", Error);
    break;

  case DEBUGGER_ERROR_MODIFY_EVENTS_INVALID_TYPE_OF_ACTION:
    ShowMessages("err, modify event with invalid type of action (%x)\n", Error);
    break;

  case DEBUGGER_ERROR_STEPPING_INVALID_PARAMETER:
    ShowMessages("err, invalid parameter passsed to stepping core. (%x)\n",
                 Error);
    break;

  case DEBUGGER_ERROR_STEPPINGS_EITHER_THREAD_NOT_FOUND_OR_DISABLED:
    ShowMessages(
        "err, target thread not found or the thread is disabled (%x)\n", Error);
    break;

  default:
    ShowMessages("err, error not found (%x)\n", Error);
    return FALSE;
    break;
  }

  return TRUE;
}

/**
 * @brief Check whether the tag exists or not, if the tag is
 * DEBUGGER_MODIFY_EVENTS_APPLY_TO_ALL_TAG then if we find just one event, it
 * also means that the tag is found
 *
 * @param Tag
 * @return BOOLEAN
 */
BOOLEAN IsTagExist(UINT64 Tag) {

  PLIST_ENTRY TempList = 0;
  PDEBUGGER_GENERAL_EVENT_DETAIL CommandDetail = {0};

  if (!g_EventTraceInitialized) {
    return FALSE;
  }

  TempList = &g_EventTrace;
  while (&g_EventTrace != TempList->Blink) {
    TempList = TempList->Blink;

    CommandDetail = CONTAINING_RECORD(TempList, DEBUGGER_GENERAL_EVENT_DETAIL,
                                      CommandsEventList);

    if (CommandDetail->Tag == Tag ||
        Tag == DEBUGGER_MODIFY_EVENTS_APPLY_TO_ALL_TAG) {

      return TRUE;
    }
  }

  //
  // Not found
  //
  return FALSE;
}

/**
 * @brief Interpret script (if an event has script)
 * @details If this function returns true then it means that there is a
 * script in this command split and the details are returned in the
 * input structure
 *
 * @param SplittedCommand the initialized command that are splitted by space
 * @param BufferAddress the address that the allocated buffer will be saved on
 * it
 * @param BufferLength the length of the buffer
 * @return BOOLEAN shows whether the interpret was successful (true) or not
 * successful (false)
 */
BOOLEAN
InterpretScript(vector<string> *SplittedCommand, PUINT64 BufferAddress,
                PUINT32 BufferLength) {

  BOOLEAN IsTextVisited = FALSE;
  BOOLEAN IsInState = FALSE;
  BOOLEAN IsEnded = FALSE;
  string Temp;
  string AppendedFinalBuffer;
  vector<string> SaveBuffer;
  vector<int> IndexesToRemove;
  UCHAR *FinalBuffer;
  int Index = 0;
  int NewIndexToRemove = 0;
  int OpenBracket = 0;
  size_t CountOfOpenBrackets;
  size_t CountOfCloseBrackets;

  for (auto Section : *SplittedCommand) {

    Index++;

    if (IsInState) {

      if (OpenBracket == 0 && Section.find("{") == string::npos) {

        //
        // Check if the buffer is ended or not
        //
        if (!Section.compare("}")) {

          //
          // Save to remove this string from the command
          //
          IndexesToRemove.push_back(Index);
          IsEnded = TRUE;
          break;
        }

        //
        // Check if the script is end or not
        //
        if (HasEnding(Section, "}")) {

          //
          // Save to remove this string from the command
          //
          IndexesToRemove.push_back(Index);

          //
          // remove the last character and append it to the ConditionBuffer
          //
          SaveBuffer.push_back(Section.substr(0, Section.size() - 1));

          IsEnded = TRUE;
          break;
        }
      }

      //
      // Save to remove this string from the command
      //
      IndexesToRemove.push_back(Index);

      //
      // Check if script contains bracket "{"
      //
      if (Section.find("{") != string::npos) {

        //
        // We find a { between the script
        //

        //
        // Check the count of brackets in the string and add it to OpenBracket
        //
        size_t CountOfBrackets = count(Section.begin(), Section.end(), '{');

        //
        // Add it to the open brackets
        //
        OpenBracket += CountOfBrackets;
      }

      //
      // Check if script contains bracket "}"
      //
      if (Section.find("}") != string::npos) {

        //
        // We find a } between the script
        //

        //
        // Check the count of brackets in the string and add it to OpenBracket
        //
        size_t CountOfBrackets = count(Section.begin(), Section.end(), '}');

        //
        // Add it to the open brackets
        //
        OpenBracket -= CountOfBrackets;

        if (OpenBracket < 0) {
          OpenBracket = 0;
          IsEnded = TRUE;
          SaveBuffer.push_back(Section.substr(0, Section.size() - 1));
          break;
        }
      }

      //
      // Add the text as the script screen
      //
      SaveBuffer.push_back(Section);

      //
      // We want to stay in this condition
      //
      continue;
    }

    if (IsTextVisited && !Section.compare("{")) {

      //
      // Save to remove this string from the command
      //
      IndexesToRemove.push_back(Index);

      IsInState = TRUE;
      continue;
    }

    if (IsTextVisited && Section.rfind("{", 0) == 0) {

      //
      // Section starts with {
      //

      //
      // Check if script contains bracket "{"
      //
      if (Section.find("{") != string::npos) {

        //
        // We find a { between the script
        //

        //
        // Check the count of brackets in the string and add it to OpenBracket
        //
        size_t CountOfBrackets = count(Section.begin(), Section.end(), '{');

        //
        // Add it to the open brackets (-1 because script starts with { which
        // is not related to our interpretation of script)
        //
        OpenBracket += CountOfBrackets - 1;
      }

      //
      // Check if it ends with }
      //
      if (OpenBracket == 0 && HasEnding(Section, "}")) {

        //
        // Save to remove this string from the command
        //
        IndexesToRemove.push_back(Index);

        Temp = Section.erase(0, 1);
        SaveBuffer.push_back(Temp.substr(0, Temp.size() - 1));

        IsEnded = TRUE;
        break;
      }

      //
      // Check if script contains bracket "}"
      //
      if (Section.find("}") != string::npos) {

        //
        // We find a } between the script
        //

        //
        // Check the count of brackets in the string and add it to OpenBracket
        //
        size_t CountOfBrackets = count(Section.begin(), Section.end(), '}');

        //
        // Add it to the open brackets
        //
        OpenBracket -= CountOfBrackets;

        if (OpenBracket < 0) {
          OpenBracket = 0;
          IsEnded = TRUE;
          SaveBuffer.push_back(Section.substr(0, Section.size() - 1));
          break;
        }
      }

      //
      // Save to remove this string from the command
      //
      IndexesToRemove.push_back(Index);

      SaveBuffer.push_back(Section.erase(0, 1));

      IsInState = TRUE;
      continue;
    }

    //
    // Check if it's a script
    //
    if (!Section.compare("script")) {

      //
      // Save to remove this string from the command
      //
      IndexesToRemove.push_back(Index);

      IsTextVisited = TRUE;
      continue;
    }

    //
    // Check if it's a script with script{
    //
    if (!Section.compare("script{")) {

      //
      // Save to remove this string from the command
      //
      IndexesToRemove.push_back(Index);

      IsTextVisited = TRUE;
      IsInState = TRUE;
      continue;
    }

    //
    // It's a script
    //
    if (Section.rfind("script{", 0) == 0) {

      //
      // Save to remove this string from the command
      //
      IndexesToRemove.push_back(Index);

      IsTextVisited = TRUE;
      IsInState = TRUE;
      CountOfOpenBrackets = count(Section.begin(), Section.end(), '{');
      CountOfCloseBrackets = count(Section.begin(), Section.end(), '}');

      //
      // Check if script contains bracket "{"
      //
      if (Section.find("{") != string::npos) {

        //
        // We find a { between the script
        //

        //
        // Add it to the open brackets (-1 because script starts with { which
        // is not related to our interpretation of script)
        //
        OpenBracket += CountOfOpenBrackets - 1;
      }

      if (CountOfOpenBrackets == CountOfCloseBrackets ||
          (OpenBracket == 0 && HasEnding(Section, "}"))) {
        //
        // remove the last character and first character append it to the
        // ConditionBuffer
        //
        Temp = Section.erase(0, 7);
        SaveBuffer.push_back(Temp.substr(0, Temp.size() - 1));

        IsEnded = TRUE;
        OpenBracket = 0;
        break;

      } else {

        //
        // Section starts with script{
        //
        SaveBuffer.push_back(Section.erase(0, 7));

        //
        // Check if script contains bracket "}"
        //
        if (Section.find("}") != string::npos) {

          //
          // We find a } between the script
          //

          //
          // Check the count of brackets in the string and add it to OpenBracket
          //
          size_t CountOfBrackets = count(Section.begin(), Section.end(), '}');

          //
          // Add it to the open brackets
          //
          OpenBracket -= CountOfBrackets;

          if (OpenBracket < 0) {
            OpenBracket = 0;
            IsEnded = TRUE;
          }
        }

        continue;
      }
    }
  }

  //
  // Now we have everything in script buffer
  // Check to see if it is empty or not
  //
  if (SaveBuffer.size() == 0) {
    //
    // Nothing in condition buffer, return zero
    //
    return FALSE;
  }

  //
  // Check if we see that all the '{' ended with '}' or not
  //
  if (OpenBracket != 0) {
    //
    // Not all open brackets close with a }
    //
    return FALSE;
  }

  //
  // Check if we see '}' at the end
  //
  if (!IsEnded) {
    //
    // Nothing in condition buffer, return zero
    //
    return FALSE;
  }

  //
  // If we reach here then there is sth in script buffer
  //
  for (auto Section : SaveBuffer) {

    AppendedFinalBuffer.append(Section);
    AppendedFinalBuffer.append(" ");
  }

  printf("script : %s\n", AppendedFinalBuffer.c_str());

  //
  // Run script engine handler
  //
  PSYMBOL_BUFFER CodeBuffer =
      ScriptEngineParse((char *)AppendedFinalBuffer.c_str());

  //
  // Print symbols (test)
  //
  PrintSymbolBuffer(CodeBuffer);

  //
  // Set the buffer and length
  //
  *BufferAddress = (UINT64)CodeBuffer->Head;
  *BufferLength = CodeBuffer->Size;

  //
  // Removing the script indexes from the command
  //
  NewIndexToRemove = 0;

  for (auto IndexToRemove : IndexesToRemove) {
    NewIndexToRemove++;

    SplittedCommand->erase(SplittedCommand->begin() +
                           (IndexToRemove - NewIndexToRemove));
  }

  return TRUE;
}

/**
 * @brief Interpret conditions (if an event has condition) and custom code
 * @details If this function returns true then it means that there is a condtion
 * or code buffer in this command split and the details are returned in the
 * input structure
 *
 * @param SplittedCommand the initialized command that are splitted by space
 * @param IsConditionBuffer is it a condition buffer or a custom code buffer
 * @param BufferAddress the address that the allocated buffer will be saved on
 * it
 * @param BufferLength the length of the buffer
 * @return BOOLEAN shows whether the interpret was successful (true) or not
 * successful (false)
 */
BOOLEAN
InterpretConditionsAndCodes(vector<string> *SplittedCommand,
                            BOOLEAN IsConditionBuffer, PUINT64 BufferAddress,
                            PUINT32 BufferLength) {
  BOOLEAN IsTextVisited = FALSE;
  BOOLEAN IsInState = FALSE;
  BOOLEAN IsEnded = FALSE;
  string Temp;
  string AppendedFinalBuffer;
  vector<string> SaveBuffer;
  vector<CHAR> ParsedBytes;
  vector<int> IndexesToRemove;
  UCHAR *FinalBuffer;
  int NewIndexToRemove = 0;
  int Index = 0;

  for (auto Section : *SplittedCommand) {
    Index++;

    if (IsInState) {

      //
      // Check if the buffer is ended or not
      //
      if (!Section.compare("}")) {
        //
        // Save to remove this string from the command
        //
        IndexesToRemove.push_back(Index);
        IsEnded = TRUE;
        break;
      }

      //
      // Check if the condition is end or not
      //
      if (HasEnding(Section, "}")) {
        //
        // Save to remove this string from the command
        //
        IndexesToRemove.push_back(Index);

        //
        // remove the last character and append it to the ConditionBuffer
        //
        SaveBuffer.push_back(Section.substr(0, Section.size() - 1));

        IsEnded = TRUE;
        break;
      }

      //
      // Save to remove this string from the command
      //
      IndexesToRemove.push_back(Index);

      //
      // Add the codes into condition bi
      //
      SaveBuffer.push_back(Section);

      //
      // We want to stay in this condition
      //
      continue;
    }

    if (IsTextVisited && !Section.compare("{")) {

      //
      // Save to remove this string from the command
      //
      IndexesToRemove.push_back(Index);

      IsInState = TRUE;
      continue;
    }

    if (IsTextVisited && Section.rfind("{", 0) == 0) {

      //
      // Section starts with {
      //

      //
      // Check if it ends with }
      //
      if (HasEnding(Section, "}")) {

        //
        // Save to remove this string from the command
        //
        IndexesToRemove.push_back(Index);

        Temp = Section.erase(0, 1);
        SaveBuffer.push_back(Temp.substr(0, Temp.size() - 1));

        IsEnded = TRUE;
        break;
      }

      //
      // Save to remove this string from the command
      //
      IndexesToRemove.push_back(Index);

      SaveBuffer.push_back(Section.erase(0, 1));

      IsInState = TRUE;
      continue;
    }

    if (IsConditionBuffer) {
      if (!Section.compare("condition")) {

        //
        // Save to remove this string from the command
        //
        IndexesToRemove.push_back(Index);

        IsTextVisited = TRUE;
        continue;
      }
    } else {

      //
      // It's code
      //
      if (!Section.compare("code")) {

        //
        // Save to remove this string from the command
        //
        IndexesToRemove.push_back(Index);

        IsTextVisited = TRUE;
        continue;
      }
    }

    if (IsConditionBuffer) {
      if (!Section.compare("condition{")) {

        //
        // Save to remove this string from the command
        //
        IndexesToRemove.push_back(Index);

        IsTextVisited = TRUE;
        IsInState = TRUE;
        continue;
      }
    } else {

      //
      // It's code
      //
      if (!Section.compare("code{")) {

        //
        // Save to remove this string from the command
        //
        IndexesToRemove.push_back(Index);

        IsTextVisited = TRUE;
        IsInState = TRUE;
        continue;
      }
    }

    if (IsConditionBuffer) {
      if (Section.rfind("condition{", 0) == 0) {

        //
        // Save to remove this string from the command
        //
        IndexesToRemove.push_back(Index);

        IsTextVisited = TRUE;
        IsInState = TRUE;

        if (!HasEnding(Section, "}")) {

          //
          // Section starts with condition{
          //
          SaveBuffer.push_back(Section.erase(0, 10));
          continue;
        } else {

          //
          // remove the last character and first character append it to the
          // ConditionBuffer
          //
          Temp = Section.erase(0, 10);
          SaveBuffer.push_back(Temp.substr(0, Temp.size() - 1));

          IsEnded = TRUE;
          break;
        }
      }
    } else {

      //
      // It's a code
      //
      if (Section.rfind("code{", 0) == 0) {

        //
        // Save to remove this string from the command
        //
        IndexesToRemove.push_back(Index);

        IsTextVisited = TRUE;
        IsInState = TRUE;

        if (!HasEnding(Section, "}")) {

          //
          // Section starts with code{
          //
          SaveBuffer.push_back(Section.erase(0, 5));
          continue;
        } else {

          //
          // remove the last character and first character append it to the
          // ConditionBuffer
          //
          Temp = Section.erase(0, 5);
          SaveBuffer.push_back(Temp.substr(0, Temp.size() - 1));

          IsEnded = TRUE;
          break;
        }
      }
    }
  }

  //
  // Now we have everything in condition buffer
  // Check to see if it is empty or not
  //
  if (SaveBuffer.size() == 0) {
    //
    // Nothing in condition buffer, return zero
    //
    return FALSE;
  }

  //
  // Check if we see '}' at the end
  //
  if (!IsEnded) {
    //
    // Nothing in condition buffer, return zero
    //
    return FALSE;
  }

  //
  // Append a 'ret' at the end of the buffer
  //
  SaveBuffer.push_back("c3");

  //
  // If we reach here then there is sth in condition buffer
  //
  for (auto Section : SaveBuffer) {

    //
    // Check if the section is started with '0x'
    //
    if (Section.rfind("0x", 0) == 0 || Section.rfind("0X", 0) == 0 ||
        Section.rfind("\\x", 0) == 0 || Section.rfind("\\X", 0) == 0) {
      Temp = Section.erase(0, 2);
    } else if (Section.rfind("x", 0) == 0 || Section.rfind("X", 0) == 0) {
      Temp = Section.erase(0, 1);
    } else {
      Temp = Section;
    }

    //
    // replace \x s
    //
    ReplaceAll(Temp, "\\x", "");

    //
    // check if the buffer is aligned to 2
    //
    if (Temp.size() % 2 != 0) {

      //
      // Add a zero to the start of the buffer
      //
      Temp.insert(0, 1, '0');
    }

    if (!IsHexNotation(Temp)) {
      ShowMessages("Please enter condition code in a hex notation.\n");
      return FALSE;
    }
    AppendedFinalBuffer.append(Temp);
  }

  //
  // Convert it to vectored bytes
  //
  ParsedBytes = HexToBytes(AppendedFinalBuffer);

  //
  // Convert to a contigues buffer
  //
  FinalBuffer = (unsigned char *)malloc(ParsedBytes.size());
  std::copy(ParsedBytes.begin(), ParsedBytes.end(), FinalBuffer);

  //
  // Set the buffer and length
  //
  *BufferAddress = (UINT64)FinalBuffer;
  *BufferLength = ParsedBytes.size();

  //
  // Removing the code or condition indexes from the command
  //
  NewIndexToRemove = 0;
  for (auto IndexToRemove : IndexesToRemove) {
    NewIndexToRemove++;

    SplittedCommand->erase(SplittedCommand->begin() +
                           (IndexToRemove - NewIndexToRemove));
  }

  return TRUE;
}

/**
 * @brief Interpret output (if an event has special output)
 * @details If this function returns true then it means that there is a special
 * input that needs to be considered for this event (other than just printing)
 * like sending over network, save to file, and send over a namedpipe
 *
 * @param SplittedCommand the initialized command that are splitted by space
 * @param BufferAddress the address that the allocated buffer will be saved on
 * it
 * @param BufferLength the length of the buffer
 * @return BOOLEAN shows whether the interpret was successful (true) or not
 * successful (false)
 */
BOOLEAN
InterpretOutput(vector<string> *SplittedCommand, PUINT64 BufferAddress,
                PUINT32 BufferLength) {

  BOOLEAN IsTextVisited = FALSE;
  BOOLEAN IsInState = FALSE;
  BOOLEAN IsEnded = FALSE;
  string Temp;
  string AppendedFinalBuffer;
  vector<string> SaveBuffer;
  vector<int> IndexesToRemove;
  int NewIndexToRemove = 0;
  int Index = 0;

  for (auto Section : *SplittedCommand) {

    Index++;

    if (IsInState) {

      //
      // Check if the buffer is ended or not
      //
      if (!Section.compare("}")) {
        //
        // Save to remove this string from the command
        //
        IndexesToRemove.push_back(Index);
        IsEnded = TRUE;
        break;
      }

      //
      // Check if the output is end or not
      //
      if (HasEnding(Section, "}")) {
        //
        // Save to remove this string from the command
        //
        IndexesToRemove.push_back(Index);

        //
        // remove the last character and append it to the output buffer
        //
        SaveBuffer.push_back(Section.substr(0, Section.size() - 1));

        IsEnded = TRUE;
        break;
      }

      //
      // Save to remove this string from the command
      //
      IndexesToRemove.push_back(Index);

      //
      // Add the codes into buffer buffer
      //
      SaveBuffer.push_back(Section);

      //
      // We want to stay in this condition
      //
      continue;
    }

    if (IsTextVisited && !Section.compare("{")) {

      //
      // Save to remove this string from the command
      //
      IndexesToRemove.push_back(Index);

      IsInState = TRUE;
      continue;
    }

    if (IsTextVisited && Section.rfind("{", 0) == 0) {

      //
      // Section starts with {
      //

      //
      // Check if it ends with }
      //
      if (HasEnding(Section, "}")) {

        //
        // Save to remove this string from the command
        //
        IndexesToRemove.push_back(Index);

        Temp = Section.erase(0, 1);
        SaveBuffer.push_back(Temp.substr(0, Temp.size() - 1));

        IsEnded = TRUE;
        break;
      }

      //
      // Save to remove this string from the command
      //
      IndexesToRemove.push_back(Index);

      SaveBuffer.push_back(Section.erase(0, 1));

      IsInState = TRUE;
      continue;
    }

    if (!Section.compare("output")) {

      //
      // Save to remove this string from the command
      //
      IndexesToRemove.push_back(Index);

      IsTextVisited = TRUE;
      continue;
    }

    if (!Section.compare("output{")) {

      //
      // Save to remove this string from the command
      //
      IndexesToRemove.push_back(Index);

      IsTextVisited = TRUE;
      IsInState = TRUE;
      continue;
    }

    if (Section.rfind("output{", 0) == 0) {

      //
      // Save to remove this string from the command
      //
      IndexesToRemove.push_back(Index);

      IsTextVisited = TRUE;
      IsInState = TRUE;

      if (!HasEnding(Section, "}")) {

        //
        // Section starts with output{
        //
        SaveBuffer.push_back(Section.erase(0, 7));
        continue;
      } else {

        //
        // remove the last character and first character append it to the
        // Output
        //
        Temp = Section.erase(0, 7);
        SaveBuffer.push_back(Temp.substr(0, Temp.size() - 1));

        IsEnded = TRUE;
        break;
      }
    }
  }

  //
  // Now we have everything in buffer buffer
  // Check to see if it is empty or not
  //
  if (SaveBuffer.size() == 0) {
    //
    // Nothing in output buffer, return zero
    //
    return FALSE;
  }

  //
  // Check if we see '}' at the end
  //
  if (!IsEnded) {
    //
    // Nothing in output buffer, return zero
    //
    return FALSE;
  }

  //
  // If we reach here then there is sth in condition buffer
  //
  for (auto Section : SaveBuffer) {

    AppendedFinalBuffer.append(Section);
    AppendedFinalBuffer.append(" ");
  }

  printf("Output : %s\n", AppendedFinalBuffer.c_str());

  //
  // Set the buffer and length
  //
  //*BufferAddress = (UINT64)FinalBuffer;
  //*BufferLength = ParsedBytes.size();

  //
  // Removing the code or condition indexes from the command
  //
  NewIndexToRemove = 0;
  for (auto IndexToRemove : IndexesToRemove) {
    NewIndexToRemove++;

    SplittedCommand->erase(SplittedCommand->begin() +
                           (IndexToRemove - NewIndexToRemove));
  }

  return TRUE;
}

/**
 * @brief Register the event to the kernel
 *
 * @param Event the event structure to send
 * @param EventBufferLength the buffer length of event
 * @return BOOLEAN if the request was successful then true
 * if the request was not successful then false
 */
BOOLEAN
SendEventToKernel(PDEBUGGER_GENERAL_EVENT_DETAIL Event,
                  UINT32 EventBufferLength) {
  BOOL Status;
  ULONG ReturnedLength;
  DEBUGGER_EVENT_AND_ACTION_REG_BUFFER ReturnedBuffer = {0};

  //
  // Test
  //

  /*
  ShowMessages("Tag : %llx\n", Event->Tag);
  ShowMessages("Command String : %s\n", Event->CommandStringBuffer);
  ShowMessages("CoreId : 0x%x\n", Event->CoreId);
  ShowMessages("Pid : 0x%x\n", Event->ProcessId);
  ShowMessages("Optional Param 1 : %llx\n", Event->OptionalParam1);
  ShowMessages("Optional Param 2 : %llx\n", Event->OptionalParam2);
  ShowMessages("Optional Param 3 : %llx\n", Event->OptionalParam3);
  ShowMessages("Optional Param 4 : %llx\n", Event->OptionalParam4);
  ShowMessages("Count of Actions : %d\n", Event->CountOfActions);
  ShowMessages("Event Type : %d\n", Event->EventType);
  return TRUE;
  */

  if (!g_DeviceHandle) {
    ShowMessages("Handle not found, probably the driver is not loaded. Did you "
                 "use 'load' command?\n");
    return FALSE;
  }

  //
  // Send IOCTL
  //

  Status =
      DeviceIoControl(g_DeviceHandle,                // Handle to device
                      IOCTL_DEBUGGER_REGISTER_EVENT, // IO Control code
                      Event,                         // Input Buffer to driver.
                      EventBufferLength,             // Input buffer length
                      &ReturnedBuffer, // Output Buffer from driver.
                      sizeof(DEBUGGER_EVENT_AND_ACTION_REG_BUFFER), // Length
                                                                    // of
                                                                    // output
                                                                    // buffer
                                                                    // in
                                                                    // bytes.
                      &ReturnedLength, // Bytes placed in buffer.
                      NULL             // synchronous call
      );

  if (!Status) {
    ShowMessages("ioctl failed with code 0x%x\n", GetLastError());
    return FALSE;
  }

  if (ReturnedBuffer.IsSuccessful && ReturnedBuffer.Error == 0) {

    //
    // The returned buffer shows that this event was
    // successful and now we can add it to the list of
    // events
    //
    InsertHeadList(&g_EventTrace, &(Event->CommandsEventList));

    //
    // Check for auto-unpause mode
    //
    if (g_BreakPrintingOutput && g_AutoUnpause) {

      //
      // Allow debugger to show its contents
      //
      ShowMessages("\n");
      g_BreakPrintingOutput = FALSE;
    }

  } else {
    //
    // It was not successful, we have to free the buffers for command
    // string and buffer for the event itself
    //
    free(Event->CommandStringBuffer);
    free(Event);

    //
    // Show the error
    //
    if (ReturnedBuffer.Error != 0) {
      ShowErrorMessage(ReturnedBuffer.Error);
    }
  }

  return TRUE;
}

/**
 * @brief Register the action to the event
 *
 * @param Action the action buffer structure
 * @param ActionsBufferLength length of action buffer
 * @return BOOLEAN BOOLEAN if the request was successful then true
 * if the request was not successful then false
 */
BOOLEAN
RegisterActionToEvent(PDEBUGGER_GENERAL_ACTION ActionBreakToDebugger,
                      UINT32 ActionBreakToDebuggerLength,
                      PDEBUGGER_GENERAL_ACTION ActionCustomCode,
                      UINT32 ActionCustomCodeLength,
                      PDEBUGGER_GENERAL_ACTION ActionScript,
                      UINT32 ActionScriptLength) {
  BOOL Status;
  ULONG ReturnedLength;
  DEBUGGER_EVENT_AND_ACTION_REG_BUFFER ReturnedBuffer = {0};

  if (!g_DeviceHandle) {
    ShowMessages("Handle not found, probably the driver is not loaded. Did you "
                 "use 'load' command?\n");
    return FALSE;
  }

  //
  // Send IOCTLs
  //

  //
  // Send Break to debugger ioctl
  //
  if (ActionBreakToDebugger != NULL) {
    Status =
        DeviceIoControl(g_DeviceHandle,                     // Handle to device
                        IOCTL_DEBUGGER_ADD_ACTION_TO_EVENT, // IO Control code
                        ActionBreakToDebugger,       // Input Buffer to driver.
                        ActionBreakToDebuggerLength, // Input buffer length
                        &ReturnedBuffer, // Output Buffer from driver.
                        sizeof(DEBUGGER_EVENT_AND_ACTION_REG_BUFFER), // Length
                                                                      // of
                                                                      // output
                                                                      // buffer
                                                                      // in
                                                                      // bytes.
                        &ReturnedLength, // Bytes placed in buffer.
                        NULL             // synchronous call
        );

    //
    // The action buffer is allocated once using malloc and
    // is not used anymore, we have to free it
    //
    free(ActionBreakToDebugger);

    if (!Status) {
      ShowMessages("ioctl failed with code 0x%x\n", GetLastError());
      return FALSE;
    }
  }

  //
  // Send custom code ioctl
  //
  if (ActionCustomCode != NULL) {
    Status =
        DeviceIoControl(g_DeviceHandle,                     // Handle to device
                        IOCTL_DEBUGGER_ADD_ACTION_TO_EVENT, // IO Control code
                        ActionCustomCode,       // Input Buffer to driver.
                        ActionCustomCodeLength, // Input buffer length
                        &ReturnedBuffer,        // Output Buffer from driver.
                        sizeof(DEBUGGER_EVENT_AND_ACTION_REG_BUFFER), // Length
                                                                      // of
                                                                      // output
                                                                      // buffer
                                                                      // in
                                                                      // bytes.
                        &ReturnedLength, // Bytes placed in buffer.
                        NULL             // synchronous call
        );

    //
    // The action buffer is allocated once using malloc and
    // is not used anymore, we have to free it
    //
    free(ActionCustomCode);

    if (!Status) {
      ShowMessages("ioctl failed with code 0x%x\n", GetLastError());
      return FALSE;
    }
  }

  //
  // Send custom code ioctl
  //
  if (ActionScript != NULL) {
    Status =
        DeviceIoControl(g_DeviceHandle,                     // Handle to device
                        IOCTL_DEBUGGER_ADD_ACTION_TO_EVENT, // IO Control code
                        ActionScript,       // Input Buffer to driver.
                        ActionScriptLength, // Input buffer length
                        &ReturnedBuffer,    // Output Buffer from driver.
                        sizeof(DEBUGGER_EVENT_AND_ACTION_REG_BUFFER), // Length
                                                                      // of
                                                                      // output
                                                                      // buffer
                                                                      // in
                                                                      // bytes.
                        &ReturnedLength, // Bytes placed in buffer.
                        NULL             // synchronous call
        );

    //
    // The action buffer is allocated once using malloc and
    // is not used anymore, we have to free it
    //
    free(ActionScript);

    if (!Status) {
      ShowMessages("ioctl failed with code 0x%x\n", GetLastError());
      return FALSE;
    }
  }

  return TRUE;
}

/**
 * @brief Get the New Debugger Event Tag object and increase the
 * global variable for tag
 *
 * @return UINT64
 */
UINT64 GetNewDebuggerEventTag() { return g_EventTag++; }

/**
 * @brief Interpret general event fields
 *
 * @param SplittedCommand the commands that was splitted by space
 * @param EventType type of event
 * @param EventDetailsToFill a pointer address that will be filled
 * by event detail buffer
 * @param EventBufferLength a pointer the receives the buffer length
 * of the event
 * @param ActionDetailsToFill a pointer address that will be filled
 * by action detail buffer
 * @param ActionBufferLength a pointer the receives the buffer length
 * of the action
 * @return BOOLEAN  If this function returns true then it means that there
 * was no error in parsing the general event details
 */
BOOLEAN InterpretGeneralEventAndActionsFields(
    vector<string> *SplittedCommand, DEBUGGER_EVENT_TYPE_ENUM EventType,
    PDEBUGGER_GENERAL_EVENT_DETAIL *EventDetailsToFill,
    PUINT32 EventBufferLength,
    PDEBUGGER_GENERAL_ACTION *ActionDetailsToFillBreakToDebugger,
    PUINT32 ActionBufferLengthBreakToDebugger,
    PDEBUGGER_GENERAL_ACTION *ActionDetailsToFillCustomCode,
    PUINT32 ActionBufferLengthCustomCode,
    PDEBUGGER_GENERAL_ACTION *ActionDetailsToFillScript,
    PUINT32 ActionBufferLengthScript) {

  PDEBUGGER_GENERAL_EVENT_DETAIL TempEvent;
  PDEBUGGER_GENERAL_ACTION TempActionBreak = NULL;
  PDEBUGGER_GENERAL_ACTION TempActionScript = NULL;
  PDEBUGGER_GENERAL_ACTION TempActionCustomCode = NULL;
  UINT32 LengthOfCustomCodeActionBuffer = 0;
  UINT32 LengthOfScriptActionBuffer = 0;
  UINT32 LengthOfBreakActionBuffer = 0;
  UINT64 ConditionBufferAddress;
  UINT32 ConditionBufferLength = 0;
  UINT64 OutputBufferAddress;
  UINT32 OutputBufferLength = 0;
  UINT64 CodeBufferAddress;
  UINT32 CodeBufferLength = 0;
  UINT64 ScriptBufferAddress;
  UINT32 ScriptBufferLength = 0;
  UINT32 LengthOfEventBuffer = 0;
  string CommandString;
  BOOLEAN HasConditionBuffer = FALSE;
  BOOLEAN HasOutputPath = FALSE;
  BOOLEAN HasCodeBuffer = FALSE;
  BOOLEAN HasScript = FALSE;
  BOOLEAN IsNextCommandPid = FALSE;
  BOOLEAN IsNextCommandCoreId = FALSE;
  BOOLEAN IsNextCommandBufferSize = FALSE;
  UINT32 CoreId;
  UINT32 ProcessId;
  UINT32 RequestBuffer = 0;
  vector<int> IndexesToRemove;
  int NewIndexToRemove = 0;
  int Index = 0;

  //
  // Create a command string to show in the history
  //
  for (auto Section : *SplittedCommand) {

    CommandString.append(Section);
    CommandString.append(" ");
  }
  //
  // Compute the size of buffer + 1 null for the end of buffer
  //

  UINT64 BufferOfCommandStringLength = CommandString.size() + 1;

  //
  // Allocate Buffer and zero for command to the buffer
  //
  PVOID BufferOfCommandString = malloc(BufferOfCommandStringLength);

  RtlZeroMemory(BufferOfCommandString, BufferOfCommandStringLength);

  //
  // Copy the string to the buffer
  //
  memcpy(BufferOfCommandString, CommandString.c_str(), CommandString.size());

  //
  // Check if there is a condition buffer in the command
  //
  if (!InterpretConditionsAndCodes(SplittedCommand, TRUE,
                                   &ConditionBufferAddress,
                                   &ConditionBufferLength)) {

    //
    // Indicate condition is not available
    //
    HasConditionBuffer = FALSE;

    //
    // ShowMessages("\nNo condition!\n");
    //

  } else {
    //
    // Indicate condition is available
    //
    HasConditionBuffer = TRUE;

    /*
    ShowMessages(
        "\n========================= Condition =========================\n");

    ShowMessages(
        "\nUINT64  DebuggerCheckForCondition(PGUEST_REGS Regs_RCX, PVOID "
        "Context_RDX)\n{\n");

    //
    // Disassemble the buffer
    //
    HyperDbgDisassembler64((unsigned char *)ConditionBufferAddress, 0x0,
                           ConditionBufferLength);

    ShowMessages("}\n\n");

    ShowMessages(
        "=============================================================\n");
      */
  }

  //
  // Check if there is a code buffer in the command
  //
  if (!InterpretConditionsAndCodes(SplittedCommand, FALSE, &CodeBufferAddress,
                                   &CodeBufferLength)) {

    //
    // Indicate code is not available
    //
    HasCodeBuffer = FALSE;
    //
    // ShowMessages("\nNo custom code!\n");
    //
  } else {

    //
    // Indicate code is available
    //
    HasCodeBuffer = TRUE;

    /*
    ShowMessages(
        "\n=========================    Code    =========================\n");
    ShowMessages("\nPVOID DebuggerRunCustomCodeFunc(PVOID "
                 "PreAllocatedBufferAddress_RCX, "
                 "PGUEST_REGS Regs_RDX, PVOID Context_R8)\n{\n");

    //
    // Disassemble the buffer
    //
    HyperDbgDisassembler64((unsigned char *)CodeBufferAddress, 0x0,
                           CodeBufferLength);

    ShowMessages("}\n\n");

    ShowMessages(
        "=============================================================\n");
        */
  }

  //
  // Check if there is a Script block in the command
  //
  if (!InterpretScript(SplittedCommand, &ScriptBufferAddress,
                       &ScriptBufferLength)) {

    //
    // Indicate code is not available
    //
    HasScript = FALSE;
    //
    // ShowMessages("\nNo script!\n");
    //
  } else {

    //
    // Indicate code is available
    //
    HasScript = TRUE;
  }

  //
  // Check if there is a output path in the command
  //
  if (!InterpretOutput(SplittedCommand, &OutputBufferAddress,
                       &OutputBufferLength)) {

    //
    // Indicate output is not available
    //
    HasOutputPath = FALSE;

    //
    // ShowMessages("\nNo condition!\n");
    //

  } else {
    //
    // Indicate output is available
    //
    HasOutputPath = TRUE;
  }

  //
  // Create action and event based on previously parsed buffers
  // (DEBUGGER_GENERAL_ACTION)
  //
  // Allocate the buffer (with ConditionBufferLength and CodeBufferLength)
  //

  /*

  Layout of Buffer :

   ________________________________
  |                                |
  |  DEBUGGER_GENERAL_EVENT_DETAIL |
  |                                |
  |________________________________|
  |                                |
  |       Condition Buffer         |
  |                                |
  |________________________________|

   */

  /*
   ________________________________
  |                                |
  |     DEBUGGER_GENERAL_ACTION    |
  |                                |
  |________________________________|
  |                                |
  |     Condition Custom Code      |
  |       or Script Buffer         |
  |________________________________|

  */

  LengthOfEventBuffer =
      sizeof(DEBUGGER_GENERAL_EVENT_DETAIL) + ConditionBufferLength;

  TempEvent = (PDEBUGGER_GENERAL_EVENT_DETAIL)malloc(LengthOfEventBuffer);
  RtlZeroMemory(TempEvent, LengthOfEventBuffer);

  //
  // Check if buffer is availabe
  //
  if (TempEvent == NULL) {
    //
    // The heap is not available
    //
    free(BufferOfCommandString);
    return FALSE;
  }

  //
  // Event is enabled by default when created
  //
  TempEvent->IsEnabled = TRUE;

  //
  // Get a new tag for it
  //
  TempEvent->Tag = GetNewDebuggerEventTag();

  //
  // Set the core Id and Process Id to all cores and all
  // processes, next time we check whether the user needs
  // a special core or a special process then we change it
  //
  TempEvent->CoreId = DEBUGGER_EVENT_APPLY_TO_ALL_CORES;
  TempEvent->ProcessId = DEBUGGER_EVENT_APPLY_TO_ALL_PROCESSES;

  //
  // Set the event type
  //
  TempEvent->EventType = EventType;

  //
  // Get the current time
  //
  TempEvent->CreationTime = time(0);

  //
  // Set buffer string command
  //
  TempEvent->CommandStringBuffer = BufferOfCommandString;

  //
  // Fill the buffer of condition for event
  //
  if (HasConditionBuffer) {

    memcpy((PVOID)((UINT64)TempEvent + sizeof(DEBUGGER_GENERAL_EVENT_DETAIL)),
           (PVOID)ConditionBufferAddress, ConditionBufferLength);

    //
    // Set the size of the buffer for event condition
    //
    TempEvent->ConditionBufferSize = ConditionBufferLength;
  }

  //
  // Fill the buffer of custom code for action
  //
  if (HasCodeBuffer) {

    //
    // Allocate the Action (THIS ACTION BUFFER WILL BE FREED WHEN WE SENT IT TO
    // THE KERNEL AND RETURNED FROM THE KERNEL AS WE DON'T NEED IT ANYMORE)
    //
    LengthOfCustomCodeActionBuffer =
        sizeof(DEBUGGER_GENERAL_ACTION) + CodeBufferLength;

    TempActionCustomCode =
        (PDEBUGGER_GENERAL_ACTION)malloc(LengthOfCustomCodeActionBuffer);

    RtlZeroMemory(TempActionCustomCode, LengthOfCustomCodeActionBuffer);

    memcpy(
        (PVOID)((UINT64)TempActionCustomCode + sizeof(DEBUGGER_GENERAL_ACTION)),
        (PVOID)CodeBufferAddress, CodeBufferLength);
    //
    // Set the action Tag
    //
    TempActionCustomCode->EventTag = TempEvent->Tag;

    //
    // Set the action type
    //
    TempActionCustomCode->ActionType = RUN_CUSTOM_CODE;

    //
    // Set the action buffer size
    //
    TempActionCustomCode->CustomCodeBufferSize = CodeBufferLength;

    //
    // Increase the count of actions
    //
    TempEvent->CountOfActions = TempEvent->CountOfActions + 1;
  }

  //
  // Fill the buffer of script for action
  //
  if (HasScript) {

    //
    // Allocate the Action (THIS ACTION BUFFER WILL BE FREED WHEN WE SENT IT TO
    // THE KERNEL AND RETURNED FROM THE KERNEL AS WE DON'T NEED IT ANYMORE)
    //
    LengthOfScriptActionBuffer =
        sizeof(DEBUGGER_GENERAL_ACTION) + ScriptBufferLength;
    TempActionScript =
        (PDEBUGGER_GENERAL_ACTION)malloc(LengthOfScriptActionBuffer);

    RtlZeroMemory(TempActionScript, LengthOfScriptActionBuffer);

    memcpy((PVOID)((UINT64)TempActionScript + sizeof(DEBUGGER_GENERAL_ACTION)),
           (PVOID)ScriptBufferAddress, ScriptBufferLength);
    //
    // Set the action Tag
    //
    TempActionScript->EventTag = TempEvent->Tag;

    //
    // Set the action type
    //
    TempActionScript->ActionType = RUN_SCRIPT;

    //
    // Set the action buffer size
    //
    TempActionScript->ScriptBufferSize = ScriptBufferLength;

    //
    // Increase the count of actions
    //
    TempEvent->CountOfActions = TempEvent->CountOfActions + 1;
  }

  //
  // If this action didn't contain a buffer for custom code and
  // a buffer for script then it's a break to debugger
  //
  if (!HasCodeBuffer && !HasScript) {

    //
    // Allocate the Action (THIS ACTION BUFFER WILL BE FREED WHEN WE SENT IT TO
    // THE KERNEL AND RETURNED FROM THE KERNEL AS WE DON'T NEED IT ANYMORE)
    //
    LengthOfBreakActionBuffer = sizeof(DEBUGGER_GENERAL_ACTION);

    TempActionBreak =
        (PDEBUGGER_GENERAL_ACTION)malloc(LengthOfBreakActionBuffer);

    RtlZeroMemory(TempActionBreak, LengthOfBreakActionBuffer);

    //
    // Set the action Tag
    //
    TempActionBreak->EventTag = TempEvent->Tag;

    //
    // Set the action type
    //
    TempActionBreak->ActionType = BREAK_TO_DEBUGGER;

    //
    // Increase the count of actions
    //
    TempEvent->CountOfActions = TempEvent->CountOfActions + 1;
  }

  //
  // Interpret rest of the command
  //
  for (auto Section : *SplittedCommand) {
    Index++;
    if (IsNextCommandBufferSize) {

      if (!ConvertStringToUInt32(Section, &RequestBuffer)) {
        return FALSE;
      } else {

        //
        // Set the specific requested buffer size
        //
        if (TempActionBreak != NULL) {
          TempActionBreak->PreAllocatedBuffer = RequestBuffer;
        }
        if (TempActionScript != NULL) {
          TempActionScript->PreAllocatedBuffer = RequestBuffer;
        }
        if (TempActionCustomCode != NULL) {
          TempActionCustomCode->PreAllocatedBuffer = RequestBuffer;
        }
      }
      IsNextCommandBufferSize = FALSE;

      //
      // Add index to remove it from the command
      //
      IndexesToRemove.push_back(Index);

      continue;
    }
    if (IsNextCommandPid) {

      if (!ConvertStringToUInt32(Section, &ProcessId)) {
        free(BufferOfCommandString);
        free(TempEvent);

        if (TempActionBreak != NULL) {
          free(TempActionBreak);
        }
        if (TempActionScript != NULL) {
          free(TempActionScript);
        }
        if (TempActionCustomCode != NULL) {
          free(TempActionCustomCode);
        }

        return FALSE;
      } else {

        //
        // Set the specific process id
        //
        TempEvent->ProcessId = ProcessId;
      }
      IsNextCommandPid = FALSE;

      //
      // Add index to remove it from the command
      //
      IndexesToRemove.push_back(Index);

      continue;
    }
    if (IsNextCommandCoreId) {

      if (!ConvertStringToUInt32(Section, &CoreId)) {
        free(BufferOfCommandString);
        free(TempEvent);

        if (TempActionBreak != NULL) {
          free(TempActionBreak);
        }
        if (TempActionScript != NULL) {
          free(TempActionScript);
        }
        if (TempActionCustomCode != NULL) {
          free(TempActionCustomCode);
        }
        return FALSE;
      } else {

        //
        // Set the specific core id
        //
        TempEvent->CoreId = CoreId;
      }
      IsNextCommandCoreId = FALSE;

      //
      // Add index to remove it from the command
      //
      IndexesToRemove.push_back(Index);

      continue;
    }

    if (!Section.compare("pid")) {
      IsNextCommandPid = TRUE;

      //
      // Add index to remove it from the command
      //
      IndexesToRemove.push_back(Index);

      continue;
    }
    if (!Section.compare("core")) {
      IsNextCommandCoreId = TRUE;

      //
      // Add index to remove it from the command
      //
      IndexesToRemove.push_back(Index);

      continue;
    }

    if (!Section.compare("buffer")) {
      IsNextCommandBufferSize = TRUE;

      //
      // Add index to remove it from the command
      //
      IndexesToRemove.push_back(Index);

      continue;
    }
  }

  //
  // Additional validation
  //
  if (IsNextCommandCoreId) {
    ShowMessages("error : please specify a value for 'core'\n\n");
    free(BufferOfCommandString);
    free(TempEvent);

    if (TempActionBreak != NULL) {
      free(TempActionBreak);
    }
    if (TempActionScript != NULL) {
      free(TempActionScript);
    }
    if (TempActionCustomCode != NULL) {
      free(TempActionCustomCode);
    }
    return FALSE;
  }
  if (IsNextCommandPid) {
    ShowMessages("error : please specify a value for 'pid'\n\n");
    free(BufferOfCommandString);
    free(TempEvent);

    if (TempActionBreak != NULL) {
      free(TempActionBreak);
    }
    if (TempActionScript != NULL) {
      free(TempActionScript);
    }
    if (TempActionCustomCode != NULL) {
      free(TempActionCustomCode);
    }
    return FALSE;
  }
  if (IsNextCommandBufferSize) {
    ShowMessages("errlr : please specify a value for 'buffer'\n\n");
    free(BufferOfCommandString);
    free(TempEvent);

    if (TempActionBreak != NULL) {
      free(TempActionBreak);
    }
    if (TempActionScript != NULL) {
      free(TempActionScript);
    }
    if (TempActionCustomCode != NULL) {
      free(TempActionCustomCode);
    }
    return FALSE;
  }

  //
  // Fill the address and length of event before release
  //
  *EventDetailsToFill = TempEvent;
  *EventBufferLength = LengthOfEventBuffer;

  //
  // Fill the address and length of action before release
  //
  if (TempActionBreak != NULL) {
    *ActionDetailsToFillBreakToDebugger = TempActionBreak;
    *ActionBufferLengthBreakToDebugger = LengthOfBreakActionBuffer;
  }
  if (TempActionScript != NULL) {
    *ActionDetailsToFillScript = TempActionScript;
    *ActionBufferLengthScript = LengthOfScriptActionBuffer;
  }
  if (TempActionCustomCode != NULL) {
    *ActionDetailsToFillCustomCode = TempActionCustomCode;
    *ActionBufferLengthCustomCode = LengthOfCustomCodeActionBuffer;
  }

  //
  // Remove the command that we interpreted above from the command
  //
  for (auto IndexToRemove : IndexesToRemove) {
    NewIndexToRemove++;
    SplittedCommand->erase(SplittedCommand->begin() +
                           (IndexToRemove - NewIndexToRemove));
  }

  //
  // Check if list is initialized or not
  //
  if (!g_EventTraceInitialized) {
    InitializeListHead(&g_EventTrace);
    g_EventTraceInitialized = TRUE;
  }

  //
  // Add the event to the trace list
  // UPDATE : if we add it here, then if the event was not
  // successful then still the event shows this event, so
  // we have to add it lated
  //
  // InsertHeadList(&g_EventTrace, &(TempEvent->CommandsEventList));

  //
  // Everything is ok, let's return TRUE
  //
  return TRUE;
}
