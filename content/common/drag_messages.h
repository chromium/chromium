// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_DRAG_MESSAGES_H_
#define CONTENT_COMMON_DRAG_MESSAGES_H_

// IPC messages for drag and drop.

#include <vector>

#include "content/public/common/drop_data.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "ui/gfx/geometry/point_f.h"

#define IPC_MESSAGE_START DragMsgStart

// Messages sent from the browser to the renderer.

IPC_MESSAGE_ROUTED5(DragMsg_TargetDragEnter,
                    std::vector<content::DropData::Metadata> /* drop_data */,
                    gfx::PointF /* client_pt */,
                    gfx::PointF /* screen_pt */,
                    blink::DragOperationsMask /* ops_allowed */,
                    int /* key_modifiers */)

// Messages sent from the renderer to the browser.

// The page wants to update the mouse cursor during a drag & drop operation.
// |is_drop_target| is true if the mouse is over a valid drop target.
IPC_MESSAGE_ROUTED1(DragHostMsg_UpdateDragCursor,
                    blink::DragOperation /* drag_operation */)

#endif  // CONTENT_COMMON_DRAG_MESSAGES_H_
