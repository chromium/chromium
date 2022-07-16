// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"

#include "base/time/time_override.h"
#include "third_party/blink/public/common/mobile_metrics/mobile_friendliness.h"
#include "third_party/blink/public/mojom/mobile_metrics/mobile_friendliness.mojom-shared.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/mobile_metrics/mobile_metrics_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

using mobile_metrics_test_helpers::MobileFriendlinessTree;

static constexpr char kBaseUrl[] = "http://www.test.com/";
static constexpr int kDeviceWidth = 480;
static constexpr int kDeviceHeight = 800;
static constexpr float kMinimumZoom = 0.25f;
static constexpr float kMaximumZoom = 5;

class MobileFriendlinessCheckerTest : public testing::Test {
 public:
  ~MobileFriendlinessCheckerTest() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  static void ConfigureAndroidSettings(WebSettings* settings) {
    settings->SetViewportEnabled(true);
    settings->SetViewportMetaEnabled(true);
  }

  MobileFriendlinessTree CalculateMetricsForHTMLString(
      const std::string& html,
      float device_scale = 1.0,
      int scroll_y_offset = 0) {
    frame_test_helpers::WebViewHelper helper;
    helper.Initialize(nullptr, nullptr, ConfigureAndroidSettings);
    helper.GetWebView()->MainFrameWidget()->SetDeviceScaleFactorForTesting(
        device_scale);
    helper.Resize(gfx::Size(kDeviceWidth, kDeviceHeight));
    helper.GetWebView()->GetPage()->SetDefaultPageScaleLimits(kMinimumZoom,
                                                              kMaximumZoom);
    frame_test_helpers::LoadHTMLString(helper.GetWebView()->MainFrameImpl(),
                                       html,
                                       url_test_helpers::ToKURL("about:blank"));
    return MobileFriendlinessTree::GetMobileFriendlinessTree(
        helper.GetWebView()->MainFrameImpl()->GetFrameView(), scroll_y_offset);
  }

  MobileFriendlinessTree CalculateMetricsForFile(const std::string& path,
                                                 float device_scale = 1.0,
                                                 int scroll_y_offset = 0) {
    frame_test_helpers::WebViewHelper helper;
    helper.Initialize(nullptr, nullptr, ConfigureAndroidSettings);
    helper.GetWebView()->MainFrameWidget()->SetDeviceScaleFactorForTesting(
        device_scale);
    helper.Resize(gfx::Size(kDeviceWidth, kDeviceHeight));
    helper.GetWebView()->GetPage()->SetDefaultPageScaleLimits(kMinimumZoom,
                                                              kMaximumZoom);
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(kBaseUrl), blink::test::CoreTestDataPath(),
        WebString::FromUTF8(path));
    frame_test_helpers::LoadFrame(helper.GetWebView()->MainFrameImpl(),
                                  kBaseUrl + path);
    return MobileFriendlinessTree::GetMobileFriendlinessTree(
        helper.GetWebView()->MainFrameImpl()->GetFrameView(), scroll_y_offset);
  }

  MobileFriendliness CalculateMainFrameMetricsForHTMLString(
      const std::string& html,
      float device_scale = 1.0,
      int scroll_y_offset = 0) {
    return CalculateMetricsForHTMLString(html, device_scale, scroll_y_offset)
        .mf;
  }

  MobileFriendliness CalculateMainFrameMetricsForFile(const std::string& path,
                                                      float device_scale = 1.0,
                                                      int scroll_y_offset = 0) {
    return CalculateMetricsForFile(path, device_scale, scroll_y_offset).mf;
  }

  void SetUseZoomForDSF(bool use_zoom_for_dsf) {
    platform_->SetUseZoomForDSF(use_zoom_for_dsf);
  }

 private:
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
};

class ClockFixedMobileFriendlinessCheckerTest
    : public MobileFriendlinessCheckerTest {
 public:
  void SetUp() override {
    clock_override_ = std::make_unique<base::subtle::ScopedTimeClockOverrides>(
        []() {
          // Returns fixed mock time to avoid BadTapTargetRatio hits
          // timeout.
          static base::Time start = base::subtle::TimeNowIgnoringOverride();
          return start;
        },
        nullptr, nullptr);
  }

 protected:
  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> clock_override_;
};

