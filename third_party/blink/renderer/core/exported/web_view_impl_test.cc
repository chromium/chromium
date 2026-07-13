// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/exported/web_view_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

class WebViewImplTest : public testing::Test {
 public:
  WebViewImplTest() = default;

  void SetUp() override {
    WebRuntimeFeatures::EnableFeatureFromString("TextScaleMetaTag", true);
    web_view_helper_.Initialize();
  }

  void TearDown() override {
    web_view_helper_.Reset();
    WebRuntimeFeatures::EnableFeatureFromString("TextScaleMetaTag", false);
  }

  WebViewImpl* WebView() const { return web_view_helper_.GetWebView(); }

 protected:
  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_F(WebViewImplTest, WebViewDoubleScaling) {
  WebViewImpl* web_view = WebView();

  frame_test_helpers::LoadHTMLString(
      web_view->MainFrameImpl(),
      "<!DOCTYPE html>"
      "<style>"
      "  .scaled {"
      "    font-size: 16px;"
      "    text-size-adjust: calc(100% * env(preferred-text-scale));"
      "  }"
      "</style>"
      "<div id='test' class='scaled'>Hello</div>",
      url_test_helpers::ToKURL("http://example.com/"));
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);
  // Now set Settings in the same order that WebView does.
  web_view->GetSettings()->SetTextSizeAdjustEnabled(true);
  if (RuntimeEnabledFeatures::WebViewEnvReorderFixEnabled()) {
    web_view->GetSettings()->SetScaleAllFontsIfNoMetaTextScaleTag(true);
  }
  web_view->GetSettings()->SetAccessibilityFontScaleFactor(1.5f);
  if (!RuntimeEnabledFeatures::WebViewEnvReorderFixEnabled()) {
    web_view->GetSettings()->SetScaleAllFontsIfNoMetaTextScaleTag(true);
  }
  web_view->MainFrameImpl()->GetFrame()->SetTextZoomFactor(1.5f);

  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  Element* test_element = document->getElementById(AtomicString("test"));
  ASSERT_TRUE(test_element);
  ASSERT_TRUE(test_element->GetComputedStyle());

  // We expect NO double scaling (24px).
  // If bug is present, it will be 36px.
  EXPECT_FLOAT_EQ(24.0f, test_element->GetComputedStyle()->FontSize());
}

TEST_F(WebViewImplTest, MaximumLegiblePageScale) {
  WebViewImpl* web_view = WebView();
  web_view->EnableFakePageScaleAnimationForTesting(true);
  web_view->GetSettings()->SetAccessibilityFontScaleFactor(2.0f);

  // Set up a page with text-scale meta tag.
  frame_test_helpers::LoadHTMLString(
      web_view->MainFrameImpl(),
      "<!DOCTYPE html>"
      "<meta name='text-scale' content='scale'>"
      "<style> body { font-size: 16px; width: 1000px; height: 1000px; } "
      "</style>"
      "<div>Content</div>",
      url_test_helpers::ToKURL("http://example.com/"));

  web_view->MainFrameWidget()->Resize(gfx::Size(800, 600));
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  // Trigger double tap zoom.
  // We need a rect that would result in a large scale if not capped.
  // E.g. a small rect.
  gfx::Point point(100, 100);
  gfx::Rect rect(90, 90, 20, 20);

  web_view->AnimateDoubleTapZoom(point, rect);

  // Check the scale.
  // The default maximum legible scale is 1.0 (from web_view_impl.cc:
  // maximum_legible_scale_ = 1) With font scale 2.0 and meta tag, it should be
  // capped at 1.0 * 2.0 = 2.0.

  EXPECT_FLOAT_EQ(2.0f, web_view->FakePageScaleAnimationPageScaleForTesting());
}

