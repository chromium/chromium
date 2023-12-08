// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"

#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

static constexpr char kBaseUrl[] = "http://www.test.com/";
static constexpr int kDeviceWidth = 480;
static constexpr int kDeviceHeight = 800;
static constexpr float kMinimumZoom = 0.25f;
static constexpr float kMaximumZoom = 5;

class MobileFriendlinessCheckerTest : public testing::Test {
  static void ConfigureAndroidSettings(WebSettings* settings) {
    settings->SetViewportEnabled(true);
    settings->SetViewportMetaEnabled(true);
  }

  template <typename LoaderCallback>
  ukm::mojom::UkmEntry EvalMobileFriendlinessUKM(const LoaderCallback& load,
                                                 float device_scale) {
    auto helper = std::make_unique<frame_test_helpers::WebViewHelper>();
    helper->Initialize(nullptr, nullptr, ConfigureAndroidSettings);
    helper->GetWebView()->MainFrameWidget()->SetDeviceScaleFactorForTesting(
        device_scale);
    helper->Resize(gfx::Size(kDeviceWidth, kDeviceHeight));
    helper->GetWebView()->GetPage()->SetDefaultPageScaleLimits(kMinimumZoom,
                                                               kMaximumZoom);
    // Model Chrome text auto-sizing more accurately.
    helper->GetWebView()->GetPage()->GetSettings().SetTextAutosizingEnabled(
        true);
    helper->GetWebView()
        ->GetPage()
        ->GetSettings()
        .SetShrinksViewportContentToFit(true);
    helper->GetWebView()->GetPage()->GetSettings().SetViewportStyle(
        mojom::blink::ViewportStyle::kMobile);
    helper->LoadAhem();

    load(*helper);

    ukm::TestAutoSetUkmRecorder result;

    DCHECK(helper->GetWebView()->MainFrameImpl()->GetFrame()->IsLocalRoot());
    helper->GetWebView()
        ->MainFrameImpl()
        ->GetFrameView()
        ->UpdateAllLifecyclePhasesForTest();
    helper->GetWebView()
        ->MainFrameImpl()
        ->GetFrameView()
        ->GetMobileFriendlinessChecker()
        ->ComputeNowForTesting();

    auto entries = result.GetEntriesByName("MobileFriendliness");
    EXPECT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0]->event_hash,
              ukm::builders::MobileFriendliness::kEntryNameHash);
    return *entries[0];
  }

 public:
  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  ukm::mojom::UkmEntry CalculateMetricsForHTMLString(const std::string& html,
                                                     float device_scale = 1.0) {
    return EvalMobileFriendlinessUKM(
        [&](frame_test_helpers::WebViewHelper& helper) {
          frame_test_helpers::LoadHTMLString(
              helper.GetWebView()->MainFrameImpl(), html,
              url_test_helpers::ToKURL("about:blank"));
        },
        device_scale);
  }

  ukm::mojom::UkmEntry CalculateMetricsForFile(const std::string& path,
                                               float device_scale = 1.0) {
    return EvalMobileFriendlinessUKM(
        [&](frame_test_helpers::WebViewHelper& helper) {
          url_test_helpers::RegisterMockedURLLoadFromBase(
              WebString::FromUTF8(kBaseUrl), blink::test::CoreTestDataPath(),
              WebString::FromUTF8(path));
          frame_test_helpers::LoadFrame(helper.GetWebView()->MainFrameImpl(),
                                        kBaseUrl + path);
        },
        device_scale);
  }

  static void ExpectUkm(const ukm::mojom::UkmEntry& ukm,
                        uint64_t name_hash,
                        int expected) {
    auto it = ukm.metrics.find(name_hash);
    EXPECT_NE(it, ukm.metrics.end());
    EXPECT_EQ(it->second, expected);
  }

  static void ExpectUkmLT(const ukm::mojom::UkmEntry& ukm,
                          uint64_t name_hash,
                          int expected) {
    auto it = ukm.metrics.find(name_hash);
    EXPECT_NE(it, ukm.metrics.end());
    EXPECT_LT(it->second, expected);
  }

  static void ExpectUkmGT(const ukm::mojom::UkmEntry& ukm,
                          uint64_t name_hash,
                          int expected) {
    auto it = ukm.metrics.find(name_hash);
    EXPECT_NE(it, ukm.metrics.end());
    EXPECT_GT(it->second, expected);
  }
  test::TaskEnvironment task_environment_;
};