TEST_F(MobileFriendlinessCheckerTest, NoViewportSetting) {
  MobileFriendliness actual_mf =
      CalculateMainFrameMetricsForHTMLString("<body>bar</body>");
  EXPECT_EQ(actual_mf.viewport_device_width, false);
  EXPECT_EQ(actual_mf.allow_user_zoom, true);
  EXPECT_EQ(actual_mf.small_text_ratio, 100);
}

TEST_F(MobileFriendlinessCheckerTest, DeviceWidth) {
  MobileFriendliness actual_mf =
      CalculateMainFrameMetricsForFile("viewport/viewport-1.html");
  EXPECT_EQ(actual_mf.viewport_device_width, true);
  EXPECT_EQ(actual_mf.allow_user_zoom, true);
}

TEST_F(MobileFriendlinessCheckerTest, HardcodedViewport) {
  MobileFriendliness actual_mf =
      CalculateMainFrameMetricsForFile("viewport/viewport-30.html");
  EXPECT_EQ(actual_mf.viewport_device_width, false);
  EXPECT_EQ(actual_mf.allow_user_zoom, true);
  EXPECT_EQ(actual_mf.viewport_hardcoded_width, 200);
}

TEST_F(MobileFriendlinessCheckerTest, HardcodedViewportWithDeviceScale3) {
  SetUseZoomForDSF(true);
  MobileFriendliness actual_mf =
      CalculateMainFrameMetricsForFile("viewport/viewport-30.html",
                                       /*device_scale=*/3.0);
  EXPECT_EQ(actual_mf.viewport_device_width, false);
  EXPECT_EQ(actual_mf.allow_user_zoom, true);
  EXPECT_EQ(actual_mf.viewport_hardcoded_width, 200);
}

TEST_F(MobileFriendlinessCheckerTest, DeviceWidthWithInitialScale05) {
  // Specifying initial-scale=0.5 is usually not the best choice for most web
  // pages. But we cannot determine that such page must not be mobile friendly.
  MobileFriendliness actual_mf =
      CalculateMainFrameMetricsForFile("viewport/viewport-34.html");
  EXPECT_EQ(actual_mf.viewport_device_width, true);
  EXPECT_EQ(actual_mf.allow_user_zoom, true);
  EXPECT_EQ(actual_mf.viewport_initial_scale_x10, 5);
}

TEST_F(MobileFriendlinessCheckerTest, AllowUserScalableWithSmallMaxZoom) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
    <head>
      <meta name="viewport" content="user-scalable=yes, maximum-scale=1.1">
    </head>
  )");
  EXPECT_EQ(actual_mf.allow_user_zoom, false);
}

TEST_F(MobileFriendlinessCheckerTest, AllowUserScalableWithLargeMaxZoom) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
    <head>
      <meta name="viewport" content="user-scalable=yes, maximum-scale=2.0">
    </head>
  )");
  EXPECT_EQ(actual_mf.allow_user_zoom, true);
}

TEST_F(MobileFriendlinessCheckerTest,
       AllowUserScalableWithLargeMaxZoomAndLargeInitialScale) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
    <head>
      <meta name="viewport" content="user-scalable=yes, maximum-scale=2.0, initial-scale=1.9">
    </head>
  )");
  EXPECT_EQ(actual_mf.allow_user_zoom, false);
}

TEST_F(MobileFriendlinessCheckerTest, UserZoom) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForFile(
      "viewport-initial-scale-and-user-scalable-no.html");
  EXPECT_EQ(actual_mf.viewport_device_width, true);
  EXPECT_EQ(actual_mf.viewport_initial_scale_x10, 20);
  EXPECT_EQ(actual_mf.allow_user_zoom, false);
  EXPECT_EQ(actual_mf.small_text_ratio, 100);
}

