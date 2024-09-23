// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/application_status_listener.h"
#include "base/android/build_info.h"
#include "base/base_switches.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_stream_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/gpu_browsertest_helpers.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#include "url/gurl.h"

namespace content {

namespace {

class CompositorImplBrowserTest : public ContentBrowserTest {
 public:
  CompositorImplBrowserTest() {}

  CompositorImplBrowserTest(const CompositorImplBrowserTest&) = delete;
  CompositorImplBrowserTest& operator=(const CompositorImplBrowserTest&) =
      delete;

  virtual std::string GetTestUrl() { return "/title1.html"; }

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
    https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    ASSERT_TRUE(https_server.Start());
    GURL http_url(embedded_test_server()->GetURL(GetTestUrl()));
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
};

class CompositorImplLowEndBrowserTest : public CompositorImplBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableLowEndDeviceMode);
    command_line->AppendSwitch(switches::kInProcessGPU);
    content::ContentBrowserTest::SetUpCommandLine(command_line);
  }
};

// RunLoop implementation that calls glFlush() every second until it observes
// OnContextLost().
class ContextLostRunLoop : public viz::ContextLostObserver {
 public:
  explicit ContextLostRunLoop(viz::RasterContextProvider* context_provider)
      : context_provider_(context_provider) {
    context_provider_->AddObserver(this);
  }

  ContextLostRunLoop(const ContextLostRunLoop&) = delete;
  ContextLostRunLoop& operator=(const ContextLostRunLoop&) = delete;

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
    context_provider_->RasterInterface()->Flush();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ContextLostRunLoop::CheckForContextLoss,
                       base::Unretained(this)),
        base::Seconds(1));
  }

 private:
  // viz::LostContextProvider:
  void OnContextLost() override { did_lose_context_ = true; }

  const raw_ptr<viz::RasterContextProvider> context_provider_;
  bool did_lose_context_ = false;
  base::RunLoop run_loop_;
};

// RunLoop implementation that runs until it observes a swap with size.
class CompositorSwapRunLoop {
 public:
  CompositorSwapRunLoop(CompositorImpl* compositor) : compositor_(compositor) {
    static_cast<Compositor*>(compositor_)
        ->SetDidSwapBuffersCallbackEnabled(true);
    compositor_->SetSwapCompletedWithSizeCallbackForTesting(base::BindRepeating(
        &CompositorSwapRunLoop::DidSwap, base::Unretained(this)));
  }

  CompositorSwapRunLoop(const CompositorSwapRunLoop&) = delete;
  CompositorSwapRunLoop& operator=(const CompositorSwapRunLoop&) = delete;

  ~CompositorSwapRunLoop() {
    compositor_->SetSwapCompletedWithSizeCallbackForTesting(base::DoNothing());
  }

  void RunUntilSwap() { run_loop_.Run(); }

 private:
  void DidSwap(const gfx::Size& pixel_size) {
    EXPECT_FALSE(pixel_size.IsEmpty());
    run_loop_.Quit();
  }

  raw_ptr<CompositorImpl> compositor_;
  base::RunLoop run_loop_;
};

IN_PROC_BROWSER_TEST_F(CompositorImplLowEndBrowserTest,
                       CompositorImplDropsResourcesOnBackground) {
  auto* rwhva = render_widget_host_view_android();
  auto* compositor = compositor_impl();
  auto context = GpuBrowsertestCreateContext(
      GpuBrowsertestEstablishGpuChannelSyncRunLoop());
  context->BindToCurrentSequence();

  // Run until we've swapped once. At this point we should have a valid frame.
  CompositorSwapRunLoop(compositor_impl()).RunUntilSwap();
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

  // Wait for a swap after becoming visible.
  CompositorSwapRunLoop(compositor_impl()).RunUntilSwap();
  EXPECT_TRUE(rwhva->HasValidFrame());
}

IN_PROC_BROWSER_TEST_F(CompositorImplBrowserTest,
                       CompositorImplReceivesSwapCallbacks) {
  CompositorSwapRunLoop(compositor_impl()).RunUntilSwap();
}

// This test waits for a presentation feedback token to arrive from the GPU. If
// this test is timing out then it demonstrates a bug.
IN_PROC_BROWSER_TEST_F(CompositorImplBrowserTest,
                       CompositorImplReceivesPresentationTimeCallbacks) {
  // Presentation feedback occurs after the GPU has presented content to the
  // display. This is later than the buffers swap.
  base::RunLoop loop;
  // The callback will cancel the loop used to wait.
  static_cast<content::Compositor*>(compositor_impl())
      ->RequestSuccessfulPresentationTimeForNextFrame(base::BindOnce(
          [](base::OnceClosure quit, const viz::FrameTimingDetails& details) {
            std::move(quit).Run();
          },
          loop.QuitClosure()));
  loop.Run();
}

class CompositorImplBrowserTestRefreshRate
    : public CompositorImplBrowserTest,
      public ui::WindowAndroid::TestHooks {
 public:
  std::string GetTestUrl() override { return "/media/tulip2.webm"; }

  // WindowAndroid::TestHooks impl.
  std::vector<float> GetSupportedRates() override {
    return {120.f, 90.f, 60.f};
  }
  void SetPreferredRate(float refresh_rate) override {
    if (fabs(refresh_rate - expected_refresh_rate_) < 2.f)
      run_loop_->Quit();
  }
  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
  }

  float expected_refresh_rate_ = 0.f;
  std::unique_ptr<base::RunLoop> run_loop_;
};

IN_PROC_BROWSER_TEST_F(CompositorImplBrowserTestRefreshRate, VideoPreference) {
  if (gfx::SurfaceControl::SupportsSetFrameRate()) {
    // If SurfaceControl allows specifying the frame rate for each Surface, it's
    // done within the GPU process instead of sending the frame preference back
    // to the browser (and so our TestHooks are not consulted).
    GTEST_SKIP();
  }

  window()->SetTestHooks(this);
  expected_refresh_rate_ = 60.f;
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
  window()->SetTestHooks(nullptr);
}

}  // namespace
}  // namespace content
