// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_VIEW_MESSAGES_H_
#define CONTENT_COMMON_VIEW_MESSAGES_H_

// IPC messages for page rendering.

#include "ipc/ipc_message_macros.h"


#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START ViewMsgStart

// Messages sent from the browser to the renderer.

// Notification that a move or resize renderer's containing window has
// started.
IPC_MESSAGE_ROUTED0(ViewMsg_MoveOrResizeStarted)

// -----------------------------------------------------------------------------
// Messages sent from the renderer to the browser.

// Adding a new message? Stick to the sort order above: first platform
// independent ViewMsg, then ifdefs for platform specific ViewMsg, then platform
// independent ViewHostMsg, then ifdefs for platform specific ViewHostMsg.

#endif  // CONTENT_COMMON_VIEW_MESSAGES_H_
