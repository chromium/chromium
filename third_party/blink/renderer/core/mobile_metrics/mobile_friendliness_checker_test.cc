// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"
#include "third_party/blink/public/common/mobile_metrics/mobile_friendliness.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/mobile_metrics/mobile_metrics_test_helpers.h"
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
    settings->SetViewportEnabled(true);
    settings->SetViewportMetaEnabled(true);
  }

  MobileFriendliness CalculateMetricsForHTMLString(const std::string& html) {
    mobile_metrics_test_helpers::TestWebFrameClient web_frame_client;
    {
      // This scope is required to make sure that ~WebViewHelper() is called
      // before the return value of this function is determined. Because
      // MobileFriendlinessChecker::NotifyDocumentDestroying is called in
      // destruction sequence of WebView.
      frame_test_helpers::WebViewHelper helper;
      helper.Initialize(&web_frame_client, nullptr, ConfigureAndroidSettings);
      helper.GetWebView()->EnableAutoResizeForTesting(gfx::Size(480, 800),
                                                      gfx::Size(480, 800));
      frame_test_helpers::LoadHTMLString(
          helper.GetWebView()->MainFrameImpl(), html,
          url_test_helpers::ToKURL("about:blank"));
      LocalFrameView& frame_view =
          *helper.GetWebView()->MainFrameImpl()->GetFrameView();
      frame_view.UpdateLifecycleToPrePaintClean(DocumentUpdateReason::kTest);
    }
    return web_frame_client.GetMobileFriendliness();
  }

  MobileFriendliness CalculateMetricsForFile(const std::string& path) {
    mobile_metrics_test_helpers::TestWebFrameClient web_frame_client;
    {
      // This scope is required to make sure that ~WebViewHelper() is called
      // before the return value of this function is determined. Because
      // MobileFriendlinessChecker::NotifyDocumentDestroying is called in
      // destruction sequence of WebView.
      frame_test_helpers::WebViewHelper helper;
      helper.Initialize(&web_frame_client, nullptr, ConfigureAndroidSettings);
      helper.GetWebView()->EnableAutoResizeForTesting(gfx::Size(480, 800),
                                                      gfx::Size(480, 800));
      url_test_helpers::RegisterMockedURLLoadFromBase(
          WebString::FromUTF8(kBaseUrl), blink::test::CoreTestDataPath(),
          WebString::FromUTF8(path));
      frame_test_helpers::LoadFrame(helper.GetWebView()->MainFrameImpl(),
                                    kBaseUrl + path);
      LocalFrameView& frame_view =
          *helper.GetWebView()->MainFrameImpl()->GetFrameView();
      frame_view.UpdateLifecycleToPrePaintClean(DocumentUpdateReason::kTest);
    }
    return web_frame_client.GetMobileFriendliness();
  }
};

TEST_F(MobileFriendlinessCheckerTest, NoViewportSetting) {
  MobileFriendliness expected_mf;
  expected_mf.small_text_ratio = 100;
  MobileFriendliness actual_mf =
      CalculateMetricsForHTMLString("<body>bar</body>");
  EXPECT_EQ(expected_mf, actual_mf);
}

TEST_F(MobileFriendlinessCheckerTest, DeviceWidth) {
  MobileFriendliness expected_mf;
  expected_mf.viewport_device_width = true;
  MobileFriendliness actual_mf =
      CalculateMetricsForFile("viewport/viewport-1.html");
  EXPECT_EQ(expected_mf, actual_mf);
}

TEST_F(MobileFriendlinessCheckerTest, HardcodedViewport) {
  MobileFriendliness expected_mf;
  expected_mf.viewport_hardcoded_width = 200;
  MobileFriendliness actual_mf =
      CalculateMetricsForFile("viewport/viewport-30.html");
  EXPECT_EQ(expected_mf, actual_mf);
}

TEST_F(MobileFriendlinessCheckerTest, DeviceWidthWithInitialScale05) {
  // Specifying initial-scale=0.5 is usually not the best choice for most web
  // pages. But we cannot determine that such page must not be mobile friendly.
  MobileFriendliness expected_mf;
  expected_mf.viewport_device_width = true;
  expected_mf.viewport_initial_scale = 0.5;
  MobileFriendliness actual_mf =
      CalculateMetricsForFile("viewport/viewport-34.html");
  EXPECT_EQ(expected_mf, actual_mf);
}

TEST_F(MobileFriendlinessCheckerTest, UserZoom) {
  MobileFriendliness expected_mf;
  expected_mf.viewport_device_width = true;
  expected_mf.viewport_initial_scale = 2.0;
  expected_mf.allow_user_zoom = false;
  expected_mf.small_text_ratio = 100;
  MobileFriendliness actual_mf = CalculateMetricsForFile(
      "viewport-initial-scale-and-user-scalable-no.html");
  EXPECT_EQ(expected_mf, actual_mf);
}