TEST_F(ClockFixedMobileFriendlinessCheckerTest, NoText) {
  MobileFriendliness actual_mf =
      CalculateMainFrameMetricsForHTMLString(R"(<body></body>)");
  EXPECT_EQ(actual_mf.viewport_device_width, false);
  EXPECT_EQ(actual_mf.allow_user_zoom, true);
  EXPECT_EQ(actual_mf.small_text_ratio, 0);
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 0);
}

TEST_F(MobileFriendlinessCheckerTest, NoSmallFonts) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body>
    <div style="font-size: 9px">
      This is legible font size example.
    </div>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.viewport_device_width, false);
  EXPECT_EQ(actual_mf.allow_user_zoom, true);
  EXPECT_EQ(actual_mf.small_text_ratio, 0);
}

TEST_F(MobileFriendlinessCheckerTest, NoSmallFontsWithDeviceScaleFactor) {
  MobileFriendliness actual_mf =
      CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body>
    <div style="font-size:9px">
      This is legible font size example.
    </div>
  </body>
</html>
)",
                                             /*device_scale=*/2.0);
  EXPECT_EQ(actual_mf.viewport_device_width, false);
  EXPECT_EQ(actual_mf.allow_user_zoom, true);
  EXPECT_EQ(actual_mf.small_text_ratio, 0);
}

TEST_F(MobileFriendlinessCheckerTest, OnlySmallFonts) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body>
    <div style="font-size:7px">
      Small font text.
    </div>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.viewport_device_width, false);
  EXPECT_EQ(actual_mf.allow_user_zoom, true);
  EXPECT_EQ(actual_mf.small_text_ratio, 100);
}

TEST_F(MobileFriendlinessCheckerTest, OnlySmallFontsWithDeviceScaleFactor) {
  MobileFriendliness actual_mf =
      CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body>
    <div style="font-size:8px">
      Small font text.
    </div>
  </body>
</html>
)",
                                             /*device_scale=*/2.0);
  EXPECT_EQ(actual_mf.viewport_device_width, false);
  EXPECT_EQ(actual_mf.allow_user_zoom, true);
  EXPECT_EQ(actual_mf.small_text_ratio, 100);
}

TEST_F(MobileFriendlinessCheckerTest, MostlySmallFont) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body>
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
  </body>
<html>
)");
  EXPECT_LT(actual_mf.small_text_ratio, 100);
  EXPECT_GT(actual_mf.small_text_ratio, 80);
}

TEST_F(MobileFriendlinessCheckerTest, MostlySmallInSpan) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<div style="font-size: 12px">
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  x
  <span style="font-size:8px">
    This is the majority part of the document.
  </span>
  y
</div>
)");
  EXPECT_LT(actual_mf.small_text_ratio, 100);
  EXPECT_GT(actual_mf.small_text_ratio, 80);
}

TEST_F(MobileFriendlinessCheckerTest, MultipleDivs) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body>
    <div style="font-size: 12px">
      x
      <div style="font-size:8px">
        middle of div
        <div style="font-size:1px">
          inner of div
        </div>
      </div>
      y
    </div>
  </body>
</html>
)");
  EXPECT_LT(actual_mf.small_text_ratio, 100);
  EXPECT_GT(actual_mf.small_text_ratio, 68);
}

TEST_F(MobileFriendlinessCheckerTest, DontCountInvisibleSmallFontArea) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
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
  EXPECT_EQ(actual_mf.small_text_ratio, 0);
  EXPECT_EQ(actual_mf.viewport_device_width, false);
  EXPECT_EQ(actual_mf.allow_user_zoom, true);
}

TEST_F(MobileFriendlinessCheckerTest, ScaleZoomedLegibleFont) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=10">
  </head>
  <body style="font-size: 5px">
    Legible text in 50px.
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.viewport_device_width, true);
  EXPECT_EQ(actual_mf.viewport_initial_scale_x10, 100);
  EXPECT_EQ(actual_mf.allow_user_zoom, true);
  EXPECT_EQ(actual_mf.small_text_ratio, 0);
}