TEST_F(WebViewImplTest, MaximumLegiblePageScaleWithoutMetaTag) {
  WebViewImpl* web_view = WebView();
  web_view->EnableFakePageScaleAnimationForTesting(true);
  web_view->GetSettings()->SetWideViewportQuirkEnabled(
      true);  // Simulate WebView quirk
  web_view->GetSettings()->SetAccessibilityFontScaleFactor(2.0f);

  // Set up a page WITHOUT text-scale meta tag.
  frame_test_helpers::LoadHTMLString(
      web_view->MainFrameImpl(),
      "<!DOCTYPE html>"
      "<style> body { font-size: 16px; width: 1000px; height: 1000px; } "
      "</style>"
      "<div>Content</div>",
      url_test_helpers::ToKURL("http://example.com/"));

  web_view->MainFrameWidget()->Resize(gfx::Size(800, 600));
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  gfx::Point point(100, 100);
  gfx::Rect rect(90, 90, 20, 20);

  web_view->AnimateDoubleTapZoom(point, rect);

  // default maximum_legible_scale_ is 1.0, but AnimateDoubleTapZoom logic bumps
  // it to at least 1.2 (doubleTapZoomAlreadyLegibleRatio).
  EXPECT_FLOAT_EQ(1.2f, web_view->FakePageScaleAnimationPageScaleForTesting());
}

