// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/application_status_listener.h"
#include "base/base_switches.h"
#include "base/test/scoped_feature_list.h"
#include "components/viz/common/features.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_stream_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/gpu_browsertest_helpers.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"

namespace content {

namespace {

enum class CompositorImplMode {
  kNormal,
  kViz,
  kVizSkDDL,
};

class CompositorImplBrowserTest
    : public testing::WithParamInterface<CompositorImplMode>,
      public ContentBrowserTest {
 public:
  CompositorImplBrowserTest() {}

  void SetUp() override {
    switch (GetParam()) {
      case CompositorImplMode::kNormal:
        break;
      case CompositorImplMode::kViz:
        scoped_feature_list_.InitAndEnableFeature(
            features::kVizDisplayCompositor);
        break;
      case CompositorImplMode::kVizSkDDL:
        scoped_feature_list_.InitWithFeatures(
            {features::kVizDisplayCompositor,
             features::kUseSkiaDeferredDisplayList, features::kUseSkiaRenderer},
            {});
        break;
    }

    ContentBrowserTest::SetUp();
  }

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
    https_server.ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(https_server.Start());
    GURL http_url(embedded_test_server()->GetURL("/title1.html"));
    ASSERT_TRUE(NavigateToURL(shell(), http_url));
  }

  ui::WindowAndroid* window() const {
    return web_contents()->GetTopLevelNativeWindow();
  }

  CompositorImpl* compositor_impl() const {
    return static_cast<CompositorImpl*>(window()->GetCompositor());
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderWidgetHostViewAndroid* render_widget_host_view_android() const {
    return static_cast<RenderWidgetHostViewAndroid*>(
        web_contents()->GetRenderWidgetHostView());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(CompositorImplBrowserTest);
};

INSTANTIATE_TEST_CASE_P(P,
                        CompositorImplBrowserTest,
                        ::testing::Values(CompositorImplMode::kNormal,
                                          CompositorImplMode::kViz,
                                          CompositorImplMode::kVizSkDDL));

class CompositorImplLowEndBrowserTest : public CompositorImplBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableLowEndDeviceMode);
    command_line->AppendSwitch(switches::kInProcessGPU);
    content::ContentBrowserTest::SetUpCommandLine(command_line);
  }
};

// Viz on android is not yet compatible with in-process GPU. Only run in
// kNormal mode.
// TODO(ericrk): Make this work everywhere. https://crbug.com/851643
INSTANTIATE_TEST_CASE_P(P,
                        CompositorImplLowEndBrowserTest,
                        ::testing::Values(CompositorImplMode::kNormal));

// RunLoop implementation that calls glFlush() every second until it observes
// OnContextLost().
class ContextLostRunLoop : public viz::ContextLostObserver {
 public:
  ContextLostRunLoop(viz::ContextProvider* context_provider)
      : context_provider_(context_provider) {
    context_provider_->AddObserver(this);
  }
  ~ContextLostRunLoop() override { context_provider_->RemoveObserver(this); }

  void RunUntilContextLost() {
    CheckForContextLoss();
    run_loop_.Run();
  }

  void CheckForContextLoss() {
    if (did_lose_context_) {
      run_loop_.Quit();
      return;
    }
    context_provider_->ContextGL()->Flush();
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ContextLostRunLoop::CheckForContextLoss,
                       base::Unretained(this)),
        base::TimeDelta::FromSeconds(1));
  }

 private:
  // viz::LostContextProvider:
  void OnContextLost() override { did_lose_context_ = true; }

  viz::ContextProvider* const context_provider_;
  bool did_lose_context_ = false;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ContextLostRunLoop);
};

// RunLoop implementation that runs until it observes a compositor frame.
class CompositorFrameRunLoop : public ui::WindowAndroidObserver {
 public:
  CompositorFrameRunLoop(ui::WindowAndroid* window) : window_(window) {
    window_->AddObserver(this);
  }
  ~CompositorFrameRunLoop() override { window_->RemoveObserver(this); }

  void RunUntilFrame() { run_loop_.Run(); }

 private:
  // ui::WindowAndroidObserver:
  void OnCompositingDidCommit() override { run_loop_.Quit(); }
  void OnRootWindowVisibilityChanged(bool visible) override {}
  void OnAttachCompositor() override {}
  void OnDetachCompositor() override {}
  void OnActivityStopped() override {}
  void OnActivityStarted() override {}

  ui::WindowAndroid* const window_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(CompositorFrameRunLoop);
};

IN_PROC_BROWSER_TEST_P(CompositorImplLowEndBrowserTest,
                       CompositorImplDropsResourcesOnBackground) {
  // This test makes invalid assumptions when surface synchronization is
  // enabled. The compositor lock is obsolete, and inspecting frames
  // from the CompositorImpl does not guarantee renderer CompositorFrames
  // are ready.
  if (features::IsSurfaceSynchronizationEnabled())
    return;

  auto* rwhva = render_widget_host_view_android();
  auto* compositor = compositor_impl();
  auto context = GpuBrowsertestCreateContext(
      GpuBrowsertestEstablishGpuChannelSyncRunLoop());
  context->BindToCurrentThread();

  CompositorFrameRunLoop(window()).RunUntilFrame();
  EXPECT_TRUE(rwhva->HasValidFrame());

  ContextLostRunLoop run_loop(context.get());
  compositor->SetVisibleForTesting(false);
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  rwhva->OnRootWindowVisibilityChanged(false);
  rwhva->Hide();

  // Ensure that context is eventually dropped and at that point we do not have
  // a valid frame.
  run_loop.RunUntilContextLost();
  EXPECT_FALSE(rwhva->HasValidFrame());

  // Become visible again:
  compositor->SetVisibleForTesting(true);
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
  rwhva->Show();
  rwhva->OnRootWindowVisibilityChanged(true);

  // We should have taken the compositor lock on resume.
  EXPECT_TRUE(compositor->IsLockedForTesting());
  EXPECT_FALSE(rwhva->HasValidFrame());

  // The compositor should eventually be unlocked and produce a frame.
  CompositorFrameRunLoop(window()).RunUntilFrame();
  EXPECT_FALSE(compositor->IsLockedForTesting());
  EXPECT_TRUE(rwhva->HasValidFrame());
}

// RunLoop implementation that runs until it observes a swap with size.
class CompositorSwapRunLoop {
 public:
  CompositorSwapRunLoop(CompositorImpl* compositor) : compositor_(compositor) {
    compositor_->SetSwapCompletedWithSizeCallbackForTesting(base::BindRepeating(
        &CompositorSwapRunLoop::DidSwap, base::Unretained(this)));
  }
  ~CompositorSwapRunLoop() {
    compositor_->SetSwapCompletedWithSizeCallbackForTesting(base::DoNothing());
  }

  void RunUntilSwap() { run_loop_.Run(); }

 private:
  void DidSwap(const gfx::Size& pixel_size) {
    EXPECT_FALSE(pixel_size.IsEmpty());
    run_loop_.Quit();
  }

  CompositorImpl* compositor_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(CompositorSwapRunLoop);
};

IN_PROC_BROWSER_TEST_P(CompositorImplBrowserTest,
                       CompositorImplReceivesSwapCallbacks) {
  CompositorSwapRunLoop(compositor_impl()).RunUntilSwap();
}

}  // namespace
}  // namespace content