TEST_F(MobileFriendlinessCheckerTest, ViewportZoomedOutIllegibleFont) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=0.5">
  </head>
  <body style="font-size: 16px; width: 960px">
    Illegible text in 8px.
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.viewport_device_width, false);
  EXPECT_EQ(actual_mf.viewport_hardcoded_width, 480);
  EXPECT_EQ(actual_mf.viewport_initial_scale_x10, 5);
  EXPECT_EQ(actual_mf.allow_user_zoom, true);
  EXPECT_EQ(actual_mf.small_text_ratio, 100);
}

TEST_F(MobileFriendlinessCheckerTest, TooWideViewportWidthIllegibleFont) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=960">
  </head>
  <body style="font-size: 12px">
    Illegible text in 6px.
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.viewport_device_width, false);
  EXPECT_EQ(actual_mf.allow_user_zoom, true);
  EXPECT_EQ(actual_mf.viewport_hardcoded_width, 960);
  EXPECT_EQ(actual_mf.small_text_ratio, 100);
}

TEST_F(MobileFriendlinessCheckerTest, CSSZoomedIllegibleFont) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <body style="font-size: 12px; zoom:50%">
    Illegible text in 6px.
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.viewport_device_width, false);
  EXPECT_EQ(actual_mf.allow_user_zoom, true);
  EXPECT_EQ(actual_mf.small_text_ratio, 100);
}

TEST_F(MobileFriendlinessCheckerTest, OnlySmallFontsClipped) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <body style="font-size: 6px; clip: rect(0 0 0 0); position: absolute">
    Small font text.
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.small_text_ratio, 0);
}

TEST_F(MobileFriendlinessCheckerTest, NormalTextAndWideImage) {
  // Wide image forces Chrome to zoom out.
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <body>
    <img style="width:3000px; height:50px">
    <p style="font-size: 12pt">Normal font text.</p>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.small_text_ratio, 100);
  EXPECT_GE(actual_mf.text_content_outside_viewport_percentage, 50);
}

TEST_F(MobileFriendlinessCheckerTest,
       NormalTextAndWideImageWithDeviceWidthViewport) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=device-width">
  </head>
  <body>
    <img style="width:3000px; height:50px">
    <p style="font-size: 12pt">Normal font text.</p>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.small_text_ratio, 100);
  EXPECT_GE(actual_mf.text_content_outside_viewport_percentage, 50);
}

TEST_F(MobileFriendlinessCheckerTest, NormalTextAndWideImageWithInitialScale) {
  // initial-scale=1.0 prevents the automatic zoom out.
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body>
    <img style="width:3000px; height:50px">
    <p style="font-size: 9pt">Normal font text.</p>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.small_text_ratio, 0);
  EXPECT_GE(actual_mf.text_content_outside_viewport_percentage, 100);
}

TEST_F(MobileFriendlinessCheckerTest,
       NormalTextAndWideImageWithInitialScaleAndDeviceScale) {
  MobileFriendliness actual_mf =
      CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body>
    <img style="width:3000px; height:50px">
    <p style="font-size: 6pt">Illegible font text.</p>
  </body>
</html>
)",
                                             /*device_scale=*/2.0);
  EXPECT_EQ(actual_mf.small_text_ratio, 100);
  EXPECT_GE(actual_mf.text_content_outside_viewport_percentage, 100);
}

TEST_F(MobileFriendlinessCheckerTest, TextNarrow) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="initial-scale=.25">
  </head>
  <body>
    <pre>foo foo foo foo foo</pre>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 0);
}

TEST_F(MobileFriendlinessCheckerTest, TextTooWide) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(
      R"(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body>
    <pre>)" +
      std::string(10000, 'a') +
      R"(</pre>
  </body>
</html>
)");
  EXPECT_NE(actual_mf.text_content_outside_viewport_percentage, 0);
}

TEST_F(MobileFriendlinessCheckerTest, TextTooWideAbsolutePositioning) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(
      R"(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body>
    <pre style="position:absolute; left:2000px">a</pre>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 317);
}

TEST_F(MobileFriendlinessCheckerTest, TextTooWideOverflowXHidden) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(
      R"(
<html>
  <body>
    <pre style="overflow-x:hidden">)" +
      std::string(10000, 'a') + R"(</pre>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 0);
}

