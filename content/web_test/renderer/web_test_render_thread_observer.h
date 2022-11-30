// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_WEB_TEST_RENDER_THREAD_OBSERVER_H_
#define CONTENT_WEB_TEST_RENDERER_WEB_TEST_RENDER_THREAD_OBSERVER_H_

#include <memory>

#include "content/public/renderer/render_thread_observer.h"
#include "content/web_test/common/web_test.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content {
class TestRunner;

class WebTestRenderThreadObserver : public RenderThreadObserver,
                                    public mojom::WebTestRenderThread {
 public:
  static WebTestRenderThreadObserver* GetInstance();

  WebTestRenderThreadObserver();

  WebTestRenderThreadObserver(const WebTestRenderThreadObserver&) = delete;
  WebTestRenderThreadObserver& operator=(const WebTestRenderThreadObserver&) =
      delete;

  ~WebTestRenderThreadObserver() override;

  TestRunner* test_runner() const { return test_runner_.get(); }

  // content::RenderThreadObserver:
  void RegisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;
  void UnregisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;

 private:
  // mojom::WebTestRenderThread implementation.
  void SetupRendererProcessForNonTestWindow() override;
  void ReplicateWebTestRuntimeFlagsChanges(
      base::Value::Dict changed_layout_test_runtime_flags) override;
  void TestFinishedFromSecondaryRenderer() override;
  void ResetRendererAfterWebTest() override;
  void ProcessWorkItem(mojom::WorkItemPtr work_item) override;
  void ReplicateWorkQueueStates(base::Value::Dict work_queue_states) override;

  // Helper to bind this class as the mojom::WebTestRenderThread.
  void OnWebTestRenderThreadAssociatedRequest(
      mojo::PendingAssociatedReceiver<mojom::WebTestRenderThread> receiver);

  std::unique_ptr<TestRunner> test_runner_;

  mojo::AssociatedReceiver<mojom::WebTestRenderThread> receiver_{this};
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_WEB_TEST_RENDER_THREAD_OBSERVER_H_