TEST_F(MobileFriendlinessCheckerTest, NoViewportSetting) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString("<body>bar</body>");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportDeviceWidthNameHash,
            false);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            true);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash,
            100);
}

TEST_F(MobileFriendlinessCheckerTest, DeviceWidth) {
  ukm::mojom::UkmEntry ukm =
      CalculateMetricsForFile("viewport/viewport-1.html");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportDeviceWidthNameHash,
            true);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            true);
}

TEST_F(MobileFriendlinessCheckerTest, HardcodedViewport) {
  ukm::mojom::UkmEntry ukm =
      CalculateMetricsForFile("viewport/viewport-30.html");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportDeviceWidthNameHash,
            false);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            true);
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportHardcodedWidthNameHash,
            340);  // Bucketed 200 -> 340.
}

TEST_F(MobileFriendlinessCheckerTest, HardcodedViewportWithDeviceScale3) {
  ukm::mojom::UkmEntry ukm =
      CalculateMetricsForFile("viewport/viewport-30.html",
                              /*device_scale=*/3.0);
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportDeviceWidthNameHash,
            false);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            true);
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportHardcodedWidthNameHash,
            340);  // Bucketed 200 -> 340.
}

TEST_F(MobileFriendlinessCheckerTest, DeviceWidthWithInitialScale05) {
  // Specifying initial-scale=0.5 is usually not the best choice for most web
  // pages. But we cannot determine that such page must not be mobile friendly.
  ukm::mojom::UkmEntry ukm =
      CalculateMetricsForFile("viewport/viewport-34.html");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportDeviceWidthNameHash,
            true);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            true);
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportInitialScaleX10NameHash,
            6);  // Bucketed 5 -> 6.
}

TEST_F(MobileFriendlinessCheckerTest, AllowUserScalableWithSmallMaxZoom) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
    <head>
      <meta name="viewport" content="user-scalable=yes, maximum-scale=1.1">
    </head>
  )HTML");
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            false);
}

TEST_F(MobileFriendlinessCheckerTest, AllowUserScalableWithLargeMaxZoom) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
    <head>
      <meta name="viewport" content="user-scalable=yes, maximum-scale=2.0">
    </head>
  )HTML");
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            true);
}

TEST_F(MobileFriendlinessCheckerTest,
       AllowUserScalableWithLargeMaxZoomAndLargeInitialScale) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
    <head>
      <meta name="viewport" content="user-scalable=yes, maximum-scale=2.0, initial-scale=1.9">
    </head>
  )HTML");
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            false);
}

TEST_F(MobileFriendlinessCheckerTest, UserZoom) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForFile(
      "viewport-initial-scale-and-user-scalable-no.html");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportDeviceWidthNameHash,
            true);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            false);
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportInitialScaleX10NameHash,
            18);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash,
            100);
}

TEST_F(MobileFriendlinessCheckerTest, NoText) {
  ukm::mojom::UkmEntry ukm =
      CalculateMetricsForHTMLString(R"HTML(<body></body>)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportDeviceWidthNameHash,
            false);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            true);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash, 0);
}

TEST_F(MobileFriendlinessCheckerTest, NoSmallFonts) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
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
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportDeviceWidthNameHash,
            false);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            true);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash, 0);
}

