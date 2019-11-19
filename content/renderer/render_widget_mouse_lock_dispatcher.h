// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_MOUSE_LOCK_DISPATCHER_H_
#define CONTENT_RENDERER_RENDER_WIDGET_MOUSE_LOCK_DISPATCHER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/renderer/mouse_lock_dispatcher.h"

namespace IPC {
class Message;
}

namespace content {

class RenderWidget;

// RenderWidgetMouseLockDispatcher is owned by RenderWidget.
class RenderWidgetMouseLockDispatcher : public MouseLockDispatcher {
 public:
  explicit RenderWidgetMouseLockDispatcher(RenderWidget* render_widget);
  ~RenderWidgetMouseLockDispatcher() override;

  bool OnMessageReceived(const IPC::Message& message);

 private:
  // MouseLockDispatcher implementation.
  void SendLockMouseRequest(blink::WebLocalFrame* requester_frame,
                            bool request_unadjusted_movement) override;
  void SendUnlockMouseRequest() override;

  void OnLockMouseACK(bool succeeded);

  RenderWidget* render_widget_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetMouseLockDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_MOUSE_LOCK_DISPATCHER_H_
