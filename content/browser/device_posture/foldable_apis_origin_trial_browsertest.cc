// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

constexpr char kBaseDataDir[] = "content/test/data/device_posture";

class TestRenderWidgetHostObserver : public RenderWidgetHostObserver {
 public:
  explicit TestRenderWidgetHostObserver(RenderWidgetHost* widget_host)
      : widget_host_(widget_host) {
    widget_host_->AddObserver(this);
  }

  ~TestRenderWidgetHostObserver() override {
    widget_host_->RemoveObserver(this);
  }

  // RenderWidgetHostObserver:
  void RenderWidgetHostDidUpdateVisualProperties(
      RenderWidgetHost* widget_host) override {
    run_loop_.Quit();
  }

  void WaitForVisualPropertiesUpdate() { run_loop_.Run(); }

 private:
  raw_ptr<RenderWidgetHost> widget_host_ = nullptr;
  base::RunLoop run_loop_;
};

class FoldableAPIsOriginTrialBrowserTest : public ContentBrowserTest {
 public:
  ~FoldableAPIsOriginTrialBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // We need to use URLLoaderInterceptor (rather than a EmbeddedTestServer),
    // because origin trial token is associated with a fixed origin, whereas
    // EmbeddedTestServer serves content on a random port.
    interceptor_ = URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
        kBaseDataDir, GURL("https://example.test/"));
  }

  RenderWidgetHostViewBase* view() {
    return static_cast<RenderWidgetHostViewBase*>(
        shell()->web_contents()->GetRenderWidgetHostView());
  }

  WebContentsImpl* web_contents_impl() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  void SetUpFoldableState() {
    const int kDisplayFeatureLength = 10;
    DisplayFeature emulated_display_feature{
        DisplayFeature::Orientation::kVertical,
        /* offset */ view()->GetVisibleViewportSize().width() / 2 -
            kDisplayFeatureLength / 2,
        /* mask_length */ kDisplayFeatureLength};
    view()->SetDisplayFeatureForTesting(&emulated_display_feature);
    FrameTreeNode* root = web_contents_impl()->GetPrimaryFrameTree().root();
    RenderWidgetHostImpl* root_widget =
        root->current_frame_host()->GetRenderWidgetHost();
    root_widget->SynchronizeVisualProperties();
    // We need to wait that visual properties are updated before we test the
    // CSS APIs of Viewport Segments.
    while (root_widget->visual_properties_ack_pending_for_testing()) {
      TestRenderWidgetHostObserver(root_widget).WaitForVisualPropertiesUpdate();
    }
  }

  void TearDownOnMainThread() override {
    interceptor_.reset();
    ContentBrowserTest::TearDownOnMainThread();
    web_contents_impl()
        ->GetDevicePostureProvider()
        ->DisableDevicePostureOverrideForEmulation();
    view()->SetDisplayFeatureForTesting(nullptr);
  }

  bool HasDevicePostureApi() {
    return EvalJs(shell(), "'devicePosture' in navigator").ExtractBool();
  }

  bool HasDevicePostureCSSApi() {
    return EvalJs(shell(),
                  "window.matchMedia('(device-posture: continuous)').matches")
               .ExtractBool() ||
           EvalJs(shell(),
                  "window.matchMedia('(device-posture: folded)').matches")
               .ExtractBool();
  }

  bool HasViewportSegmentsApi() {
    return EvalJs(
               shell(),
               "window.viewport != undefined && 'segments' in window.viewport")
        .ExtractBool();
  }

  bool HasViewportSegmentsCSSApi() {
    return EvalJs(
               shell(),
               "window.matchMedia('(horizontal-viewport-segments: 2)').matches")
        .ExtractBool();
  }

  bool HasViewportSegmentsEnvVariablesCSSApi() {
    return EvalJs(shell(),
                  "getComputedStyle(document.getElementById('content')).width "
                  "!= '0px'")
        .ExtractBool();
  }

 protected:
  const GURL kValidTokenUrl{"https://example.test/valid_token.html"};
  const GURL kNoTokenUrl{"https://example.test/no_token.html"};

  std::unique_ptr<content::URLLoaderInterceptor> interceptor_;
};

IN_PROC_BROWSER_TEST_F(FoldableAPIsOriginTrialBrowserTest,
                       ValidOriginTrialToken) {
  ASSERT_TRUE(NavigateToURL(shell(), kValidTokenUrl));
  SetUpFoldableState();
  EXPECT_TRUE(HasDevicePostureApi());
  EXPECT_TRUE(HasDevicePostureCSSApi());
  EXPECT_TRUE(HasViewportSegmentsApi());
  EXPECT_TRUE(HasViewportSegmentsCSSApi());
  EXPECT_TRUE(HasViewportSegmentsEnvVariablesCSSApi());
}

IN_PROC_BROWSER_TEST_F(FoldableAPIsOriginTrialBrowserTest, NoOriginTrialToken) {
  ASSERT_TRUE(NavigateToURL(shell(), kNoTokenUrl));
  SetUpFoldableState();
  EXPECT_FALSE(HasDevicePostureApi());
  EXPECT_FALSE(HasDevicePostureCSSApi());
  EXPECT_FALSE(HasViewportSegmentsApi());
  EXPECT_FALSE(HasViewportSegmentsCSSApi());
  EXPECT_FALSE(HasViewportSegmentsEnvVariablesCSSApi());
}

class FoldableAPIsOriginTrialKillSwitchBrowserTest
    : public FoldableAPIsOriginTrialBrowserTest {
 public:
  FoldableAPIsOriginTrialKillSwitchBrowserTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        {},
        {blink::features::kDevicePosture, blink::features::kViewportSegments});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FoldableAPIsOriginTrialKillSwitchBrowserTest,
                       ValidOriginTrialToken) {
  ASSERT_TRUE(NavigateToURL(shell(), kValidTokenUrl));
  SetUpFoldableState();
  EXPECT_FALSE(HasDevicePostureApi());
  EXPECT_FALSE(HasDevicePostureCSSApi());
  EXPECT_FALSE(HasViewportSegmentsApi());
  EXPECT_FALSE(HasViewportSegmentsCSSApi());
  EXPECT_FALSE(HasViewportSegmentsEnvVariablesCSSApi());
}

IN_PROC_BROWSER_TEST_F(FoldableAPIsOriginTrialKillSwitchBrowserTest,
                       NoOriginTrialToken) {
  ASSERT_TRUE(NavigateToURL(shell(), kNoTokenUrl));
  SetUpFoldableState();
  EXPECT_FALSE(HasDevicePostureApi());
  EXPECT_FALSE(HasDevicePostureCSSApi());
  EXPECT_FALSE(HasViewportSegmentsApi());
  EXPECT_FALSE(HasViewportSegmentsCSSApi());
  EXPECT_FALSE(HasViewportSegmentsEnvVariablesCSSApi());
}

}  // namespace

}  // namespace content