TEST_F(MobileFriendlinessCheckerTest, NoSmallFontsWithDeviceScaleFactor) {
  ukm::mojom::UkmEntry ukm =
      CalculateMetricsForHTMLString(R"HTML(
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
)HTML",
                                    /*device_scale=*/2.0);
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportDeviceWidthNameHash,
            false);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            true);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash, 0);
}

TEST_F(MobileFriendlinessCheckerTest, OnlySmallFonts) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
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
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportDeviceWidthNameHash,
            false);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            true);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash,
            100);
}

TEST_F(MobileFriendlinessCheckerTest, OnlySmallFontsWithDeviceScaleFactor) {
  ukm::mojom::UkmEntry ukm =
      CalculateMetricsForHTMLString(R"HTML(
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
)HTML",
                                    /*device_scale=*/2.0);
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportDeviceWidthNameHash,
            false);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            true);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash,
            100);
}

TEST_F(MobileFriendlinessCheckerTest, MostlySmallFont) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
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
)HTML");
  ExpectUkmLT(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash,
              100);
  ExpectUkmGT(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash,
              80);
}

TEST_F(MobileFriendlinessCheckerTest, MostlySmallInSpan) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
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
)HTML");
  ExpectUkmLT(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash,
              100);
  ExpectUkmGT(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash,
              80);
}

TEST_F(MobileFriendlinessCheckerTest, MultipleDivs) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
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
)HTML");
  ExpectUkmLT(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash,
              90);
  ExpectUkmGT(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash,
              60);
}

TEST_F(MobileFriendlinessCheckerTest, DontCountInvisibleSmallFontArea) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
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
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportDeviceWidthNameHash,
            false);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            true);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash, 0);
}

TEST_F(MobileFriendlinessCheckerTest, ScaleZoomedLegibleFont) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=10">
  </head>
  <body style="font-size: 5px">
    Legible text in 50px.
  </body>
</html>
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportDeviceWidthNameHash,
            true);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            true);
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportInitialScaleX10NameHash,
            74);  // Bucketed 100 -> 74.
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash, 0);
}

TEST_F(MobileFriendlinessCheckerTest, ViewportZoomedOutIllegibleFont) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=0.5">
  </head>
  <body style="font-size: 16px; width: 960px">
    Illegible text in 8px.
  </body>
</html>
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportDeviceWidthNameHash,
            false);
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportHardcodedWidthNameHash,
            480);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            true);
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportInitialScaleX10NameHash,
            6);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash,
            100);
}

TEST_F(MobileFriendlinessCheckerTest, TooWideViewportWidthIllegibleFont) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <meta name="viewport" content="width=960">
  </head>
  <body style="font-size: 12px">
    Illegible text in 6px.
  </body>
</html>
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportDeviceWidthNameHash,
            false);
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportHardcodedWidthNameHash,
            820);  // Bucketed 960 -> 820.
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            true);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash,
            100);
}

TEST_F(MobileFriendlinessCheckerTest, CSSZoomedIllegibleFont) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <body style="font-size: 12px; zoom:50%">
    Illegible text in 6px.
  </body>
</html>
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportDeviceWidthNameHash,
            false);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            true);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash,
            100);
}

TEST_F(MobileFriendlinessCheckerTest, OnlySmallFontsClipped) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <body style="font-size: 6px; clip: rect(0 0 0 0); position: absolute">
    Small font text.
  </body>
</html>
)HTML");
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash, 0);
}

TEST_F(MobileFriendlinessCheckerTest, NormalTextAndWideImage) {
  // Wide image forces Chrome to zoom out.
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <body style="margin:0px">
    <img style="width:720px; height:800px">
    <p style="font-size: 12pt">Normal font text.</p>
  </body>
</html>
)HTML");
  // Automatic zoom-out makes text small and image fits in display.
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash,
            100);
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            0);
}

