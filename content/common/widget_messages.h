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
#include "ui/base/ime/text_input_action.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START WidgetMsgStart

//
// Browser -> Renderer Messages.
//

// Reply to WidgetHostMsg_RequestSetBounds, WidgetHostMsg_ShowWidget, and
// FrameHostMsg_ShowCreatedWindow, to inform the renderer that the browser has
// processed the bounds-setting.  The browser may have ignored the new bounds,
// but it finished processing.  This is used because the renderer keeps a
// temporary cache of the widget position while these asynchronous operations
// are in progress.
IPC_MESSAGE_ROUTED0(WidgetMsg_SetBounds_ACK)

//
// Renderer -> Browser Messages.
//

// Sent by the renderer process to request that the browser change the bounds of
// the widget. This corresponds to the window.resizeTo() and window.moveTo()
// APIs, and the browser may ignore this message.
IPC_MESSAGE_ROUTED1(WidgetHostMsg_RequestSetBounds, gfx::Rect /* bounds */)

#endif  //  CONTENT_COMMON_WIDGET_MESSAGES_H_