TEST_F(MobileFriendlinessCheckerTest, TextTooWideHidden) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(
      R"(
<html>
  <body>
    <pre style="overflow:hidden">)" +
      std::string(10000, 'a') +
      R"(</pre>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 0);
}

TEST_F(MobileFriendlinessCheckerTest, TextTooWideHiddenInDiv) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(
      R"(
<html>
  <body>
    <div style="overflow:hidden">
      <pre>)" +
      std::string(10000, 'a') +
      R"(
      </pre>
    </div>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 0);
}

TEST_F(MobileFriendlinessCheckerTest, TextTooWideHiddenInDivDiv) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(
      R"(
<html>
  <body>
    <div style="overflow:hidden">
      <div>
        <pre>)" +
      std::string(10000, 'a') +
      R"(
        </pre>
      <div>
    </div>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 0);
}

TEST_F(MobileFriendlinessCheckerTest, ImageNarrow) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <body>
    <img style="width:200px; height:50px">
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 0);
}

TEST_F(MobileFriendlinessCheckerTest, ImageTooWide) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body>
    <img style="width:2000px; height:50px">
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 319);
}

TEST_F(MobileFriendlinessCheckerTest, ImageTooWideTwoImages) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body style="width:4000px">
    <img style="width:2000px; height:50px">
    <img style="width:2000px; height:50px">
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 735);
}

TEST_F(MobileFriendlinessCheckerTest, ImageTooWideAbsolutePosition) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body>
    <img style="width:100px; height:100px; position:absolute; left:2000px">
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 338);
}

TEST_F(MobileFriendlinessCheckerTest, ImageTooWideDisplayNone) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <body>
    <img style="width:2000px; height:50px; display:none">
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 0);
}

TEST_F(MobileFriendlinessCheckerTest, ScaleTextOutsideViewport) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="minimum-scale=1, initial-scale=3">
  </head>
  <body style="font-size: 76px; width: 480">
    foo foo foo foo foo foo foo foo foo foo
    foo foo foo foo foo foo foo foo foo foo
    foo foo foo foo foo foo foo foo foo foo
    foo foo foo foo foo foo foo foo foo foo
    foo foo foo foo foo foo foo foo foo foo
    foo foo foo foo foo foo foo foo foo foo
    foo foo foo foo foo foo foo foo foo foo
    foo foo foo foo foo foo foo foo foo foo
    foo foo foo foo foo foo foo foo foo foo
    foo foo foo foo foo foo foo foo foo foo
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.viewport_initial_scale_x10, 30);
  EXPECT_GE(actual_mf.text_content_outside_viewport_percentage, 100.0);
}

TEST_F(MobileFriendlinessCheckerTest, ScrollerOutsideViewport) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <style>
      div.scrollmenu {
        background-color: #333;
        overflow: auto;
        white-space: nowrap;
      }
      div.scrollmenu a {
        display: inline-block;
        color: white;
        padding: 14px;
      }
    </style>
    <meta name="viewport" content="width=device-width, initial-scale=1.0 minimum-scale=1.0">
  </head>
  <body style="font-size: 18px">
  <div class="scrollmenu">
    <a href="#1">First text</a>
    <a href="#2">Second text</a>
    <a href="#3">Third text</a>
    <a href="#4">Fourth text</a>
    <a href="#5">Fifth text</a>
    <a href="#6">Sixth text</a>
    <a href="#7">Seventh text</a>
    <a href="#8">Eighth text</a>
    <a href="#9">Ninth text</a>
    <a href="#10">Tenth text</a>
  </div>
  </body>
</html>
)");
  // the viewport
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 0.0);
}

TEST_F(ClockFixedMobileFriendlinessCheckerTest, SingleTapTarget) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
</html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <a onclick="alert('clicked');">
      link
    </a>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 0);
}

TEST_F(ClockFixedMobileFriendlinessCheckerTest, NoBadTapTarget) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <button style="width:30px; height:30px">
      a
    </button>
    <button style="width:30px; height:30px">
      b
    </button>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 0);
}