TEST_F(MobileFriendlinessCheckerTest, SmallTextByWideTable) {
  // Wide image forces Chrome to zoom out.
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <body style="font-size: 12pt">
    <table>
      <tr>
        <td width=100px>a</td>
        <td width=100px>b</td>
        <td width=100px>c</td>
      </tr>
    </table>
  </body>
</html>
)HTML");
  // Automatic zoom-out makes text small.
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash,
            100);
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            0);
}

TEST_F(MobileFriendlinessCheckerTest,
       NormalTextAndWideImageWithDeviceWidthViewport) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <meta name="viewport" content="width=device-width">
  </head>
  <body>
    <img style="width:5000px; height:50px">
    <p style="font-size: 12pt">Normal font text.</p>
  </body>
</html>
)HTML");
  // Automatic zoom-out makes text small and image fits in display.
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash,
            100);
  ExpectUkmGT(ukm,
              ukm::builders::MobileFriendliness::
                  kTextContentOutsideViewportPercentageNameHash,
              10);
}

TEST_F(MobileFriendlinessCheckerTest, ZIndex) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <meta name="viewport" content="width=device-width,initial-scale=1.0">
  </head>
  <body style="margin:240px;font-size: 12pt">
    <div style="z-index: 1">
      hello
      <div style="z-index: 10">
        foo
        <img style="width:5000px; height:380px">
        <p>Normal font text.</p>
      </div>
    </div>
  </body>
</html>
)HTML");
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash, 0);
  ExpectUkmGT(ukm,
              ukm::builders::MobileFriendliness::
                  kTextContentOutsideViewportPercentageNameHash,
              50);
}

TEST_F(MobileFriendlinessCheckerTest, NormalTextAndWideImageWithInitialScale) {
  // initial-scale=1.0 prevents the automatic zoom out.
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body style="margin: 0px">
    <img style="width:3000px; height:240px">
    <p style="font-size: 9pt">Normal font text.</p>
  </body>
</html>
)HTML");
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash, 0);
  ExpectUkmGT(ukm,
              ukm::builders::MobileFriendliness::
                  kTextContentOutsideViewportPercentageNameHash,
              50);
}

TEST_F(MobileFriendlinessCheckerTest,
       NormalTextAndWideImageWithInitialScaleAndDeviceScale) {
  ukm::mojom::UkmEntry ukm =
      CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body style="margin: 0px">
    <img style="width:3000px; height:240px">
    <p style="font-size: 6pt">Illegible font text.</p>
  </body>
</html>
)HTML",
                                    /*device_scale=*/2.0);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash,
            100);
  ExpectUkmGT(ukm,
              ukm::builders::MobileFriendliness::
                  kTextContentOutsideViewportPercentageNameHash,
              100);
}

// This test shows that text will grow with text-size-adjust: auto in a
// fixed-width table.
TEST_F(MobileFriendlinessCheckerTest, FixedWidthTableTextSizeAdjustAuto) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <body>
    <table width="800">
      <tr><td style="font-size: 12px; text-size-adjust: auto">
        blah blah blah blah blah blah blah blah blah blah blah blah blah blah
        blah blah blah blah blah blah blah blah blah blah blah blah blah blah
        blah blah blah blah blah blah blah blah blah blah blah blah blah blah
        blah blah blah blah blah blah blah blah blah blah blah blah blah blah
      </td></tr>
    </table>
  </body>
</html>
)HTML");
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash, 0);
}

// This test shows that text remains small with text-size-adjust: none in a
// fixed-width table.
TEST_F(MobileFriendlinessCheckerTest, FixedWidthTableTextSizeAdjustNone) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <body>
    <table width="800">
      <tr><td style="font-size: 12px; text-size-adjust: none">
        blah blah blah blah blah blah blah blah blah blah blah blah blah blah
        blah blah blah blah blah blah blah blah blah blah blah blah blah blah
        blah blah blah blah blah blah blah blah blah blah blah blah blah blah
        blah blah blah blah blah blah blah blah blah blah blah blah blah blah
      </td></tr>
    </table>
  </body>
