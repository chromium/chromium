// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_COMMON_WEB_TEST_BLINK_TEST_MESSAGES_H_
#define CONTENT_SHELL_COMMON_WEB_TEST_BLINK_TEST_MESSAGES_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"

#define IPC_MESSAGE_START BlinkTestMsgStart

// Tells the renderer to reset all test runners.
IPC_MESSAGE_ROUTED0(BlinkTestMsg_Reset)

// Tells the main window that a secondary renderer in a different process asked
// to finish the test.
IPC_MESSAGE_ROUTED0(BlinkTestMsg_TestFinishedInSecondaryRenderer)

// Notifies BlinkTestRunner that the layout dump has completed
// (and that it can proceed with finishing up the test).
IPC_MESSAGE_ROUTED1(BlinkTestMsg_LayoutDumpCompleted,
                    std::string /* completed/stitched layout dump */)

IPC_MESSAGE_ROUTED1(BlinkTestMsg_ReplyBluetoothManualChooserEvents,
                    std::vector<std::string> /* events */)

// Asks the browser process to perform a layout dump spanning all the
// (potentially cross-process) frames.  This goes through multiple
// WebTestControl.DumpFrameLayout calls and ends with sending of
// BlinkTestMsg_LayoutDumpCompleted.
IPC_MESSAGE_ROUTED0(BlinkTestHostMsg_InitiateLayoutDump)

IPC_MESSAGE_ROUTED0(BlinkTestHostMsg_ResetDone)

// WebTestDelegate related.
IPC_MESSAGE_ROUTED1(BlinkTestHostMsg_OverridePreferences,
                    content::WebPreferences /* preferences */)
IPC_MESSAGE_ROUTED1(BlinkTestHostMsg_PrintMessage, std::string /* message */)
IPC_MESSAGE_ROUTED1(BlinkTestHostMsg_PrintMessageToStderr,
                    std::string /* message */)
IPC_MESSAGE_ROUTED1(BlinkTestHostMsg_NavigateSecondaryWindow, GURL /* url */)
IPC_MESSAGE_ROUTED1(BlinkTestHostMsg_GoToOffset, int /* offset */)
IPC_MESSAGE_ROUTED0(BlinkTestHostMsg_Reload)
IPC_MESSAGE_ROUTED2(BlinkTestHostMsg_LoadURLForFrame,
                    GURL /* url */,
                    std::string /* frame_name */)
IPC_MESSAGE_ROUTED0(BlinkTestHostMsg_CloseRemainingWindows)

IPC_MESSAGE_ROUTED1(BlinkTestHostMsg_SetBluetoothManualChooser,
                    bool /* enable */)
IPC_MESSAGE_ROUTED0(BlinkTestHostMsg_GetBluetoothManualChooserEvents)
IPC_MESSAGE_ROUTED2(BlinkTestHostMsg_SendBluetoothManualChooserEvent,
                    std::string /* event */,
                    std::string /* argument */)
IPC_MESSAGE_ROUTED1(BlinkTestHostMsg_SetPopupBlockingEnabled,
                    bool /* block_popups */)

#endif  // CONTENT_SHELL_COMMON_WEB_TEST_BLINK_TEST_MESSAGES_H_
