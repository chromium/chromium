// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/application_status_listener.h"
#include "base/android/build_info.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
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
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"

namespace content {

namespace {

enum class CompositorImplMode {
  kNormal,
  kSkiaRenderer,
};

class CompositorImplBrowserTest
    : public testing::WithParamInterface<CompositorImplMode>,
      public ContentBrowserTest {
 public:
  CompositorImplBrowserTest() {}

  void SetUp() override {
    std::vector<base::Feature> features;

    switch (GetParam()) {
      case CompositorImplMode::kNormal:
        break;
      case CompositorImplMode::kSkiaRenderer:
        features = std::vector<base::Feature>({features::kUseSkiaRenderer});
        break;
    }

    AppendFeatures(&features);
    scoped_feature_list_.InitWithFeatures(features, {});

    ContentBrowserTest::SetUp();
  }

  virtual std::string GetTestUrl() { return "/title1.html"; }
  virtual void AppendFeatures(std::vector<base::Feature>* features) {}

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

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(CompositorImplBrowserTest);
};

INSTANTIATE_TEST_SUITE_P(P,
                         CompositorImplBrowserTest,
                         ::testing::Values(CompositorImplMode::kNormal,
                                           CompositorImplMode::kSkiaRenderer));

class CompositorImplLowEndBrowserTest : public CompositorImplBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableLowEndDeviceMode);
    command_line->AppendSwitch(switches::kInProcessGPU);
    content::ContentBrowserTest::SetUpCommandLine(command_line);
  }
};

INSTANTIATE_TEST_SUITE_P(P,
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

IN_PROC_BROWSER_TEST_P(CompositorImplLowEndBrowserTest,
                       CompositorImplDropsResourcesOnBackground) {
  auto* rwhva = render_widget_host_view_android();
  auto* compositor = compositor_impl();
  auto context = GpuBrowsertestCreateContext(
      GpuBrowsertestEstablishGpuChannelSyncRunLoop());
  context->BindToCurrentThread();

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

IN_PROC_BROWSER_TEST_P(CompositorImplBrowserTest,
                       CompositorImplReceivesSwapCallbacks) {
  // OOP-R is required for this test to succeed with SkDDL, but is disabled on
  // Android L and lower.
  if (GetParam() == CompositorImplMode::kSkiaRenderer &&
      base::android::BuildInfo::GetInstance()->sdk_int() <
          base::android::SDK_VERSION_MARSHMALLOW) {
    return;
  }
  CompositorSwapRunLoop(compositor_impl()).RunUntilSwap();
}

class CompositorImplBrowserTestRefreshRate
    : public CompositorImplBrowserTest,
      public ui::WindowAndroid::TestHooks {
 public:
  std::string GetTestUrl() override { return "/media/tulip2.webm"; }
  void AppendFeatures(std::vector<base::Feature>* features) override {
    features->push_back(media::kUseSurfaceLayerForVideo);
  }

  // WindowAndroid::TestHooks impl.
  std::vector<float> GetSupportedRates() override {
    return {120.f, 90.f, 60.f};
  }
  void SetPreferredRate(float refresh_rate) override {
    if (fabs(refresh_rate - expected_refresh_rate_) < 2.f)
      run_loop_->Quit();
  }

  float expected_refresh_rate_ = 0.f;
  std::unique_ptr<base::RunLoop> run_loop_;
};

IN_PROC_BROWSER_TEST_P(CompositorImplBrowserTestRefreshRate, VideoPreference) {
  window()->SetTestHooks(this);
  expected_refresh_rate_ = 60.f;
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
  window()->SetTestHooks(nullptr);
}

INSTANTIATE_TEST_SUITE_P(P,
                         CompositorImplBrowserTestRefreshRate,
                         ::testing::Values(CompositorImplMode::kNormal));

}  // namespace
}  // namespace content
