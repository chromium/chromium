// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/web_test_render_thread_observer.h"

#include <utility>

#include "content/public/common/content_client.h"
#include "content/public/renderer/render_thread.h"
#include "content/web_test/common/web_test_switches.h"
#include "content/web_test/renderer/test_runner.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/blink.h"

namespace content {

namespace {
WebTestRenderThreadObserver* g_instance = nullptr;
}

// static
WebTestRenderThreadObserver* WebTestRenderThreadObserver::GetInstance() {
  return g_instance;
}

WebTestRenderThreadObserver::WebTestRenderThreadObserver() {
  CHECK(!g_instance);
  g_instance = this;
  RenderThread::Get()->AddObserver(this);

  blink::SetWebTestMode(true);

  test_runner_ = std::make_unique<TestRunner>();
}

WebTestRenderThreadObserver::~WebTestRenderThreadObserver() {
  CHECK(g_instance == this);
  g_instance = nullptr;
}

void WebTestRenderThreadObserver::RegisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->AddInterface<mojom::WebTestRenderThread>(
      base::BindRepeating(
          &WebTestRenderThreadObserver::OnWebTestRenderThreadAssociatedRequest,
          base::Unretained(this)));
}

void WebTestRenderThreadObserver::UnregisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->RemoveInterface(mojom::WebTestRenderThread::Name_);
}

void WebTestRenderThreadObserver::OnWebTestRenderThreadAssociatedRequest(
    mojo::PendingAssociatedReceiver<mojom::WebTestRenderThread> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void WebTestRenderThreadObserver::SetupRendererProcessForNonTestWindow() {
  // Allows the window to receive replicated WebTestRuntimeFlags and to
  // control or end the test.
  test_runner_->SetTestIsRunning(true);
}

void WebTestRenderThreadObserver::ReplicateWebTestRuntimeFlagsChanges(
    base::Value::Dict changed_layout_test_runtime_flags) {
  test_runner_->ReplicateWebTestRuntimeFlagsChanges(
      std::move(changed_layout_test_runtime_flags));
}

void WebTestRenderThreadObserver::TestFinishedFromSecondaryRenderer() {
  test_runner_->TestFinishedFromSecondaryRenderer();
}

void WebTestRenderThreadObserver::ResetRendererAfterWebTest() {
  test_runner_->ResetRendererAfterWebTest();
}

void WebTestRenderThreadObserver::ProcessWorkItem(
    mojom::WorkItemPtr work_item) {
  test_runner_->ProcessWorkItem(std::move(work_item));
}

void WebTestRenderThreadObserver::ReplicateWorkQueueStates(
    base::Value::Dict work_queue_states) {
  test_runner_->ReplicateWorkQueueStates(std::move(work_queue_states));
}

}  // namespace content
