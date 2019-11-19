// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_UNFREEZABLE_FRAME_MESSAGES_H_
#define CONTENT_COMMON_UNFREEZABLE_FRAME_MESSAGES_H_

#include "ipc/ipc_message_macros.h"

// IPC messages for frames which should be executed and not be frozen even when
// the frame is frozen.
// Currently most IPC messages to the renderer are executed on freezable
// per-frame task runners, but messages in this class will be handled as an
// exception and will be posted on an unfreezable task runner and will be
// guaranteed to run regardless of the frame's status.
// These messages are primarily intended to support bfcache functionality.

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START UnfreezableFrameMsgStart

// Swap this RenderFrame out so the frame can navigate to a document rendered by
// a different process.
IPC_MESSAGE_ROUTED3(UnfreezableFrameMsg_SwapOut,
                    int /* proxy_routing_id */,
                    bool /* is_loading */,
                    content::FrameReplicationState /* replication_state */)

// Instructs the renderer to delete the RenderFrame.
IPC_MESSAGE_ROUTED1(UnfreezableFrameMsg_Delete, content::FrameDeleteIntention)

// Requests the corresponding RenderFrameProxy to be deleted and removed from
// the frame tree.
IPC_MESSAGE_ROUTED0(UnfreezableFrameMsg_DeleteProxy)

#endif  // CONTENT_COMMON_UNFREEZABLE_FRAME_MESSAGES_H_