// Test where the page adds and removes <meta text-scale>.
TEST_F(WebViewImplTest, DynamicMetaTagTextZoom) {
  WebViewImpl* web_view = WebView();
  web_view->GetPage()->GetSettings().SetDefaultFontSize(16);

#if BUILDFLAG(IS_ANDROID)
  // Simulate a 2x OS-level font scale
  web_view->GetSettings()->SetScaleAllFontsIfNoMetaTextScaleTag(true);
  web_view->GetSettings()->SetTextSizeAdjustEnabled(true);
  web_view->GetSettings()->SetAccessibilityFontScaleFactor(2.0f);
  web_view->MainFrameImpl()->GetFrame()->SetTextZoomFactor(2.0f);
#else
  // Simulate a 1.5x OS-level font scale, and a 2.0x OS-level device
  // scale factor (3.0x effective)
  web_view->SetZoomFactorForDeviceScaleFactor(/*device_scale_factor=*/3.0f,
                                              /*text_scale_multiplier=*/1.5f);
#endif  // BUILDFLAG(IS_ANDROID)

  frame_test_helpers::LoadHTMLString(
      web_view->MainFrameImpl(),
      "<!DOCTYPE html>"
      "<div id='test-medium-font'></div>"
      "<div id='test-fixed-font' style='font-size: 16px'></div>"
      "<div id='test-env' style='width: calc(env(preferred-text-scale) * "
      "100px)'></div>"
      "<div id='test-width' style='width: 100px'></div>",
      url_test_helpers::ToKURL("http://example.com/"));
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  Element* head = document->head();
  Element* test_medium_font =
      document->getElementById(AtomicString("test-medium-font"));
  Element* test_fixed_font =
      document->getElementById(AtomicString("test-fixed-font"));
  Element* test_env = document->getElementById(AtomicString("test-env"));
  Element* test_width = document->getElementById(AtomicString("test-width"));
  ASSERT_TRUE(test_medium_font);
  ASSERT_TRUE(test_fixed_font);
  ASSERT_TRUE(test_env);
  ASSERT_TRUE(test_width);
  ASSERT_TRUE(test_medium_font->GetComputedStyle());
  ASSERT_TRUE(test_fixed_font->GetComputedStyle());
  ASSERT_TRUE(test_env->GetComputedStyle());
  ASSERT_TRUE(test_width->GetComputedStyle());

  // Initial state, no meta tag:
#if BUILDFLAG(IS_ANDROID)
  // TextZoomFactor is 2.0.
  // Default font size 16px -> 32px.
  // Fixed font size 16px -> 32px.
  // env(preferred-text-scale) is 1.0 (hidden), so width should be 100.
  // Width is 100.
  EXPECT_FLOAT_EQ(2.0f,
                  web_view->MainFrameImpl()->GetFrame()->TextZoomFactor());
  EXPECT_FLOAT_EQ(32.0f, test_medium_font->GetComputedStyle()->FontSize());
  EXPECT_FLOAT_EQ(32.0f, test_fixed_font->GetComputedStyle()->FontSize());
  EXPECT_FLOAT_EQ(100.0f, test_env->GetComputedStyle()->Width().Pixels());
  EXPECT_FLOAT_EQ(100.0f, test_width->GetComputedStyle()->Width().Pixels());
#else
  // Device scale factor is 3.0.
  // Default font size 16px, scaled to 48px.
  // Fix font size 16px, scaled to 48px.
  // env(preferred-text-scale) is 1.0 (hidden), so width is 100, scaled to 300.
  // Width is 100, scaled to 300.
  EXPECT_FLOAT_EQ(48.0f, test_medium_font->GetComputedStyle()->FontSize());
  EXPECT_FLOAT_EQ(48.0f, test_fixed_font->GetComputedStyle()->FontSize());
  EXPECT_FLOAT_EQ(300.0f, test_env->GetComputedStyle()->Width().Pixels());
  EXPECT_FLOAT_EQ(300.0f, test_width->GetComputedStyle()->Width().Pixels());
#endif  // BUILDFLAG(IS_ANDROID)

  // 1. Append meta tag.
  Element* meta = document->CreateRawElement(html_names::kMetaTag);
  meta->setAttribute(html_names::kNameAttr, AtomicString("text-scale"));
  meta->setAttribute(html_names::kContentAttr, AtomicString("scale"));
  head->AppendChild(meta);
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  // Meta tag present:
#if BUILDFLAG(IS_ANDROID)
  // TextZoomFactor is 1.0.
  // Default font size 16px -> 32px (Still scaled by
  // AccessibilityFontScaleFactor). Fixed font size 16px -> 16px (Unscaled).
  // env(preferred-text-scale) is 2.0 (exposed), so width should be 200.
  // Width is 100, should remain unscaled to 100.
  EXPECT_FLOAT_EQ(1.0f,
                  web_view->MainFrameImpl()->GetFrame()->TextZoomFactor());
  EXPECT_FLOAT_EQ(32.0f, test_medium_font->GetComputedStyle()->FontSize());
  EXPECT_FLOAT_EQ(16.0f, test_fixed_font->GetComputedStyle()->FontSize());
  EXPECT_FLOAT_EQ(200.0f, test_env->GetComputedStyle()->Width().Pixels());
  EXPECT_FLOAT_EQ(100.0f, test_width->GetComputedStyle()->Width().Pixels());
#else
  // Device scale factor is 2.0 (was 2.0 * 1.5 text multiplier = 3.0)
  // Default font size 16px -> 24px (still scaled by
  // AccessibilityFontScaleFactor), scaled to 48px.
  // Fix font size 16px, scaled to 32px.
  // env(preferred-text-scale) is 1.5 (exposed), so width is 150, scaled to 300.
  // Width is 100, scaled to 200.
  EXPECT_FLOAT_EQ(48.0f, test_medium_font->GetComputedStyle()->FontSize());
  EXPECT_FLOAT_EQ(32.0f, test_fixed_font->GetComputedStyle()->FontSize());
  EXPECT_FLOAT_EQ(300.0f, test_env->GetComputedStyle()->Width().Pixels());
  EXPECT_FLOAT_EQ(200.0f, test_width->GetComputedStyle()->Width().Pixels());
#endif  // BUILDFLAG(IS_ANDROID)

  // 2. Remove meta tag.
  meta->remove();
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  // Back to initial state.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FLOAT_EQ(2.0f,
                  web_view->MainFrameImpl()->GetFrame()->TextZoomFactor());
  EXPECT_FLOAT_EQ(32.0f, test_medium_font->GetComputedStyle()->FontSize());
  EXPECT_FLOAT_EQ(32.0f, test_fixed_font->GetComputedStyle()->FontSize());
  EXPECT_FLOAT_EQ(100.0f, test_env->GetComputedStyle()->Width().Pixels());
  EXPECT_FLOAT_EQ(100.0f, test_width->GetComputedStyle()->Width().Pixels());
#else
  EXPECT_FLOAT_EQ(48.0f, test_medium_font->GetComputedStyle()->FontSize());
  EXPECT_FLOAT_EQ(48.0f, test_fixed_font->GetComputedStyle()->FontSize());
  EXPECT_FLOAT_EQ(300.0f, test_env->GetComputedStyle()->Width().Pixels());
  EXPECT_FLOAT_EQ(300.0f, test_width->GetComputedStyle()->Width().Pixels());
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace blink
