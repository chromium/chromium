// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_render_input_router.h"

namespace content {

MockRenderInputRouter::MockRenderInputRouter(
    InputRouterImplClient* host,
    InputDispositionHandler* handler,
    std::unique_ptr<FlingSchedulerBase> fling_scheduler,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : RenderInputRouter(host,
                        handler,
                        std::move(fling_scheduler),
                        std::move(task_runner)) {
  mock_widget_input_handler_ = std::make_unique<MockWidgetInputHandler>();
}

MockRenderInputRouter::~MockRenderInputRouter() = default;

void MockRenderInputRouter::SetupForInputRouterTest() {
  input_router_ = std::make_unique<MockInputRouter>(this);
}

MockWidgetInputHandler::MessageVector
MockRenderInputRouter::GetAndResetDispatchedMessages() {
  return mock_widget_input_handler_->GetAndResetDispatchedMessages();
}

blink::mojom::WidgetInputHandler*
MockRenderInputRouter::GetWidgetInputHandler() {
  return mock_widget_input_handler_.get();
}

}  // namespace content