TEST_F(MobileFriendlinessCheckerTest, NoText) {
  MobileFriendliness expected_mf;
  expected_mf.small_text_ratio = 0;
  MobileFriendliness actual_mf =
      CalculateMetricsForHTMLString(R"(<body></body>)");
  EXPECT_EQ(expected_mf, actual_mf);
}

TEST_F(MobileFriendlinessCheckerTest, NoSmallFonts) {
  MobileFriendliness expected_mf;
  expected_mf.small_text_ratio = 0;
  MobileFriendliness actual_mf = CalculateMetricsForHTMLString(R"(
<div style="font-size: 12px">
  This is legible font size example.
</div>
)");
  EXPECT_EQ(expected_mf, actual_mf);
}

TEST_F(MobileFriendlinessCheckerTest, OnlySmallFonts) {
  MobileFriendliness expected_mf;
  expected_mf.small_text_ratio = 100;
  MobileFriendliness actual_mf = CalculateMetricsForHTMLString(R"(
<div style="font-size:7px">
  Small font text.
</div>
)");
  EXPECT_EQ(expected_mf, actual_mf);
}

TEST_F(MobileFriendlinessCheckerTest, MostlySmallFont) {
  MobileFriendliness actual_mf = CalculateMetricsForHTMLString(R"(
<div style="font-size:12px">
  legible text.
  <div style="font-size:8px">
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
  </div>
</div>
)");
  EXPECT_LT(actual_mf.small_text_ratio, 100);
  EXPECT_GT(actual_mf.small_text_ratio, 80);
}

TEST_F(MobileFriendlinessCheckerTest, MostlySmallInSpan) {
  MobileFriendliness actual_mf = CalculateMetricsForHTMLString(R"(
<div style="font-size: 12px">
  x
  <span style="font-size:11px">
    This is the majority part of the document.
  </span>
  y
</div>
)");
  EXPECT_LT(actual_mf.small_text_ratio, 100);
  EXPECT_GT(actual_mf.small_text_ratio, 80);
}

TEST_F(MobileFriendlinessCheckerTest, MultipleDivs) {
  MobileFriendliness actual_mf = CalculateMetricsForHTMLString(R"(
<div style="font-size: 12px">
  x
  <div style="font-size:11px">
    middle of div
    <div style="font-size:1px">
      inner of div
    </div>
  </div>
  y
</div>
)");
  EXPECT_LT(actual_mf.small_text_ratio, 100);
  EXPECT_GT(actual_mf.small_text_ratio, 80);
}

TEST_F(MobileFriendlinessCheckerTest, DontCountInvisibleSmallFontArea) {
  MobileFriendliness expected_mf;
  MobileFriendliness actual_mf = CalculateMetricsForHTMLString(R"(
<html>
  <body>
    <div style="font-size: 12px">
      x
      <div style="font-size:4px;display:none;">
        this is an invisible string.
      </div>
    </div>
  </body>
</html>
)");
  EXPECT_EQ(expected_mf, actual_mf);
}

TEST_F(MobileFriendlinessCheckerTest, ScaleZoomedLegibleFont) {
  MobileFriendliness expected_mf;
  expected_mf.viewport_device_width = true;
  expected_mf.viewport_initial_scale = 10;
  expected_mf.small_text_ratio = 0;
  MobileFriendliness actual_mf = CalculateMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=10">
  </head>
  <body style="font-size: 5px">
    Legible text in 50px.
  </body>
</html>
)");
  EXPECT_EQ(expected_mf, actual_mf);
}

TEST_F(MobileFriendlinessCheckerTest, ViewportZoomedOutIllegibleFont) {
  MobileFriendliness expected_mf;
  expected_mf.viewport_hardcoded_width = 480;
  expected_mf.viewport_initial_scale = 0.5;
  expected_mf.small_text_ratio = 100;
  MobileFriendliness actual_mf = CalculateMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=0.5">
  </head>
  <body style="font-size: 22px; width: 960px">
    Illegible text in 11px.
  </body>
</html>
)");
  EXPECT_EQ(expected_mf, actual_mf);
}

TEST_F(MobileFriendlinessCheckerTest, TooWideViewportWidthIllegibleFont) {
  MobileFriendliness expected_mf;
  expected_mf.viewport_hardcoded_width = 960;
  expected_mf.small_text_ratio = 100;
  MobileFriendliness actual_mf = CalculateMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=960">
  </head>
  <body style="font-size: 12px">
    Illegible text in 6px.
  </body>
</html>
)");
  EXPECT_EQ(expected_mf, actual_mf);
}

TEST_F(MobileFriendlinessCheckerTest, CSSZoomedIllegibleFont) {
  MobileFriendliness expected_mf;
  expected_mf.small_text_ratio = 100;
  MobileFriendliness actual_mf = CalculateMetricsForHTMLString(R"(
<html>
  <body style="font-size: 12px; zoom:50%">
    Illegible text in 6px.
  </body>
</html>
)");
  EXPECT_EQ(expected_mf, actual_mf);
}

}  // namespace blink