TEST_F(ClockFixedMobileFriendlinessCheckerTest,
       NoBadTapTargetWithDeviceScaleFactor) {
  MobileFriendliness actual_mf =
      CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <button style="width:30px; height:30px">
      a
    </button>
    <button style="width:30px; height:30px">
      b
    </button>
  </body>
</html>
)",
                                             /*device_scale=*/2.0);
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 0);
}

TEST_F(ClockFixedMobileFriendlinessCheckerTest,
       BadTapTargetWithDeviceScaleFactor) {
  MobileFriendliness actual_mf =
      CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <button style="width:25px; height:25px">
      a
    </button>
    <button style="width:25px; height:25px">
      b
    </button>
  </body>
</html>
)",
                                             /*device_scale=*/4.0);
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 100);
}

TEST_F(ClockFixedMobileFriendlinessCheckerTest, BadTapTargetWithAutoZoomOut) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <body style="font-size: 18px">
    <img style="width:30000px; height:50px">
    <button style="width:30px; height:30px">
      a
    </button>
    <button style="width:30px; height:30px">
      b
    </button>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 100);
}

TEST_F(ClockFixedMobileFriendlinessCheckerTest, TooCloseTapTargetsVertical) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <a href="about:blank">
      <div style="width: 400px;height: 400px; margin: 0px">
        A
      </div>
    </a>
    <a href="about:blank">
      <div style="width: 10px;height: 10px; margin: 0px">
        B
      </div>
    </a>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 50);
}

TEST_F(ClockFixedMobileFriendlinessCheckerTest,
       TooCloseTapTargetsVerticalSamePoint) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <a href="about:blank">
      <div style="width: 400px;height: 400px; margin: 0px">
        A
      </div>
    </a>
    <a href="about:blank">
      <div style="width: 10px;height: 10px; margin: 0px">
        B
      </div>
    </a>
    <a href="about:blank">
      <div style="width: 400px;height: 400px; margin: 0px">
        C
      </div>
    </a>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 33);
}

TEST_F(ClockFixedMobileFriendlinessCheckerTest, TooCloseTapTargetsHorizontal) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <a href="about:blank">
      <div style="width: 300px;height: 300px; margin: 0px; display:inline-block">
        A
      </div>
    </a>
    <a href="about:blank">
      <div style="width: 10px;height: 10px; margin: 0px; display:inline-block">
        B
      </div>
    </a>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 50);
}

TEST_F(ClockFixedMobileFriendlinessCheckerTest,
       TooCloseTapTargetsHorizontalSamePoint) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <a href="about:blank">
      <div style="width: 200px;height: 200px; margin: 0px; display:inline-block">
        A
      </div>
    </a>
    <a href="about:blank">
      <div style="width: 10px;height: 10px; margin: 0px; display:inline-block">
        B
      </div>
    </a>
    <a href="about:blank">
      <div style="width: 200px;height: 200px; margin: 0px; display:inline-block">
        C
      </div>
    </a>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 33);
}

TEST_F(ClockFixedMobileFriendlinessCheckerTest, GridGoodTargets3X3) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <div>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          1-1
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          2-1
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          3-1
        </div>
      </a>
    </div>
    <div>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          1-2
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          2-2
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          3-2
        </div>
      </a>
    </div>
    <div>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          1-3
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          2-3
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          3-3
        </div>
      </a>
    </div>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 0);
}

TEST_F(ClockFixedMobileFriendlinessCheckerTest, GridBadTargets3X3) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <div>
      <a href="about:blank">
        <div style="width: 10px;height: 10px; margin: 10px; display:inline-block">
          1-1
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 10px;height: 10px; margin: 10px; display:inline-block">
          2-1
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 10px;height: 10px; margin: 10px; display:inline-block">
          3-1
        </div>
      </a>
    </div>
    <div>
      <a href="about:blank">
        <div style="width: 10px;height: 10px; margin: 10px; display:inline-block">
          1-2
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 10px;height: 10px; margin: 10px; display:inline-block">
          2-2
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 10px;height: 10px; margin: 10px; display:inline-block">
          3-2
        </div>
      </a>
    </div>
    <div>
      <a href="about:blank">
        <div style="width: 10px;height: 10px; margin: 10px; display:inline-block">
          1-3
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 10px;height: 10px; margin: 10px; display:inline-block">
          2-3
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 10px;height: 10px; margin: 10px; display:inline-block">
          3-3
        </div>
      </a>
    </div>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 100);
}

