// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"
#include "third_party/blink/public/common/mobile_metrics/mobile_friendliness.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

static constexpr char kBaseUrl[] = "http://www.test.com/";
class MobileFriendlinessCheckerTest : public testing::Test {
 public:
  ~MobileFriendlinessCheckerTest() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  static void ConfigureAndroidSettings(WebSettings* settings) {
    settings->SetJavaScriptEnabled(true);
    settings->SetPreferCompositingToLCDTextEnabled(true);
    settings->SetViewportEnabled(true);
    settings->SetViewportMetaEnabled(true);
    settings->SetShrinksViewportContentToFit(true);
    settings->SetMainFrameResizesAreOrientationChanges(true);
  }

  void SetUp() override {
    helper_.Initialize(nullptr, nullptr, nullptr, ConfigureAndroidSettings);
  }

  WebViewImpl* WebView() const { return helper_.GetWebView(); }

  const MobileFriendliness& CalculateMobileFriendlinessFromFile(
      const std::string& path) {
    RegisterMockedHttpURLLoad(path);
    NavigateTo(kBaseUrl + path);
    LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();
    frame_view.UpdateLifecycleToPrePaintClean(DocumentUpdateReason::kTest);
    return frame_view.GetMobileFriendlinessChecker().GetMobileFriendliness();
  }

 private:
  void NavigateTo(const std::string& url) {
    frame_test_helpers::LoadFrame(WebView()->MainFrameImpl(), url);
  }

  void RegisterMockedHttpURLLoad(const std::string& file_name) {
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(kBaseUrl), blink::test::CoreTestDataPath(),
        WebString::FromUTF8(file_name));
  }

  frame_test_helpers::WebViewHelper helper_;
};

TEST_F(MobileFriendlinessCheckerTest, NoViewportSetting) {
  MobileFriendliness expected_mf;
  MobileFriendliness actual_mf =
      CalculateMobileFriendlinessFromFile("bar.html");
  EXPECT_EQ(expected_mf, actual_mf);
}

TEST_F(MobileFriendlinessCheckerTest, DeviceWidth) {
  MobileFriendliness expected_mf;
  expected_mf.viewport_device_width = true;
  MobileFriendliness actual_mf =
      CalculateMobileFriendlinessFromFile("viewport/viewport-1.html");
  EXPECT_EQ(expected_mf, actual_mf);
}

TEST_F(MobileFriendlinessCheckerTest, HardcodedViewport) {
  MobileFriendliness expected_mf;
  expected_mf.viewport_hardcoded_width = 200;
  MobileFriendliness actual_mf =
      CalculateMobileFriendlinessFromFile("viewport/viewport-30.html");
  EXPECT_EQ(expected_mf, actual_mf);
}

TEST_F(MobileFriendlinessCheckerTest, DeviceWidthWithInitialScale05) {
  // Specifying initial-scale=0.5 is usually not the best choice for most web
  // pages. But we cannot determine that such page must not be mobile friendly.
  MobileFriendliness expected_mf;
  expected_mf.viewport_device_width = true;
  expected_mf.viewport_initial_scale = 0.5;
  MobileFriendliness actual_mf =
      CalculateMobileFriendlinessFromFile("viewport/viewport-34.html");
  EXPECT_EQ(expected_mf, actual_mf);
}

TEST_F(MobileFriendlinessCheckerTest, UserZoom) {
  MobileFriendliness expected_mf;
  expected_mf.viewport_device_width = true;
  expected_mf.viewport_initial_scale = 2.0;
  expected_mf.allow_user_zoom = false;
  MobileFriendliness actual_mf = CalculateMobileFriendlinessFromFile(
      "viewport-initial-scale-and-user-scalable-no.html");
  EXPECT_EQ(expected_mf, actual_mf);
}

}  // namespace blink
