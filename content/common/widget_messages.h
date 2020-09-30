// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_WIDGET_MESSAGES_H_
#define CONTENT_COMMON_WIDGET_MESSAGES_H_

// IPC messages for controlling painting and input events.

#include "base/optional.h"
#include "base/time/time.h"
#include "cc/input/touch_action.h"
#include "content/common/common_param_traits_macros.h"
#include "content/common/content_param_traits.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/blink/public/common/widget/visual_properties.h"
#include "third_party/blink/public/mojom/page/record_content_to_visible_time_request.mojom-forward.h"
#include "third_party/blink/public/platform/viewport_intersection_state.h"
#include "ui/base/ime/text_input_action.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START WidgetMsgStart

IPC_STRUCT_TRAITS_BEGIN(blink::WebSize)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
IPC_STRUCT_TRAITS_END()

//
// Browser -> Renderer Messages.
//

// Tells the render widget to close.
// Expects a Close_ACK message when finished.
IPC_MESSAGE_ROUTED0(WidgetMsg_Close)

// Activate/deactivate the RenderWidget (i.e., set its controls' tint
// accordingly, etc.).
IPC_MESSAGE_ROUTED1(WidgetMsg_SetActive, bool /* active */)

// Reply to WidgetHostMsg_RequestSetBounds, WidgetHostMsg_ShowWidget, and
// FrameHostMsg_ShowCreatedWindow, to inform the renderer that the browser has
// processed the bounds-setting.  The browser may have ignored the new bounds,
// but it finished processing.  This is used because the renderer keeps a
// temporary cache of the widget position while these asynchronous operations
// are in progress.
IPC_MESSAGE_ROUTED0(WidgetMsg_SetBounds_ACK)

// Sent by a parent frame to notify its child about the state of the child's
// intersection with the parent's viewport, primarily for use by the
// IntersectionObserver API. Also see FrameHostMsg_UpdateViewportIntersection.
IPC_MESSAGE_ROUTED1(WidgetMsg_SetViewportIntersection,
                    blink::ViewportIntersectionState /* intersection_state */)

//
// Renderer -> Browser Messages.
//

// Sent by the renderer process to request that the browser close the widget.
// This corresponds to the window.close() API, and the browser may ignore
// this message.  Otherwise, the browser will generate a WidgetMsg_Close
// message to close the widget.
IPC_MESSAGE_ROUTED0(WidgetHostMsg_Close)

// Sent by the renderer process to request that the browser change the bounds of
// the widget. This corresponds to the window.resizeTo() and window.moveTo()
// APIs, and the browser may ignore this message.
IPC_MESSAGE_ROUTED1(WidgetHostMsg_RequestSetBounds, gfx::Rect /* bounds */)

// Indicates that the render widget has been closed in response to a
// Close message.
IPC_MESSAGE_CONTROL1(WidgetHostMsg_Close_ACK, int /* old_route_id */)

#endif  //  CONTENT_COMMON_WIDGET_MESSAGES_H_