</html>
)HTML");
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash,
            100);
}

TEST_F(MobileFriendlinessCheckerTest, TextNarrow) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <meta name="viewport" content="initial-scale=.25">
  </head>
  <body>
    <pre>foo foo foo foo foo</pre>
  </body>
</html>
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            0);
}

TEST_F(MobileFriendlinessCheckerTest, TextTooWide) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(
      R"HTML(
<html>
  <head>
    <link rel="stylesheet" type="text/css" href="/fonts/ahem.css" />
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body>
    <pre style="font: 30px Ahem; line-height: 1">)HTML" +
      std::string(10000, 'a') +
      R"HTML(</pre>
  </body>
</html>
)HTML");
  ExpectUkmGT(ukm,
              ukm::builders::MobileFriendliness::
                  kTextContentOutsideViewportPercentageNameHash,
              20);
}

TEST_F(MobileFriendlinessCheckerTest, TextAbsolutePositioning) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(
      R"HTML(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body style="font-size: 12px">
    <pre style="position:absolute; left:2000px">)HTML" +
      std::string(10000, 'a') +
      R"HTML(</pre>
  </body>
</html>
)HTML");
  ExpectUkmGT(ukm,
              ukm::builders::MobileFriendliness::
                  kTextContentOutsideViewportPercentageNameHash,
              14);
}

TEST_F(MobileFriendlinessCheckerTest, ImageAbsolutePositioning) {
  ukm::mojom::UkmEntry ukm_full = CalculateMetricsForHTMLString(
      R"HTML(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body style="margin: 0px">
    <img style="width:480px; height:800px; position:absolute; left:480px">
  </body>
</html>
)HTML");
  ExpectUkm(ukm_full,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            100);

  ukm::mojom::UkmEntry ukm_half = CalculateMetricsForHTMLString(
      R"HTML(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body style="margin: 0px">
    <img style="width:480px; height:800px; position:absolute; left:240px">
  </body>
</html>
)HTML");
  ExpectUkm(ukm_half,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            50);
}

TEST_F(MobileFriendlinessCheckerTest, SmallTextOutsideViewportCeiling) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(
      R"HTML(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body style="font-size: 12px">
    <pre style="position:absolute; left:2000px">x</pre>
  </body>
</html>
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            1);
}

TEST_F(MobileFriendlinessCheckerTest, TextTooWideOverflowXHidden) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(
      R"HTML(
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
  </head>
  <body>
    <pre style="overflow-x:hidden; font-size:12px">)HTML" +
      std::string(10000, 'a') + R"HTML(</pre>
  </body>
</html>
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            0);
}

TEST_F(MobileFriendlinessCheckerTest, TextTooWideHidden) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(
      R"HTML(
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
  </head>
  <body>
    <pre style="overflow:hidden">)HTML" +
      std::string(10000, 'a') +
      R"HTML(</pre>
  </body>
</html>
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            0);
}

TEST_F(MobileFriendlinessCheckerTest, TextTooWideHiddenInDiv) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(
      R"HTML(
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
  </head>
  <body>
    <div style="overflow:hidden; font-size: 12px">
      <pre>)HTML" +
      std::string(10000, 'a') +
      R"HTML(
      </pre>
    </div>
  </body>
</html>
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            0);
}

TEST_F(MobileFriendlinessCheckerTest, TextTooWideHiddenInDivDiv) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(
      R"HTML(
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
  </head>
  <body>
    <div style="overflow:hidden">
      <div>
        <pre>)HTML" +
      std::string(10000, 'a') +
      R"HTML(
        </pre>
      <div>
    </div>
  </body>
</html>
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            0);
}

TEST_F(MobileFriendlinessCheckerTest, ImageNarrow) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <body>
    <img style="width:200px; height:50px">
  </body>
</html>
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            0);
}

