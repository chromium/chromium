// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PAGE_MESSAGES_H_
#define CONTENT_COMMON_PAGE_MESSAGES_H_

#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"

// IPC messages for page-level actions.
// TODO(https://crbug.com/775827): Convert to mojo.

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START PageMsgStart

// Messages sent from the browser to the renderer.

// Sent when the history for this page is altered from another process. The
// history list should be reset to |history_length| length, and the offset
// should be reset to |history_offset|.
IPC_MESSAGE_ROUTED2(PageMsg_SetHistoryOffsetAndLength,
                    int /* history_offset */,
                    int /* history_length */)

// -----------------------------------------------------------------------------
// Messages sent from the renderer to the browser.

// Adding a new message? Stick to the sort order above: first platform
// independent PageMsg, then ifdefs for platform specific PageMsg, then platform
// independent PageHostMsg, then ifdefs for platform specific PageHostMsg.

#endif  // CONTENT_COMMON_PAGE_MESSAGES_H_
