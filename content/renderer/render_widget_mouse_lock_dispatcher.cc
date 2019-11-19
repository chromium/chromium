// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget_mouse_lock_dispatcher.h"

#include "content/common/widget_messages.h"
#include "content/renderer/render_view_impl.h"
#include "ipc/ipc_message.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_user_gesture_indicator.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"

namespace content {

RenderWidgetMouseLockDispatcher::RenderWidgetMouseLockDispatcher(
    RenderWidget* render_widget)
    : render_widget_(render_widget) {}

RenderWidgetMouseLockDispatcher::~RenderWidgetMouseLockDispatcher() {}

void RenderWidgetMouseLockDispatcher::SendLockMouseRequest(
    blink::WebLocalFrame* requester_frame,
    bool request_unadjusted_movement) {
  bool user_gesture =
      blink::WebUserGestureIndicator::IsProcessingUserGesture(requester_frame);
  render_widget_->Send(
      new WidgetHostMsg_LockMouse(render_widget_->routing_id(), user_gesture,
                                  false, request_unadjusted_movement));
}

void RenderWidgetMouseLockDispatcher::SendUnlockMouseRequest() {
  render_widget_->Send(
      new WidgetHostMsg_UnlockMouse(render_widget_->routing_id()));
}

bool RenderWidgetMouseLockDispatcher::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidgetMouseLockDispatcher, message)
    IPC_MESSAGE_HANDLER(WidgetMsg_LockMouse_ACK, OnLockMouseACK)
    IPC_MESSAGE_FORWARD(WidgetMsg_MouseLockLost,
                        static_cast<MouseLockDispatcher*>(this),
                        MouseLockDispatcher::OnMouseLockLost)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderWidgetMouseLockDispatcher::OnLockMouseACK(bool succeeded) {
  // Notify the base class.
  MouseLockDispatcher::OnLockMouseACK(succeeded);

  // Mouse Lock removes the system cursor and provides all mouse motion as
  // .movementX/Y values on events all sent to a fixed target. This requires
  // content to specifically request the mode to be entered.
  // Mouse Capture is implicitly given for the duration of a drag event, and
  // sends all mouse events to the initial target of the drag.
  // If Lock is entered it supercedes any in progress Capture.
  if (succeeded && render_widget_->GetWebWidget())
    render_widget_->GetWebWidget()->MouseCaptureLost();
}

}  // namespace content