TEST_F(MobileFriendlinessCheckerTest, ImageTooWide) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body>
    <img style="width:2000px; height:50px">
  </body>
</html>
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            20);
}

TEST_F(MobileFriendlinessCheckerTest, ImageTooWide100) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body style="margin:0px;">
    <img style="width:960px; height:800px">
  </body>
</html>
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            100);
}

TEST_F(MobileFriendlinessCheckerTest, WideImageClipped) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body>
    <div style="overflow: hidden">
      <img style="width:2000px; height:50px">
    </div>
  </body>
</html>
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            0);
}

TEST_F(MobileFriendlinessCheckerTest, ImageTooWideTwoImages) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
  </head>
  <body style="width:4036px">
    <img style="width:2000px; height:50px">
    <img style="width:2000px; height:50px">
  </body>
</html>
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            46);
}

TEST_F(MobileFriendlinessCheckerTest, ImageTooWideAbsolutePosition) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
  </head>
  <body>
    <img style="width:480px; height:800px; position:absolute; left:2000px">
  </body>
</html>
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            100);
}

TEST_F(MobileFriendlinessCheckerTest, ImageTooWideDisplayNone) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <body>
    <img style="width:2000px; height:50px; display:none">
  </body>
</html>
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            0);
}

TEST_F(MobileFriendlinessCheckerTest, ScaleTextOutsideViewport) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <link rel="stylesheet" type="text/css" href="/fonts/ahem.css" />
    <meta name="viewport" content="minimum-scale=1, initial-scale=3">
  </head>
  <body style="font: 76px Ahem; width: 480">
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
)HTML");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportInitialScaleX10NameHash,
            26);  // Bucketed 30 -> 26.
  ExpectUkmGT(ukm,
              ukm::builders::MobileFriendliness::
                  kTextContentOutsideViewportPercentageNameHash,
              55);
}

TEST_F(MobileFriendlinessCheckerTest, ScrollerOutsideViewport) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <link rel="stylesheet" type="text/css" href="/fonts/ahem.css" />
    <style>
      body {
        margin: 0px;
      }
      div.scrollmenu {
        background-color: #333;
        white-space: nowrap;
      }
      div.scrollmenu a {
        display: inline-block;
        color: white;
        padding: 14px;
      }
    </style>
    <meta name="viewport" content="width=480px, initial-scale=1.0 minimum-scale=1.0">
  </head>
  <body style="font: 40px/1 Ahem; line-height: 1; height: 200px">
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
      <a href="#11">Eleventh text</a>
      <a href="#12">Twelveth text</a>
    </div>
  </body>
</html>
)HTML");
  // the viewport
  ExpectUkmGT(ukm,
              ukm::builders::MobileFriendliness::
                  kTextContentOutsideViewportPercentageNameHash,
              10);
}

TEST_F(MobileFriendlinessCheckerTest, SubScroller) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <link rel="stylesheet" type="text/css" href="/fonts/ahem.css" />
    <style>
      body {
        margin: 0px;
      }
      div.scrollmenu {
        width: 480px;
        background-color: #333;
        overflow: scroll;
        white-space: nowrap;
      }
      div.scrollmenu a {
        display: inline-block;
        color: white;
        padding: 14px;
      }
    </style>
    <meta name="viewport" content="width=480px, initial-scale=1.0 minimum-scale=1.0">
  </head>
  <body style="font: 40px/1 Ahem; line-height: 1">
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
    <a href="#11">Eleventh text</a>
    <a href="#12">Twelveth text</a>
  </div>
  </body>
</html>
)HTML");
  // Fits within the viewport by scrollbar.
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            0);
}

