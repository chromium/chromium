// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_RENDER_INPUT_ROUTER_H_
#define CONTENT_TEST_MOCK_RENDER_INPUT_ROUTER_H_

#include <stddef.h>
#include <memory>
#include <utility>

#include "content/browser/renderer_host/input/mock_input_router.h"
#include "content/common/input/render_input_router.h"
#include "content/test/mock_widget_input_handler.h"

namespace content {

class MockRenderInputRouter : public RenderInputRouter {
 public:
  using RenderInputRouter::input_router_;

  explicit MockRenderInputRouter(
      InputRouterImplClient* host,
      InputDispositionHandler* handler,
      std::unique_ptr<FlingSchedulerBase> fling_scheduler,
      RenderInputRouterDelegate* delegate,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  MockRenderInputRouter(const MockRenderInputRouter&) = delete;
  MockRenderInputRouter& operator=(const MockRenderInputRouter&) = delete;

  ~MockRenderInputRouter() override;

  // InputRouterImplClient overrides.
  blink::mojom::WidgetInputHandler* GetWidgetInputHandler() override;

  void SetupForInputRouterTest();

  MockWidgetInputHandler::MessageVector GetAndResetDispatchedMessages();

  std::unique_ptr<MockWidgetInputHandler> mock_widget_input_handler_;
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_RENDER_INPUT_ROUTER_H_
