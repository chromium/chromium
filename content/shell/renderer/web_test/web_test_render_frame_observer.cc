// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/web_test_render_frame_observer.h"

#include <string>

#include "base/bind.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/shell/renderer/web_test/blink_test_runner.h"
#include "content/shell/renderer/web_test/web_test_render_thread_observer.h"
#include "content/shell/test_runner/web_test_interfaces.h"
#include "content/shell/test_runner/web_test_runner.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

WebTestRenderFrameObserver::WebTestRenderFrameObserver(
    RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
  test_runner::WebTestRunner* test_runner =
      WebTestRenderThreadObserver::GetInstance()
          ->test_interfaces()
          ->TestRunner();
  render_frame->GetWebFrame()->SetContentSettingsClient(
      test_runner->GetWebContentSettings());
  render_frame->GetWebFrame()->SetTextCheckClient(
      test_runner->GetWebTextCheckClient());
  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(&WebTestRenderFrameObserver::BindReceiver,
                          base::Unretained(this)));
}

WebTestRenderFrameObserver::~WebTestRenderFrameObserver() = default;

void WebTestRenderFrameObserver::BindReceiver(
    mojo::PendingAssociatedReceiver<mojom::WebTestControl> receiver) {
  receiver_.Bind(std::move(receiver),
                 blink::scheduler::GetSingleThreadTaskRunnerForTesting());
}

void WebTestRenderFrameObserver::ReadyToCommitNavigation(
    blink::WebDocumentLoader* document_loader) {
  if (!render_frame()->IsMainFrame())
    return;
  focus_on_next_commit_ = true;
}

void WebTestRenderFrameObserver::DidCommitProvisionalLoad(
    bool is_same_document_navigation,
    ui::PageTransition transition) {
  if (!render_frame()->IsMainFrame())
    return;
  if (focus_on_next_commit_) {
    focus_on_next_commit_ = false;
    render_frame()->GetRenderView()->GetWebView()->SetFocusedFrame(
        render_frame()->GetWebFrame());
  }
  BlinkTestRunner::Get(render_frame()->GetRenderView())
      ->DidCommitNavigationInMainFrame();
}

void WebTestRenderFrameObserver::DidFailProvisionalLoad() {
  if (!render_frame()->IsMainFrame())
    return;
  focus_on_next_commit_ = false;
}

void WebTestRenderFrameObserver::OnDestruct() {
  delete this;
}

void WebTestRenderFrameObserver::CaptureDump(CaptureDumpCallback callback) {
  BlinkTestRunner::Get(render_frame()->GetRenderView())
      ->CaptureDump(std::move(callback));
}

void WebTestRenderFrameObserver::CompositeWithRaster(
    CompositeWithRasterCallback callback) {
  // When the TestFinished() occurred, if the browser is capturing pixels, it
  // asks each composited RenderFrame to submit a new frame via here.
  render_frame()->UpdateAllLifecyclePhasesAndCompositeForTesting();
  std::move(callback).Run();
}

void WebTestRenderFrameObserver::DumpFrameLayout(
    DumpFrameLayoutCallback callback) {
  std::string dump = WebTestRenderThreadObserver::GetInstance()
                         ->test_interfaces()
                         ->TestRunner()
                         ->DumpLayout(render_frame()->GetWebFrame());
  std::move(callback).Run(dump);
}

void WebTestRenderFrameObserver::ReplicateTestConfiguration(
    mojom::ShellTestConfigurationPtr config) {
  BlinkTestRunner::Get(render_frame()->GetRenderView())
      ->OnReplicateTestConfiguration(std::move(config));
}

void WebTestRenderFrameObserver::SetTestConfiguration(
    mojom::ShellTestConfigurationPtr config) {
  BlinkTestRunner::Get(render_frame()->GetRenderView())
      ->OnSetTestConfiguration(std::move(config));
}

void WebTestRenderFrameObserver::SetupSecondaryRenderer() {
  BlinkTestRunner::Get(render_frame()->GetRenderView())
      ->OnSetupSecondaryRenderer();
}

}  // namespace content
