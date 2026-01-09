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

TEST_F(WebViewImplTest, MaximumLegiblePageScale) {
  WebViewImpl* web_view = WebView();
  web_view->EnableFakePageScaleAnimationForTesting(true);
  web_view->GetSettings()->SetAccessibilityFontScaleFactor(2.0f);
  web_view->GetSettings()->SetTextAutosizingEnabled(false);

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
  web_view->GetSettings()->SetTextAutosizingEnabled(false);

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

// Test for Android WebView where the page adds and removes <meta text-scale>.
TEST_F(WebViewImplTest, DynamicMetaTagTextZoom) {
  WebViewImpl* web_view = WebView();
  web_view->GetPage()->GetSettings().SetDefaultFontSize(16);

  // Simulate Android WebView where the user has a 2x OS-level font scale.
  web_view->GetSettings()->SetScaleAllFontsIfNoMetaTextScaleTag(true);
  web_view->GetSettings()->SetAccessibilityFontScaleFactor(2.0f);
  web_view->MainFrameImpl()->GetFrame()->SetTextZoomFactor(2.0f);

  frame_test_helpers::LoadHTMLString(
      web_view->MainFrameImpl(),
      "<!DOCTYPE html>"
      "<div id='test-medium-font'></div>"
      "<div id='test-fixed-font' style='font-size: 16px'></div>"
      "<div id='test-env' style='width: calc(env(preferred-text-scale) * "
      "100px)'></div>",
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
  ASSERT_TRUE(test_medium_font);
  ASSERT_TRUE(test_fixed_font);
  ASSERT_TRUE(test_env);
  ASSERT_TRUE(test_medium_font->GetComputedStyle());
  ASSERT_TRUE(test_fixed_font->GetComputedStyle());
  ASSERT_TRUE(test_env->GetComputedStyle());

  // Initial state, no meta tag:
  // TextZoomFactor is 2.0.
  // Default font size 16px -> 32px.
  // Fixed font size 16px -> 32px.
  // env(preferred-text-scale) is 1.0 (hidden), so width should be 100.
  EXPECT_FLOAT_EQ(2.0f,
                  web_view->MainFrameImpl()->GetFrame()->TextZoomFactor());
  EXPECT_FLOAT_EQ(32.0f, test_medium_font->GetComputedStyle()->FontSize());
  EXPECT_FLOAT_EQ(32.0f, test_fixed_font->GetComputedStyle()->FontSize());
  EXPECT_FLOAT_EQ(100.0f, test_env->GetComputedStyle()->Width().Pixels());

  // 1. Append meta tag.
  Element* meta = document->CreateRawElement(html_names::kMetaTag);
  meta->setAttribute(html_names::kNameAttr, AtomicString("text-scale"));
  meta->setAttribute(html_names::kContentAttr, AtomicString("scale"));
  head->AppendChild(meta);
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  // Meta tag present:
  // TextZoomFactor is 1.0.
  // Default font size 16px -> 32px (Still scaled by
  // AccessibilityFontScaleFactor). Fixed font size 16px -> 16px (Unscaled).
  // env(preferred-text-scale) is 2.0 (exposed), so width should be 200.
  EXPECT_FLOAT_EQ(1.0f,
                  web_view->MainFrameImpl()->GetFrame()->TextZoomFactor());
  EXPECT_FLOAT_EQ(32.0f, test_medium_font->GetComputedStyle()->FontSize());
  EXPECT_FLOAT_EQ(16.0f, test_fixed_font->GetComputedStyle()->FontSize());
  EXPECT_FLOAT_EQ(200.0f, test_env->GetComputedStyle()->Width().Pixels());

  // 2. Remove meta tag.
  meta->remove();
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  // Back to initial state.
  EXPECT_FLOAT_EQ(2.0f,
                  web_view->MainFrameImpl()->GetFrame()->TextZoomFactor());
  EXPECT_FLOAT_EQ(32.0f, test_medium_font->GetComputedStyle()->FontSize());
  EXPECT_FLOAT_EQ(32.0f, test_fixed_font->GetComputedStyle()->FontSize());
  EXPECT_FLOAT_EQ(100.0f, test_env->GetComputedStyle()->Width().Pixels());
}

}  // namespace blink
