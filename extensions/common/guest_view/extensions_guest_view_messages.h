// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for extensions GuestViews.

#ifndef EXTENSIONS_COMMON_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MESSAGES_H_
#define EXTENSIONS_COMMON_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MESSAGES_H_

#include <string>

#include "ipc/ipc_message_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

#define IPC_MESSAGE_START ExtensionsGuestViewMsgStart
// Messages sent from the browser to the renderer.

// The ACK for GuestViewHostMsg_CreateMimeHandlerViewGuest.
IPC_MESSAGE_CONTROL1(ExtensionsGuestViewMsg_CreateMimeHandlerViewGuestACK,
                     int /* element_instance_id */)

// Once a MimeHandlerView guest's JavaScript onload function has been called,
// this IPC is sent to the container to notify it.
IPC_MESSAGE_CONTROL1(ExtensionsGuestViewMsg_MimeHandlerViewGuestOnLoadCompleted,
                     int /* element_instance_id */)

// Notifies the embedder to the destroy the MimeHandlerViewFrameContainer
// associated with |element_instance_id|.
IPC_MESSAGE_CONTROL1(ExtensionsGuestViewMsg_DestroyFrameContainer,
                     int /* element_instance_id */)

// Notifies the embedder that the current guest creation has failed and it
// should retry creating a MimeHandlerViewGuest.
IPC_MESSAGE_CONTROL1(ExtensionsGuestViewMsg_RetryCreatingMimeHandlerViewGuest,
                     int /* element_instance_id */)

// Messages sent from the renderer to the browser.

// Queries whether the RenderView of the provided |routing_id| is allowed to
// inject the script with the provided |script_id|.
IPC_SYNC_MESSAGE_CONTROL2_1(
    ExtensionsGuestViewHostMsg_CanExecuteContentScriptSync,
    int /* routing_id */,
    int /* script_id */,
    bool /* allowed */)

// A renderer sends this message when it wants to resize a guest.
IPC_MESSAGE_CONTROL3(ExtensionsGuestViewHostMsg_ResizeGuest,
                     int /* routing_id */,
                     int /* element_instance_id*/,
                     gfx::Size /* new_size */)

#endif  // EXTENSIONS_COMMON_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MESSAGES_H_
