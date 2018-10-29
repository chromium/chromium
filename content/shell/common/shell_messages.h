// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_COMMON_SHELL_MESSAGES_H_
#define CONTENT_SHELL_COMMON_SHELL_MESSAGES_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/page_state.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

#define IPC_MESSAGE_START ShellMsgStart

// Tells the renderer to reset all test runners.
IPC_MESSAGE_ROUTED0(ShellViewMsg_Reset)

// Tells the main window that a secondary renderer in a different process asked
// to finish the test.
IPC_MESSAGE_ROUTED0(ShellViewMsg_TestFinishedInSecondaryRenderer)

// Notifies BlinkTestRunner that the layout dump has completed
// (and that it can proceed with finishing up the test).
IPC_MESSAGE_ROUTED1(ShellViewMsg_LayoutDumpCompleted,
                    std::string /* completed/stitched layout dump */)

// Asks the browser process to perform a layout dump spanning all the
// (potentially cross-process) frames.  This goes through multiple
// LayoutTestControl.DumpFrameLayout calls and ends with sending of
// ShellViewMsg_LayoutDumpCompleted.
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_InitiateLayoutDump)

IPC_MESSAGE_ROUTED0(ShellViewHostMsg_ResetDone)

// WebTestDelegate related.
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_OverridePreferences,
                    content::WebPreferences /* preferences */)
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_PrintMessage,
                    std::string /* message */)
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_PrintMessageToStderr,
                    std::string /* message */)
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_NavigateSecondaryWindow, GURL /* url */)
IPC_MESSAGE_ROUTED2(ShellViewHostMsg_ShowDevTools,
                    std::string /* settings */,
                    std::string /* frontend_url */)
IPC_MESSAGE_ROUTED2(ShellViewHostMsg_EvaluateInDevTools,
                    int /* call_id */,
                    std::string /* script */)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_CloseDevTools)
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_GoToOffset,
                    int /* offset */)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_Reload)
IPC_MESSAGE_ROUTED2(ShellViewHostMsg_LoadURLForFrame,
                    GURL /* url */,
                    std::string /* frame_name */)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_CloseRemainingWindows)

IPC_MESSAGE_ROUTED1(ShellViewHostMsg_SetBluetoothManualChooser,
                    bool /* enable */)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_GetBluetoothManualChooserEvents)
IPC_MESSAGE_ROUTED1(ShellViewMsg_ReplyBluetoothManualChooserEvents,
                    std::vector<std::string> /* events */)
IPC_MESSAGE_ROUTED2(ShellViewHostMsg_SendBluetoothManualChooserEvent,
                    std::string /* event */,
                    std::string /* argument */)
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_SetPopupBlockingEnabled,
                    bool /* block_popups */)

#endif  // CONTENT_SHELL_COMMON_SHELL_MESSAGES_H_