TEST_F(MobileFriendlinessCheckerTest, SubScrollerHalfOutByMargin) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <link rel="stylesheet" type="text/css" href="/fonts/ahem.css" />
    <style>
      body {
        margin: 0px;
      }
      div.scrollmenu {
        margin-left: 240px;
        width: 480px;
        height: 800px;
        background-color: #333;
        overflow: scroll;
        white-space: nowrap;
      }
      div.scrollmenu a {
        display: inline-block;
        color: white;
        padding: 14px;
      }
    </style>
    <meta name="viewport" content="width=480px, initial-scale=1.0 minimum-scale=1.0">
  </head>
  <body style="font: 40px/1 Ahem; line-height: 1">
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
    <a href="#11">Eleventh text</a>
    <a href="#12">Twelveth text</a>
  </div>
  </body>
</html>
)HTML");
  // Fits within the viewport by scrollbar.
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            50);
}

TEST_F(MobileFriendlinessCheckerTest, SubScrollerOutByTranslate) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <link rel="stylesheet" type="text/css" href="/fonts/ahem.css" />
    <style>
      body {
        margin: 0px;
      }
      div.scrollmenu {
        transform: translate(360px, 0px);
        width: 480px;
        height: 800px;
        background-color: #333;
        overflow: scroll;
        white-space: nowrap;
      }
      div.scrollmenu a {
        display: inline-block;
        color: white;
        padding: 14px;
      }
    </style>
    <meta name="viewport" content="width=480px, initial-scale=1.0 minimum-scale=1.0">
  </head>
  <body style="font: 40px/1 Ahem; line-height: 1">
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
    <a href="#11">Eleventh text</a>
    <a href="#12">Twelveth text</a>
  </div>
  </body>
</html>
)HTML");
  // Fits within the viewport by scrollbar.
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            75);
}

/*
 * TODO(kumagi): Get precise paint offset of rtl environment is hard.
TEST_F(MobileFriendlinessCheckerTest, SubScrollerGoesLeft) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <style>
      body {
        margin: 0px;
        direction: rtl;
      }
      div.scroller {
        margin-right: 360px;
        width: 480px;
        height: 800px;
        overflow: scroll;
      }
    </style>
    <meta name="viewport" content="width=480px, initial-scale=1.0
minimum-scale=1.0">
  </head>
  <body style="font: 40px/1 Ahem; line-height: 1">
    <div class="scroller">
      <img style="width: 9000px; height: 1px">
    </div>
  </body>
</html>
)HTML");
  // Right to left language scrollbar goes to left.
  ExpectUkm(actual_mf.text_content_outside_viewport_percentage, 75);
}
*/

TEST_F(MobileFriendlinessCheckerTest, SubScrollerFitsWithinViewport) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <style>
      body {
        margin: 0px;
      }
      div.scroller1 {
        width: 481px;
        height: 1px;
        overflow: scroll;
      }
      div.scroller2 {
        width: 10px;
        height: 800px;
        overflow: scroll;
      }
    </style>
    <meta name="viewport" content="width=480px, initial-scale=1.0 minimum-scale=1.0">
  </head>
  <body style="font: 40px/1 Ahem; line-height: 1">
    <div class="scroller1">
      <img style="width: 9000px; height: 1px">
      This div goes out of viewport width 1px.
    </div>
    <div class="scroller2">
      <img style="width: 9000px; height: 1px">
      This div fits within viewport width.
    </div>
  </body>
</html>
)HTML");
  // Only scroller1 gets out of viewport width.
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            1);
}

TEST_F(MobileFriendlinessCheckerTest, SubScrollerTwice) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <style>
      body {
        margin: 0px;
      }
      div.scroller {
        margin-left: 240px;
        width: 480px;
        height: 400px;
        overflow: scroll;
      }
    </style>
    <meta name="viewport" content="width=480px, initial-scale=1.0 minimum-scale=1.0">
  </head>
  <body style="font: 40px/1 Ahem; line-height: 1">
    <div class="scroller">
      <img style="width: 9000px; height: 1px">
      hello this is a pen.
    </div>
    <div class="scroller">
      <img style="width: 9000px; height: 1px">
    </div>
  </body>
