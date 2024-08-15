// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/text_autosizer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {
class TextAutosizerClient : public RenderingTestChromeClient {
 public:
  float WindowToViewportScalar(LocalFrame*, const float value) const override {
    return value * device_scale_factor_;
  }
  gfx::Rect LocalRootToScreenDIPs(const gfx::Rect& rect,
                                  const LocalFrameView*) const override {
    return gfx::ScaleToRoundedRect(rect, 1 / device_scale_factor_);
  }

  void set_device_scale_factor(float device_scale_factor) {
    device_scale_factor_ = device_scale_factor;
  }

 private:
  float device_scale_factor_;
};

class TextAutosizerTest : public RenderingTest,
                          public testing::WithParamInterface<bool>,
                          private ScopedTextSizeAdjustImprovementsForTest {
 public:
  TextAutosizerTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()),
        ScopedTextSizeAdjustImprovementsForTest(GetParam()) {}

  RenderingTestChromeClient& GetChromeClient() const override {
    return GetTextAutosizerClient();
  }
  TextAutosizerClient& GetTextAutosizerClient() const {
    DEFINE_STATIC_LOCAL(Persistent<TextAutosizerClient>, client,
                        (MakeGarbageCollected<TextAutosizerClient>()));
    return *client;
  }
  void set_device_scale_factor(float device_scale_factor) {
    GetTextAutosizerClient().set_device_scale_factor(device_scale_factor);

    // This fake ChromeClient cannot update device scale factor (DSF). We apply
    // DSF to the zoom factor manually.
    GetDocument().GetFrame()->SetLayoutZoomFactor(device_scale_factor);
  }

 private:
  void SetUp() override {
    GetTextAutosizerClient().set_device_scale_factor(1.f);
    RenderingTest::SetUp();
    GetDocument().GetSettings()->SetTextAutosizingEnabled(true);
    GetDocument().GetSettings()->SetTextAutosizingWindowSizeOverride(
        gfx::Size(320, 480));
  }
};

INSTANTIATE_TEST_SUITE_P(All, TextAutosizerTest, testing::Bool());

