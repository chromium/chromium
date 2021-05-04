// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for extensions GuestViews.

#ifndef EXTENSIONS_COMMON_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MESSAGES_H_
#define EXTENSIONS_COMMON_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MESSAGES_H_

#include <string>

#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

#define IPC_MESSAGE_START ExtensionsGuestViewMsgStart
// Messages sent from the renderer to the browser.

// Queries whether the RenderView of the provided |routing_id| is allowed to
// inject the script with the provided |script_id|.
IPC_SYNC_MESSAGE_CONTROL2_1(
    ExtensionsGuestViewHostMsg_CanExecuteContentScriptSync,
    int /* routing_id */,
    std::string /* script_id */,
    bool /* allowed */)

#endif  // EXTENSIONS_COMMON_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MESSAGES_H_