</html>
)HTML");
  // Both of subscrollers get out of viewport width.
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            50);
}

TEST_F(MobileFriendlinessCheckerTest, SubScrollerInSubScroller) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <style>
      body {
        margin: 0px;
      }
      div.scroller {
        width: 480px;
        height: 200px;
        overflow: scroll;
      }
    </style>
    <meta name="viewport" content="width=480px, initial-scale=1.0 minimum-scale=1.0">
  </head>
  <body style="font: 20px/1; line-height: 1">
    <div class="scroller" style="margin-left: 240px;">
      240px*200px gets out of viewport.
      <img style="width: 9000px; height: 1px">
    </div>
    <div class="scroller" style="margin-left: 480px;">
      480px*200px gets out of viewport.
      <img style="width: 9000px; height: 1px">
    </div>
    <div class="scroller" style="margin-left: 240px;">
      240px*200px gets out of viewport.
      <img style="width: 9000px; height: 1px">
      <div class="scroller">
        Contents inside of scroller will be ignored from the
        text_content_outside_viewport_percentage metrics.
        <img style="width: 9000px; height: 1px">
      </div>
    </div>
    <div class="scroller" style="margin-left: 480px;">
      480px*200px gets out of viewport.
      Hereby (240px*2)*400px + (480px*2)*400px gets out of viewport(480px*800px),
      This is exactly 0.75. Then text_content_outside_viewport_percentage should be 75.
      <img style="width: 9000px; height: 1px">
      <div class="scroller">
        <img style="width: 9000px; height: 1px">
        <div class="scroller">
          <img style="width: 9000px; height: 1px">
        </div>
      </div>
    </div>
  </body>
</html>
)HTML");
  // Fits within the viewport by scrollbar.
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            75);
}

TEST_F(MobileFriendlinessCheckerTest, ScrollableLayoutView) {
  ukm::mojom::UkmEntry ukm = CalculateMetricsForHTMLString(R"HTML(
<html>
  <head>
    <style>
      body {
        margin: 0px;
        width: 960px;
        height: 800px;
      }
    </style>
    <meta name="viewport" content="width=480px, initial-scale=1.0 minimum-scale=1.0">
  </head>
  <body style="font: 20px/1; line-height: 1">
    <img style="width: 600px; height: 800px">
  </body>
</html>
)HTML");
  // Fits within the viewport by scrollbar.
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::
                kTextContentOutsideViewportPercentageNameHash,
            25);
}

TEST_F(MobileFriendlinessCheckerTest, IFrame) {
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(kBaseUrl), blink::test::CoreTestDataPath(),
      WebString::FromUTF8("visible_iframe.html"));
  const ukm::mojom::UkmEntry ukm =
      CalculateMetricsForFile("single_iframe.html");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportDeviceWidthNameHash,
            false);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            true);
}

TEST_F(MobileFriendlinessCheckerTest, IFrameVieportDeviceWidth) {
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(kBaseUrl), blink::test::CoreTestDataPath(),
      WebString::FromUTF8("viewport/viewport-1.html"));
  const ukm::mojom::UkmEntry ukm =
      CalculateMetricsForFile("page_contains_viewport_iframe.html");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportDeviceWidthNameHash,
            true);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            true);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash, 0);
}

TEST_F(MobileFriendlinessCheckerTest, IFrameSmallTextRatio) {
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(kBaseUrl), blink::test::CoreTestDataPath(),
      WebString::FromUTF8("small_text_iframe.html"));
  const ukm::mojom::UkmEntry ukm =
      CalculateMetricsForFile("page_contains_small_text_iframe.html");
  ExpectUkm(ukm,
            ukm::builders::MobileFriendliness::kViewportDeviceWidthNameHash,
            false);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kAllowUserZoomNameHash,
            true);
  ExpectUkm(ukm, ukm::builders::MobileFriendliness::kSmallTextRatioNameHash,
            100);
}

}  // namespace blink