TEST_P(TextAutosizerTest, SimpleParagraph) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='autosized'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");
  Element* autosized = GetElementById("autosized");
  EXPECT_FLOAT_EQ(16.f,
                  autosized->GetLayoutObject()->StyleRef().SpecifiedFontSize());
  // (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 40px.
  EXPECT_FLOAT_EQ(40.f,
                  autosized->GetLayoutObject()->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, TextSizeAdjustDisablesAutosizing) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='textSizeAdjustAuto' style='text-size-adjust: auto;'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
    <div id='textSizeAdjustNone' style='text-size-adjust: none;'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
    <div id='textSizeAdjust100' style='text-size-adjust: 100%;'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");
  LayoutObject* text_size_adjust_auto =
      GetLayoutObjectByElementId("textSizeAdjustAuto");
  EXPECT_FLOAT_EQ(16.f, text_size_adjust_auto->StyleRef().SpecifiedFontSize());
  EXPECT_FLOAT_EQ(40.f, text_size_adjust_auto->StyleRef().ComputedFontSize());
  LayoutObject* text_size_adjust_none =
      GetLayoutObjectByElementId("textSizeAdjustNone");
  EXPECT_FLOAT_EQ(16.f, text_size_adjust_none->StyleRef().SpecifiedFontSize());
  EXPECT_FLOAT_EQ(16.f, text_size_adjust_none->StyleRef().ComputedFontSize());
  LayoutObject* text_size_adjust100 =
      GetLayoutObjectByElementId("textSizeAdjust100");
  EXPECT_FLOAT_EQ(16.f, text_size_adjust100->StyleRef().SpecifiedFontSize());
  EXPECT_FLOAT_EQ(16.f, text_size_adjust100->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, ParagraphWithChangingTextSizeAdjustment) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
      .none { text-size-adjust: none; }
      .small { text-size-adjust: 50%; }
      .large { text-size-adjust: 150%; }
    </style>
    <div id='autosized'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");
  Element* autosized_div = GetElementById("autosized");
  EXPECT_FLOAT_EQ(
      16.f, autosized_div->GetLayoutObject()->StyleRef().SpecifiedFontSize());
  EXPECT_FLOAT_EQ(
      40.f, autosized_div->GetLayoutObject()->StyleRef().ComputedFontSize());

  autosized_div->setAttribute(html_names::kClassAttr, AtomicString("none"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FLOAT_EQ(
      16.f, autosized_div->GetLayoutObject()->StyleRef().SpecifiedFontSize());
  EXPECT_FLOAT_EQ(
      16.f, autosized_div->GetLayoutObject()->StyleRef().ComputedFontSize());

  autosized_div->setAttribute(html_names::kClassAttr, AtomicString("small"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FLOAT_EQ(
      16.f, autosized_div->GetLayoutObject()->StyleRef().SpecifiedFontSize());
  EXPECT_FLOAT_EQ(
      8.f, autosized_div->GetLayoutObject()->StyleRef().ComputedFontSize());

  autosized_div->setAttribute(html_names::kClassAttr, AtomicString("large"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FLOAT_EQ(
      16.f, autosized_div->GetLayoutObject()->StyleRef().SpecifiedFontSize());
  EXPECT_FLOAT_EQ(
      24.f, autosized_div->GetLayoutObject()->StyleRef().ComputedFontSize());

  autosized_div->removeAttribute(html_names::kClassAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FLOAT_EQ(
      16.f, autosized_div->GetLayoutObject()->StyleRef().SpecifiedFontSize());
  EXPECT_FLOAT_EQ(
      40.f, autosized_div->GetLayoutObject()->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, ZeroTextSizeAdjustment) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='textSizeAdjustZero' style='text-size-adjust: 0%;'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");
  LayoutObject* text_size_adjust_zero =
      GetElementById("textSizeAdjustZero")->GetLayoutObject();
  EXPECT_FLOAT_EQ(16.f, text_size_adjust_zero->StyleRef().SpecifiedFontSize());
  EXPECT_FLOAT_EQ(0.f, text_size_adjust_zero->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, NegativeTextSizeAdjustment) {
  SetBodyInnerHTML(
      "<style>"
      "  html { font-size: 16px; }"
      "  body { width: 800px; margin: 0; overflow-y: hidden; }"
      "</style>"
      // Negative values should be treated as auto.
      "<div id='textSizeAdjustNegative' style='text-size-adjust: -10%;'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>");
  LayoutObject* text_size_adjust_negative =
      GetLayoutObjectByElementId("textSizeAdjustNegative");
  EXPECT_FLOAT_EQ(16.f,
                  text_size_adjust_negative->StyleRef().SpecifiedFontSize());
  EXPECT_FLOAT_EQ(40.f,
                  text_size_adjust_negative->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, TextSizeAdjustmentPixelUnits) {
  SetBodyInnerHTML(
      "<style>"
      "  html { font-size: 16px; }"
      "  body { width: 800px; margin: 0; overflow-y: hidden; }"
      "</style>"
      // Non-percentage values should be treated as auto.
      "<div id='textSizeAdjustPixels' style='text-size-adjust: 0.1px;'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>");
  LayoutObject* text_size_adjust_pixels =
      GetLayoutObjectByElementId("textSizeAdjustPixels");
  EXPECT_FLOAT_EQ(16.f,
                  text_size_adjust_pixels->StyleRef().SpecifiedFontSize());
  EXPECT_FLOAT_EQ(40.f, text_size_adjust_pixels->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, NestedTextSizeAdjust) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='textSizeAdjustA' style='text-size-adjust: 47%;'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
      <div id='textSizeAdjustB' style='text-size-adjust: 53%;'>
        Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
        eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
        ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
        aliquip ex ea commodo consequat. Duis aute irure dolor in
        reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
        pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
        culpa qui officia deserunt mollit anim id est laborum.
      </div>
    </div>
  )HTML");
  LayoutObject* text_size_adjust_a =
      GetLayoutObjectByElementId("textSizeAdjustA");
  EXPECT_FLOAT_EQ(16.f, text_size_adjust_a->StyleRef().SpecifiedFontSize());
  // 16px * 47% = 7.52
  EXPECT_FLOAT_EQ(7.52f, text_size_adjust_a->StyleRef().ComputedFontSize());
  LayoutObject* text_size_adjust_b =
      GetLayoutObjectByElementId("textSizeAdjustB");
  EXPECT_FLOAT_EQ(16.f, text_size_adjust_b->StyleRef().SpecifiedFontSize());
  // 16px * 53% = 8.48
  EXPECT_FLOAT_EQ(8.48f, text_size_adjust_b->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, PrefixedTextSizeAdjustIsAlias) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='textSizeAdjust' style='-webkit-text-size-adjust: 50%;'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");
  LayoutObject* text_size_adjust = GetLayoutObjectByElementId("textSizeAdjust");
  EXPECT_FLOAT_EQ(16.f, text_size_adjust->StyleRef().SpecifiedFontSize());
  EXPECT_FLOAT_EQ(8.f, text_size_adjust->StyleRef().ComputedFontSize());
  EXPECT_FLOAT_EQ(
      .5f, text_size_adjust->StyleRef().GetTextSizeAdjust().Multiplier());
}

TEST_P(TextAutosizerTest, AccessibilityFontScaleFactor) {
  GetDocument().GetSettings()->SetAccessibilityFontScaleFactor(1.5);
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='autosized'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");
  Element* autosized = GetElementById("autosized");
  EXPECT_FLOAT_EQ(16.f,
                  autosized->GetLayoutObject()->StyleRef().SpecifiedFontSize());
  // 1.5 * (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 60px.
  EXPECT_FLOAT_EQ(60.f,
                  autosized->GetLayoutObject()->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, AccessibilityFontScaleFactorWithTextSizeAdjustNone) {
  if (RuntimeEnabledFeatures::TextSizeAdjustImprovementsEnabled()) {
    // Non-auto values of text-size-adjust should disable all automatic font
    // scale adjustment.
    return;
  }

  GetDocument().GetSettings()->SetAccessibilityFontScaleFactor(1.5);
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
      #autosized { width: 400px; text-size-adjust: 100%; }
      #notAutosized { width: 100px; text-size-adjust: 100%; }
    </style>
    <div id='autosized'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
    <div id='notAutosized'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");
  Element* autosized = GetElementById("autosized");
  EXPECT_FLOAT_EQ(16.f,
                  autosized->GetLayoutObject()->StyleRef().SpecifiedFontSize());
  // 1.5 * (specified font-size = 16px) = 24px.
  EXPECT_FLOAT_EQ(24.f,
                  autosized->GetLayoutObject()->StyleRef().ComputedFontSize());

  // Because this does not autosize (due to the width), no accessibility font
  // scale factor should be applied.
  Element* not_autosized = GetElementById("notAutosized");
  EXPECT_FLOAT_EQ(
      16.f, not_autosized->GetLayoutObject()->StyleRef().SpecifiedFontSize());
  // specified font-size = 16px.
  EXPECT_FLOAT_EQ(
      16.f, not_autosized->GetLayoutObject()->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, ChangingAccessibilityFontScaleFactor) {
  GetDocument().GetSettings()->SetAccessibilityFontScaleFactor(1);
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='autosized'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");
  Element* autosized = GetElementById("autosized");
  EXPECT_FLOAT_EQ(16.f,
                  autosized->GetLayoutObject()->StyleRef().SpecifiedFontSize());
  // 1.0 * (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 40px.
  EXPECT_FLOAT_EQ(40.f,
                  autosized->GetLayoutObject()->StyleRef().ComputedFontSize());

  GetDocument().GetSettings()->SetAccessibilityFontScaleFactor(2);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FLOAT_EQ(16.f,
                  autosized->GetLayoutObject()->StyleRef().SpecifiedFontSize());
  // 2.0 * (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 80px.
  EXPECT_FLOAT_EQ(80.f,
                  autosized->GetLayoutObject()->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, TextSizeAdjustDoesNotDisableAccessibility) {
  if (RuntimeEnabledFeatures::TextSizeAdjustImprovementsEnabled()) {
    // Non-auto values of text-size-adjust should disable all automatic font
    // scale adjustment.
    return;
  }

  GetDocument().GetSettings()->SetAccessibilityFontScaleFactor(1.5);
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='textSizeAdjustNone' style='text-size-adjust: none;'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
    <div id='textSizeAdjustDouble' style='text-size-adjust: 200%;'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");
  Element* text_size_adjust_none = GetElementById("textSizeAdjustNone");
  EXPECT_FLOAT_EQ(
      16.f,
      text_size_adjust_none->GetLayoutObject()->StyleRef().SpecifiedFontSize());
  // 1.5 * (specified font-size = 16px) = 24px.
  EXPECT_FLOAT_EQ(
      24.f,
      text_size_adjust_none->GetLayoutObject()->StyleRef().ComputedFontSize());

  Element* text_size_adjust_double = GetElementById("textSizeAdjustDouble");
  EXPECT_FLOAT_EQ(16.f, text_size_adjust_double->GetLayoutObject()
                            ->StyleRef()
                            .SpecifiedFontSize());
  // 1.5 * (specified font-size = 16px) * (text size adjustment = 2) = 48px.
  EXPECT_FLOAT_EQ(48.f, text_size_adjust_double->GetLayoutObject()
                            ->StyleRef()
                            .ComputedFontSize());

  // Changing the accessibility font scale factor should change the adjusted
  // size.
  GetDocument().GetSettings()->SetAccessibilityFontScaleFactor(2);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FLOAT_EQ(
      16.f,
      text_size_adjust_none->GetLayoutObject()->StyleRef().SpecifiedFontSize());
  // 2.0 * (specified font-size = 16px) = 32px.
  EXPECT_FLOAT_EQ(
      32.f,
      text_size_adjust_none->GetLayoutObject()->StyleRef().ComputedFontSize());

  EXPECT_FLOAT_EQ(16.f, text_size_adjust_double->GetLayoutObject()
                            ->StyleRef()
                            .SpecifiedFontSize());
  // 2.0 * (specified font-size = 16px) * (text size adjustment = 2) = 64px.
  EXPECT_FLOAT_EQ(64.f, text_size_adjust_double->GetLayoutObject()
                            ->StyleRef()
                            .ComputedFontSize());
}

// https://crbug.com/646237
TEST_P(TextAutosizerTest, DISABLED_TextSizeAdjustWithoutNeedingAutosizing) {
  GetDocument().GetSettings()->SetTextAutosizingWindowSizeOverride(
      gfx::Size(800, 600));
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='textSizeAdjust' style='text-size-adjust: 150%;'>
      Text
    </div>
  )HTML");

  LayoutObject* text_size_adjust = GetLayoutObjectByElementId("textSizeAdjust");
  EXPECT_FLOAT_EQ(16.f, text_size_adjust->StyleRef().SpecifiedFontSize());
  EXPECT_FLOAT_EQ(24.f, text_size_adjust->StyleRef().ComputedFontSize());
  EXPECT_FLOAT_EQ(
      1.5f, text_size_adjust->StyleRef().GetTextSizeAdjust().Multiplier());
}

TEST_P(TextAutosizerTest, DeviceScaleAdjustmentWithViewport) {
  SetBodyInnerHTML(R"HTML(
    <meta name='viewport' content='width=800'>
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='autosized'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");

  GetDocument().GetSettings()->SetViewportMetaEnabled(true);
  GetDocument().GetSettings()->SetDeviceScaleAdjustment(1.5f);
  UpdateAllLifecyclePhasesForTest();

  Element* autosized = GetElementById("autosized");
  EXPECT_FLOAT_EQ(16.f,
                  autosized->GetLayoutObject()->StyleRef().SpecifiedFontSize());
  // (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 40px.
  // The device scale adjustment of 1.5 is ignored.
  EXPECT_FLOAT_EQ(40.f,
                  autosized->GetLayoutObject()->StyleRef().ComputedFontSize());

  GetDocument().GetSettings()->SetViewportMetaEnabled(false);
  UpdateAllLifecyclePhasesForTest();

  autosized = GetElementById("autosized");
  EXPECT_FLOAT_EQ(16.f,
                  autosized->GetLayoutObject()->StyleRef().SpecifiedFontSize());
  // (device scale adjustment = 1.5) * (specified font-size = 16px) *
  // (viewport width = 800px) / (window width = 320px) = 60px.
  EXPECT_FLOAT_EQ(60.f,
                  autosized->GetLayoutObject()->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, ChangingSuperClusterFirstText) {
  SetBodyInnerHTML(R"HTML(
    <meta name='viewport' content='width=800'>
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
      .supercluster { width:560px; }
    </style>
    <div class='supercluster'>
      <div id='longText'>short blah blah</div>
    </div>
    <div class='supercluster'>
      <div id='shortText'>short blah blah</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* long_text_element = GetElementById("longText");
  long_text_element->setInnerHTML(
      "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed "
      "do eiusmod tempor"
      "    incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
      "veniam, quis nostrud"
      "    exercitation ullamco laboris nisi ut aliquip ex ea commodo "
      "consequat. Duis aute irure"
      "    dolor in reprehenderit in voluptate velit esse cillum dolore eu "
      "fugiat nulla pariatur."
      "    Excepteur sint occaecat cupidatat non proident, sunt in culpa "
      "qui officia deserunt"
      "    mollit anim id est laborum.",
      ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  LayoutObject* long_text = GetLayoutObjectByElementId("longText");
  EXPECT_FLOAT_EQ(16.f, long_text->StyleRef().SpecifiedFontSize());
  //(specified font-size = 16px) * (block width = 560px) /
  // (window width = 320px) = 28px.
  EXPECT_FLOAT_EQ(28.f, long_text->StyleRef().ComputedFontSize());
  LayoutObject* short_text = GetLayoutObjectByElementId("shortText");
  EXPECT_FLOAT_EQ(16.f, short_text->StyleRef().SpecifiedFontSize());
  EXPECT_FLOAT_EQ(28.f, short_text->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, ChangingSuperClusterSecondText) {
  SetBodyInnerHTML(R"HTML(
    <meta name='viewport' content='width=800'>
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
      .supercluster { width:560px; }
    </style>
    <div class='supercluster'>
      <div id='shortText'>short blah blah</div>
    </div>
    <div class='supercluster'>
      <div id='longText'>short blah blah</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* long_text_element = GetElementById("longText");
  long_text_element->setInnerHTML(
      "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed "
      "do eiusmod tempor"
      "    incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
      "veniam, quis nostrud"
      "    exercitation ullamco laboris nisi ut aliquip ex ea commodo "
      "consequat. Duis aute irure"
      "    dolor in reprehenderit in voluptate velit esse cillum dolore eu "
      "fugiat nulla pariatur."
      "    Excepteur sint occaecat cupidatat non proident, sunt in culpa "
      "qui officia deserunt"
      "    mollit anim id est laborum.",
      ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  LayoutObject* long_text = GetLayoutObjectByElementId("longText");
  EXPECT_FLOAT_EQ(16.f, long_text->StyleRef().SpecifiedFontSize());
  //(specified font-size = 16px) * (block width = 560px) /
  // (window width = 320px) = 28px.
  EXPECT_FLOAT_EQ(28.f, long_text->StyleRef().ComputedFontSize());
  LayoutObject* short_text = GetLayoutObjectByElementId("shortText");
  EXPECT_FLOAT_EQ(16.f, short_text->StyleRef().SpecifiedFontSize());
  EXPECT_FLOAT_EQ(28.f, short_text->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, AddingSuperCluster) {
  SetBodyInnerHTML(R"HTML(
    <meta name='viewport' content='width=800'>
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
      .supercluster { width:560px; }
    </style>
    <div>
      <div class='supercluster' id='shortText'>
          short blah blah
      </div>
    </div>
    <div id='container'></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* container = GetElementById("container");
  container->setInnerHTML(
      "<div class='supercluster' id='longText'>"
      "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed "
      "do eiusmod tempor"
      "    incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
      "veniam, quis nostrud"
      "    exercitation ullamco laboris nisi ut aliquip ex ea commodo "
      "consequat. Duis aute irure"
      "    dolor in reprehenderit in voluptate velit esse cillum dolore eu "
      "fugiat nulla pariatur."
      "    Excepteur sint occaecat cupidatat non proident, sunt in culpa "
      "qui officia deserunt"
      "    mollit anim id est laborum."
      "</div>",
      ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  LayoutObject* long_text = GetLayoutObjectByElementId("longText");
  EXPECT_FLOAT_EQ(16.f, long_text->StyleRef().SpecifiedFontSize());
  //(specified font-size = 16px) * (block width = 560px) /
  // (window width = 320px) = 28px.
  EXPECT_FLOAT_EQ(28.f, long_text->StyleRef().ComputedFontSize());
  LayoutObject* short_text = GetLayoutObjectByElementId("shortText");
  EXPECT_FLOAT_EQ(16.f, short_text->StyleRef().SpecifiedFontSize());
  EXPECT_FLOAT_EQ(28.f, short_text->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, ChangingInheritedClusterTextInsideSuperCluster) {
  SetBodyInnerHTML(R"HTML(
    <meta name='viewport' content='width=800'>
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
      .supercluster { width:560px; }
      .cluster{width:560px;}
    </style>
    <div class='supercluster'>
      <div class='cluster' id='longText'>short blah blah</div>
    </div>
    <div class='supercluster'>
      <div class='cluster' id='shortText'>short blah blah</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* long_text_element = GetElementById("longText");
  long_text_element->setInnerHTML(
      "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed "
      "do eiusmod tempor"
      "    incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
      "veniam, quis nostrud"
      "    exercitation ullamco laboris nisi ut aliquip ex ea commodo "
      "consequat. Duis aute irure"
      "    dolor in reprehenderit in voluptate velit esse cillum dolore eu "
      "fugiat nulla pariatur."
      "    Excepteur sint occaecat cupidatat non proident, sunt in culpa "
      "qui officia deserunt"
      "    mollit anim id est laborum.",
      ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  LayoutObject* long_text = GetLayoutObjectByElementId("longText");
  ;
  EXPECT_FLOAT_EQ(16.f, long_text->StyleRef().SpecifiedFontSize());
  //(specified font-size = 16px) * (block width = 560px) /
  // (window width = 320px) = 28px.
  EXPECT_FLOAT_EQ(28.f, long_text->StyleRef().ComputedFontSize());
  LayoutObject* short_text = GetLayoutObjectByElementId("shortText");
  EXPECT_FLOAT_EQ(16.f, short_text->StyleRef().SpecifiedFontSize());
  EXPECT_FLOAT_EQ(28.f, short_text->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, AutosizeInnerContentOfRuby) {
  SetBodyInnerHTML(R"HTML(
    <meta name='viewport' content='width=800'>
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='autosized'>
      東京特許許可局許可局長　今日
      <ruby>
        <rb id='rubyInline'>急遽</rb>
        <rp>(</rp>
        <rt>きゅうきょ</rt>
        <rp>)</rp>
      </ruby>
      許可却下、<br><br>
      <span>
          Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec
          sed diam facilisis, elementum elit at, elementum sem. Aliquam
          consectetur leo at nisi fermentum, vitae maximus libero
    sodales. Sed
          laoreet congue ipsum, at tincidunt ante tempor sed. Cras eget
    erat
          mattis urna vestibulum porta. Sed tempus vitae dui et suscipit.
          Curabitur laoreet accumsan pharetra. Nunc facilisis, elit sit
    amet
          sollicitudin condimentum, ipsum velit ultricies mi, eget
    dapibus nunc
          nulla nec sapien. Fusce dictum imperdiet aliquet.
      </span>
      <ruby style='display:block'>
        <rb id='rubyBlock'>拼音</rb>
        <rt>pin yin</rt>
      </ruby>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* ruby_inline = GetElementById("rubyInline");
  EXPECT_FLOAT_EQ(
      16.f, ruby_inline->GetLayoutObject()->StyleRef().SpecifiedFontSize());
  // (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 40px.
  EXPECT_FLOAT_EQ(
      40.f, ruby_inline->GetLayoutObject()->StyleRef().ComputedFontSize());

  Element* ruby_block = GetElementById("rubyBlock");
  EXPECT_FLOAT_EQ(
      16.f, ruby_block->GetLayoutObject()->StyleRef().SpecifiedFontSize());
  // (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 40px.
  EXPECT_FLOAT_EQ(40.f,
                  ruby_block->GetLayoutObject()->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, ResizeAndGlyphOverflowChanged) {
  GetDocument().GetSettings()->SetTextAutosizingWindowSizeOverride(
      gfx::Size(360, 640));
  Element* html = GetDocument().body()->parentElement();
  html->setInnerHTML(
      "<head>"
      "  <meta name='viewport' content='width=800'>"
      "  <style>"
      "    html { font-size:16px; font-family:'Times New Roman';}"
      "  </style>"
      "</head>"
      "<body>"
      "  <span id='autosized' style='font-size:10px'>"
      "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "    eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "    ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "    aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "    reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "    pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "    culpa qui officia deserunt mollit anim id est laborum."
      "  </span>"
      "  <span style='font-size:8px'>n</span>"
      "  <span style='font-size:9px'>n</span>"
      "  <span style='font-size:10px'>n</span>"
      "  <span style='font-size:11px'>n</span>"
      "  <span style='font-size:12px'>n</span>"
      "  <span style='font-size:13px'>n</span>"
      "  <span style='font-size:14px'>n</span>"
      "  <span style='font-size:15px'>n</span>"
      "</body>",
      ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  GetDocument().GetSettings()->SetTextAutosizingWindowSizeOverride(
      gfx::Size(640, 360));
  UpdateAllLifecyclePhasesForTest();

  GetDocument().GetSettings()->SetTextAutosizingWindowSizeOverride(
      gfx::Size(360, 640));
  UpdateAllLifecyclePhasesForTest();
}

TEST_P(TextAutosizerTest, narrowContentInsideNestedWideBlock) {
  Element* html = GetDocument().body()->parentElement();
  html->setInnerHTML(
      "<head>"
      "  <meta name='viewport' content='width=800'>"
      "  <style>"
      "    html { font-size:16px;}"
      "  </style>"
      "</head>"
      "<body>"
      "  <div style='width:800px'>"
      "    <div style='width:800px'>"
      "      <div style='width:200px' id='content'>"
      "        Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed "
      "        do eiusmod tempor incididunt ut labore et dolore magna aliqua."
      "        Ut enim ad minim veniam, quis nostrud exercitation ullamco "
      "        laboris nisi ut aliquip ex ea commodo consequat. Duis aute "
      "        irure dolor in reprehenderit in voluptate velit esse cillum "
      "        dolore eu fugiat nulla pariatur. Excepteur sint occaecat "
      "        cupidatat non proident, sunt in culpa qui officia deserunt "
      "        mollit anim id est laborum."
      "      </div>"
      "    </div>"
      "    Content belong to first wide block."
      "  </div>"
      "</body>",
      ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  Element* content = GetElementById("content");
  //(content width = 200px) / (window width = 320px) < 1.0f, multiplier = 1.0,
  // font-size = 16px;
  EXPECT_FLOAT_EQ(16.f,
                  content->GetLayoutObject()->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, LayoutViewWidthProvider) {
  Element* html = GetDocument().body()->parentElement();
  html->setInnerHTML(
      "<head>"
      "  <meta name='viewport' content='width=800'>"
      "  <style>"
      "    html { font-size:16px;}"
      "    #content {margin-left: 140px;}"
      "  </style>"
      "</head>"
      "<body>"
      "  <div id='content'>"
      "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "    eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "    ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "    aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "    reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "    pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "    culpa qui officia deserunt mollit anim id est laborum."
      "  </div>"
      "  <div id='panel'></div>"
      "</body>",
      ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  Element* content = GetElementById("content");
  // (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 40px.
  EXPECT_FLOAT_EQ(40.f,
                  content->GetLayoutObject()->StyleRef().ComputedFontSize());

  GetElementById("panel")->setInnerHTML("insert text");
  content->setInnerHTML(content->innerHTML());
  UpdateAllLifecyclePhasesForTest();

  // (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 40px.
  EXPECT_FLOAT_EQ(40.f,
                  content->GetLayoutObject()->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, MultiColumns) {
  Element* html = GetDocument().body()->parentElement();
  html->setInnerHTML(
      "<head>"
      "  <meta name='viewport' content='width=800'>"
      "  <style>"
      "    html { font-size:16px;}"
      "    #mc {columns: 3;}"
      "  </style>"
      "</head>"
      "<body>"
      "  <div id='mc'>"
      "    <div id='target'>"
      "      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed "
      "      do eiusmod tempor incididunt ut labore et dolore magna aliqua."
      "      Ut enim ad minim veniam, quis nostrud exercitation ullamco "
      "      laboris nisi ut aliquip ex ea commodo consequat. Duis aute "
      "      irure dolor in reprehenderit in voluptate velit esse cillum "
      "      dolore eu fugiat nulla pariatur. Excepteur sint occaecat "
      "      cupidatat non proident, sunt in culpa qui officia deserunt "
      "    </div>"
      "  </div>"
      "  <div> hello </div>"
      "</body>",
      ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetElementById("target");
  // (specified font-size = 16px) * ( thread flow layout width = 800px / 3) /
  // (window width = 320px) < 16px.
  EXPECT_FLOAT_EQ(16.f,
                  target->GetLayoutObject()->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, MultiColumns2) {
  Element* html = GetDocument().body()->parentElement();
  html->setInnerHTML(
      "<head>"
      "  <meta name='viewport' content='width=800'>"
      "  <style>"
      "    html { font-size:16px;}"
      "    #mc {columns: 3; column-gap: 0;}"
      "  </style>"
      "</head>"
      "<body>"
      "  <div id='mc'>"
      "    <div id='target1'>"
      "      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed "
      "      do eiusmod tempor incididunt ut labore et dolore magna aliqua."
      "      Ut enim ad minim veniam, quis nostrud exercitation ullamco "
      "      laboris nisi ut aliquip ex ea commodo consequat. Duis aute "
      "      irure dolor in reprehenderit in voluptate velit esse cillum "
      "      dolore eu fugiat nulla pariatur. Excepteur sint occaecat "
      "      cupidatat non proident, sunt in culpa qui officia deserunt "
      "    </div>"
      "    <div id='target2'>"
      "      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed "
      "      do eiusmod tempor incididunt ut labore et dolore magna aliqua."
      "      Ut enim ad minim veniam, quis nostrud exercitation ullamco "
      "      laboris nisi ut aliquip ex ea commodo consequat. Duis aute "
      "      irure dolor in reprehenderit in voluptate velit esse cillum "
      "      dolore eu fugiat nulla pariatur. Excepteur sint occaecat "
      "      cupidatat non proident, sunt in culpa qui officia deserunt "
      "    </div>"
      "  </div>"
      "  <div> hello </div>"
      "</body>",
      ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  Element* target1 = GetElementById("target1");
  Element* target2 = GetElementById("target2");
  // (specified font-size = 16px) * ( column width = 800px / 3) /
  // (window width = 320px) < 16px.
  EXPECT_FLOAT_EQ(16.f,
                  target1->GetLayoutObject()->StyleRef().ComputedFontSize());
  EXPECT_FLOAT_EQ(16.f,
                  target2->GetLayoutObject()->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, ScaledbyDSF) {
  const float device_scale = 3;
  set_device_scale_factor(device_scale);
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
      .target { width: 560px; }
    </style>
    <body>
      <div id='target'>
        Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed
        do eiusmod tempor incididunt ut labore et dolore magna aliqua.
        Ut enim ad minim veniam, quis nostrud exercitation ullamco
        laboris nisi ut aliquip ex ea commodo consequat. Duis aute
        irure dolor in reprehenderit in voluptate velit esse cillum
        dolore eu fugiat nulla pariatur. Excepteur sint occaecat
        cupidatat non proident, sunt in culpa qui officia deserunt
      </div>
    </body>
  )HTML");
  Element* target = GetElementById("target");
  // (specified font-size = 16px) * (thread flow layout width = 800px) /
  // (window width = 320px) * (device scale factor) = 40px * device_scale.
  EXPECT_FLOAT_EQ(40.0f * device_scale,
                  target->GetLayoutObject()->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, ClusterHasNotEnoughTextToAutosizeForZoomDSF) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 8px; }
    </style>
    <body>
      <div id='target'>
        Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed
        do eiusmod tempor incididunt ut labore et dolore magna aliqua.
        Ut enim ad minim veniam, quis nostrud exercitation ullamco
        laboris nisi ut aliquip ex ea commodo consequat.
      </div>
    </body>
  )HTML");
  Element* target = GetElementById("target");
  // ClusterHasEnoughTextToAutosize() returns false because
  // minimum_text_length_to_autosize < length. Thus, ClusterMultiplier()
  // returns 1 (not multiplied by the accessibility font scale factor).
  // computed font-size = specified font-size = 8px.
  EXPECT_FLOAT_EQ(8.0f,
                  target->GetLayoutObject()->StyleRef().ComputedFontSize());
}

// TODO(jaebaek): Unit tests ClusterHasNotEnoughTextToAutosizeForZoomDSF and
// ClusterHasEnoughTextToAutosizeForZoomDSF must be updated.
// The return value of TextAutosizer::ClusterHasEnoughTextToAutosize() must not
// be the same regardless of DSF. In real world
// TextAutosizer::ClusterHasEnoughTextToAutosize(),
// minimum_text_length_to_autosize is in physical pixel scale. However, in
// these unit tests, it is in DIP scale, which makes
// ClusterHasEnoughTextToAutosizeForZoomDSF not fail. We need a trick to update
// the minimum_text_length_to_autosize in these unit test and check the return
// value change of TextAutosizer::ClusterHasEnoughTextToAutosize() depending on
// the length of text even when DSF is not 1 (e.g., letting DummyPageHolder
// update the view size according to the change of DSF).
TEST_P(TextAutosizerTest, ClusterHasEnoughTextToAutosizeForZoomDSF) {
  const float device_scale = 3;
  set_device_scale_factor(device_scale);
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 8px; }
    </style>
    <body>
      <div id='target'>
        Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed
        do eiusmod tempor incididunt ut labore et dolore magna aliqua.
        Ut enim ad minim veniam, quis nostrud exercitation ullamco
        laboris nisi ut aliquip ex ea commodo consequat.
      </div>
    </body>
  )HTML");
  Element* target = GetElementById("target");
  // (specified font-size = 8px) * (thread flow layout width = 800px) /
  // (window width = 320px) * (device scale factor) = 20px * device_scale.
  // ClusterHasEnoughTextToAutosize() returns true and both accessibility font
  // scale factor and device scale factor are multiplied.
  EXPECT_FLOAT_EQ(20.0f * device_scale,
                  target->GetLayoutObject()->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, AfterPrint) {
  const float device_scale = 3;
  gfx::SizeF print_size(160, 240);
  set_device_scale_factor(device_scale);
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 8px; }
    </style>
    <body>
      <div id='target'>
        Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed
        do eiusmod tempor incididunt ut labore et dolore magna aliqua.
        Ut enim ad minim veniam, quis nostrud exercitation ullamco
        laboris nisi ut aliquip ex ea commodo consequat.
      </div>
    </body>
  )HTML");
  Element* target = GetElementById("target");
  EXPECT_FLOAT_EQ(20.0f * device_scale,
                  target->GetLayoutObject()->StyleRef().ComputedFontSize());
  GetDocument().GetFrame()->StartPrinting(WebPrintParams(print_size));
  EXPECT_FLOAT_EQ(8.0f,
                  target->GetLayoutObject()->StyleRef().ComputedFontSize());
  GetDocument().GetFrame()->EndPrinting();
  EXPECT_FLOAT_EQ(20.0f * device_scale,
                  target->GetLayoutObject()->StyleRef().ComputedFontSize());
}

TEST_P(TextAutosizerTest, FingerprintWidth) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 8px; }
      #target { width: calc(1px); }
    </style>
    <body>
      <div id='target'>
        Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed
        do eiusmod tempor incididunt ut labore et dolore magna aliqua.
        Ut enim ad minim veniam, quis nostrud exercitation ullamco
        laboris nisi ut aliquip ex ea commodo consequat.
      </div>
    </body>
  )HTML");
  // The test pass if it doesn't crash nor hit DCHECK.
}

// Test that `kUsedDeviceScaleAdjustment` is not recorded when a user-specified
// meta viewport is present on the outermost main frame.
TEST_P(TextAutosizerTest, UsedDeviceScaleAdjustmentUseCounterWithViewport) {
  SetBodyInnerHTML(R"HTML(
    <meta name="viewport" content="width=device-width">
    <div style="font-size: 20px">
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod
      tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim
      veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea
      commodo consequat.
    </div>
    <iframe></iframe>
  )HTML");
  GetDocument().GetSettings()->SetViewportMetaEnabled(true);
  GetDocument().GetSettings()->SetDeviceScaleAdjustment(1.5f);
  // Do not specify a meta viewport in the subframe. If the subframe's lack of a
  // meta viewport were used, it would incorrectly cause the device scale
  // adjustment to be used.
  SetChildFrameHTML(R"HTML(
    <div style="font-size: 20px">
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod
      tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim
      veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea
      commodo consequat.
    </div>
  )HTML");
  ASSERT_TRUE(ChildDocument().GetSettings()->GetViewportMetaEnabled());
  ASSERT_EQ(1.5f, ChildDocument().GetSettings()->GetDeviceScaleAdjustment());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kUsedDeviceScaleAdjustment));
  EXPECT_FALSE(
      ChildDocument().IsUseCounted(WebFeature::kUsedDeviceScaleAdjustment));
}

// Test that `kUsedDeviceScaleAdjustment` is recorded when a user-specified meta
// viewport is not specified on the outermost main frame.
TEST_P(TextAutosizerTest, UsedDeviceScaleAdjustmentUseCounterWithoutViewport) {
  SetBodyInnerHTML(R"HTML(
    <div style="font-size: 20px">
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod
      tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim
      veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea
      commodo consequat.
    </div>
    <iframe></iframe>
  )HTML");
  // We should not record the metric before setting the viewport settings.
  ASSERT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kUsedDeviceScaleAdjustment));
  GetDocument().GetSettings()->SetViewportMetaEnabled(true);
  GetDocument().GetSettings()->SetDeviceScaleAdjustment(1.5f);
  // Specify a meta viewport in the subframe. If the subframe's meta viewport
  // were used, it would incorrectly prevent the device scale adjustment from
  // being used.
  SetChildFrameHTML(R"HTML(
    <meta name="viewport" content="width=device-width">
    <div style="font-size: 20px">
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod
      tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim
      veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea
      commodo consequat.
    </div>
  )HTML");
  ASSERT_TRUE(ChildDocument().GetSettings()->GetViewportMetaEnabled());
  ASSERT_EQ(1.5f, ChildDocument().GetSettings()->GetDeviceScaleAdjustment());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kUsedDeviceScaleAdjustment));
  EXPECT_TRUE(
      ChildDocument().IsUseCounted(WebFeature::kUsedDeviceScaleAdjustment));
}

class TextAutosizerSimTest : public SimTest,
                             public testing::WithParamInterface<bool>,
                             private ScopedTextSizeAdjustImprovementsForTest {
 public:
  TextAutosizerSimTest()
      : ScopedTextSizeAdjustImprovementsForTest(GetParam()) {}

 private:
  void SetUp() override {
    SimTest::SetUp();

    WebSettings* web_settings = WebView().GetSettings();
    web_settings->SetViewportEnabled(true);
    web_settings->SetViewportMetaEnabled(true);

    Settings& settings = WebView().GetPage()->GetSettings();
    settings.SetTextAutosizingEnabled(true);
    settings.SetTextAutosizingWindowSizeOverride(gfx::Size(400, 400));
    settings.SetDeviceScaleAdjustment(1.5f);
  }
};

INSTANTIATE_TEST_SUITE_P(All, TextAutosizerSimTest, testing::Bool());

TEST_P(TextAutosizerSimTest, CrossSiteUseCounter) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 800));

  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest child_resource("https://crosssite.com/", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete(
      "<iframe width=700 src='https://crosssite.com/'></iframe>");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  child_resource.Complete(R"HTML(
    <body style='font-size: 20px'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed
      do eiusmod tempor incididunt ut labore et dolore magna aliqua.
      Ut enim ad minim veniam, quis nostrud exercitation ullamco
      laboris nisi ut aliquip ex ea commodo consequat.
    </body>
  )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  auto* child_frame = To<WebLocalFrameImpl>(MainFrame().FirstChild());
  auto* child_doc = child_frame->GetFrame()->GetDocument();

  EXPECT_TRUE(
      child_doc->IsUseCounted(WebFeature::kTextAutosizedCrossSiteIframe));
}

TEST_P(TextAutosizerSimTest, ViewportChangesUpdateAutosizing) {
  if (!RuntimeEnabledFeatures::ViewportChangesUpdateTextAutosizingEnabled()) {
    GTEST_SKIP();
  }

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <meta>
    <style>
      html { font-size: 16px; }
      body { width: 320px; margin: 0; overflow-y: hidden; }
    </style>
    <div>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(GetDocument()
                   .GetViewportData()
                   .GetViewportDescription()
                   .IsSpecifiedByAuthor());

  // The page should autosize because a meta viewport is not specified.
  auto* div = GetDocument().QuerySelector(AtomicString("div"));
  EXPECT_FLOAT_EQ(18.f, div->GetLayoutObject()->StyleRef().ComputedFontSize());

  Element* meta = GetDocument().QuerySelector(AtomicString("meta"));
  meta->setAttribute(html_names::kNameAttr, AtomicString("viewport"));
  meta->setAttribute(html_names::kContentAttr,
                     AtomicString("width=device-width"));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // The page should no longer autosize because a meta viewport is specified.
  EXPECT_FLOAT_EQ(16.f, div->GetLayoutObject()->StyleRef().ComputedFontSize());
}

}  // namespace blink