TEST_F(ClockFixedMobileFriendlinessCheckerTest, FormTapTargets) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <form>
      <input style="height: 400px; margin: 0px"><br>
      <input style="height: 10px; margin: 0px">
    </form>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 50);
}

TEST_F(ClockFixedMobileFriendlinessCheckerTest,
       InvisibleTapTargetWillBeIgnored) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <form>
      <input style="height: 400px; margin: 0px"><br>
      <div style="display:none">
        <input style="height: 10px; margin: 0px">
      </div>
    </form>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 0);
}

TEST_F(ClockFixedMobileFriendlinessCheckerTest,
       BadTapTargetWithPositionAbsolute) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <button style="position:absolute; width:50px; height:50px">
      a
    </button>
    <button style="position:relative; width:50px; height:50px">
      b
    </button>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 100);
}

TEST_F(ClockFixedMobileFriendlinessCheckerTest,
       BadTapTargetBelowFirstOnePager) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <button style="position:absolute; width:50px; height:50px">
      a
    </button>
    <button style="position:relative; width:50px; height:50px">
      b
    </button>
    <!-- below area must be ignored -->
    <div style="margin-top: 800px">
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          have
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          enough
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          spans
        </div>
      </a>
    </div>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 100);
}

TEST_F(ClockFixedMobileFriendlinessCheckerTest,
       BadTapTargetBelowFirstOnePagerWithScroll) {
  auto eval_btt_with_scroll = [&](const int scroll_offset) {
    return CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <button style="position:absolute; width:50px; height:50px">
      a
    </button>
    <button style="position:relative; width:50px; height:50px">
      b
    </button>
    <!-- below area must be ignored -->
    <div style="margin-top: 800px">
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          have
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          enough
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          spans
        </div>
      </a>
    </div>
  </body>
</html>
)",
                                                  1.0 /*=device_scale*/,
                                                  scroll_offset)
        .bad_tap_targets_ratio;
  };

  // BadTapTargetResult must not be affected by scrolling offset.
  EXPECT_EQ(eval_btt_with_scroll(0), 100);
  EXPECT_EQ(eval_btt_with_scroll(400), 100);
  EXPECT_EQ(eval_btt_with_scroll(800), 100);
  EXPECT_EQ(eval_btt_with_scroll(1200), 100);
}

TEST_F(ClockFixedMobileFriendlinessCheckerTest, TapTargetTimeout) {
  clock_override_.reset();
  clock_override_ = std::make_unique<base::subtle::ScopedTimeClockOverrides>(
      []() {
        // Time::Now() progress 1 ms stride for every check to force timeout.
        static base::Time now = base::subtle::TimeNowIgnoringOverride();
        now += base::Milliseconds(1);
        return now;
      },
      nullptr, nullptr);
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <button>
      a
    </button>
    <button>
      b
    </button>
    <button>
      c
    </button>
    <button>
      d
    </button>
    <button>
      e
    </button>
    <button>
      f
    </button>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, -2);
}

TEST_F(ClockFixedMobileFriendlinessCheckerTest, IFrameTest) {
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(kBaseUrl), blink::test::CoreTestDataPath(),
      WebString::FromUTF8("visible_iframe.html"));
  MobileFriendlinessTree actual_mf_tree =
      CalculateMetricsForFile("single_iframe.html");
  const MobileFriendliness& mainframe_mf = actual_mf_tree.mf;
  EXPECT_EQ(mainframe_mf.viewport_device_width, false);
  EXPECT_EQ(mainframe_mf.allow_user_zoom, true);
  EXPECT_EQ(mainframe_mf.bad_tap_targets_ratio, 0);

  EXPECT_EQ(actual_mf_tree.children.size(), 1u);
  const MobileFriendliness& subframe_mf = actual_mf_tree.children[0].mf;
  EXPECT_EQ(subframe_mf.bad_tap_targets_ratio, 0);
}

}  // namespace blink
