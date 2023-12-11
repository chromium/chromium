// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_contrast.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class InspectorContrastTest : public testing::Test {
 protected:
  void SetUp() override;

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

 private:
  test::TaskEnvironment task_environment_;

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void InspectorContrastTest::SetUp() {
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
}

TEST_F(InspectorContrastTest, GetBackgroundColors) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id="target" style="color: white; background-color: red;">
      test
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Element* target = GetDocument().getElementById(AtomicString("target"));
  InspectorContrast contrast(&GetDocument());
  float fg_opacity = 1.0f;
  Vector<Color> colors = contrast.GetBackgroundColors(target, &fg_opacity);
  EXPECT_EQ(1u, colors.size());
  EXPECT_EQ("rgb(255, 0, 0)", colors.at(0).SerializeAsCSSColor());
  EXPECT_EQ(1.0f, fg_opacity);
}

TEST_F(InspectorContrastTest, GetBackgroundColorsNoText) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <!-- No text -->
    <div class="testCase noText">
      <div class="layer">
        <p id="target"></p>
      </div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Element* target = GetDocument().getElementById(AtomicString("target"));
  InspectorContrast contrast(&GetDocument());
  float fg_opacity = 1.0f;
  Vector<Color> colors = contrast.GetBackgroundColors(target, &fg_opacity);
  EXPECT_EQ(0u, colors.size());
  EXPECT_EQ(1.0f, fg_opacity);
}

TEST_F(InspectorContrastTest, GetBackgroundColorsBgOpacity) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div style="position: relative">
      <div style="position: absolute; width: 100px; height: 100px; background-color: black; opacity: 0.1;"></div>
      <div id="target" style="position: absolute; width: 100px; height: 100px; color: black;">test</div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Element* target = GetDocument().getElementById(AtomicString("target"));
  InspectorContrast contrast(&GetDocument());
  float fg_opacity = 1.0f;
  Vector<Color> colors = contrast.GetBackgroundColors(target, &fg_opacity);
  EXPECT_EQ(1u, colors.size());
  EXPECT_EQ("rgb(229, 229, 229)", colors.at(0).SerializeAsCSSColor());
  EXPECT_EQ(1.0f, fg_opacity);
}

TEST_F(InspectorContrastTest, GetBackgroundColorsBgOpacityParent) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div style="background-color: black; opacity: 0.1;">
      <div id="target" style="color: black;">test</div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Element* target = GetDocument().getElementById(AtomicString("target"));
  InspectorContrast contrast(&GetDocument());
  float fg_opacity = 1.0f;
  Vector<Color> colors = contrast.GetBackgroundColors(target, &fg_opacity);
  EXPECT_EQ(1u, colors.size());
  EXPECT_EQ("rgb(229, 229, 229)", colors.at(0).SerializeAsCSSColor());
  EXPECT_EQ(0.1f, fg_opacity);
}

TEST_F(InspectorContrastTest, GetBackgroundColorsElementWithOpacity) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id="target" style="opacity: 0.1; color: black;">test</div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Element* target = GetDocument().getElementById(AtomicString("target"));
  InspectorContrast contrast(&GetDocument());
  float fg_opacity = 1.0f;
  Vector<Color> colors = contrast.GetBackgroundColors(target, &fg_opacity);
  EXPECT_EQ(1u, colors.size());
  EXPECT_EQ("rgb(255, 255, 255)", colors.at(0).SerializeAsCSSColor());
  EXPECT_EQ(0.1f, fg_opacity);
}

TEST_F(InspectorContrastTest, GetBackgroundColorsBgHidden) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div style="position: relative">
      <div style="position: absolute; width: 100px; height: 100px; background-color: black; visibility: hidden;"></div>
      <div id="target" style="position: absolute; width: 100px; height: 100px; color: black;">test</div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Element* target = GetDocument().getElementById(AtomicString("target"));
  InspectorContrast contrast(&GetDocument());
  float fg_opacity = 1.0f;
  Vector<Color> colors = contrast.GetBackgroundColors(target, &fg_opacity);
  EXPECT_EQ(1u, colors.size());
  EXPECT_EQ("rgb(255, 255, 255)", colors.at(0).SerializeAsCSSColor());
  EXPECT_EQ(1.0f, fg_opacity);
}

TEST_F(InspectorContrastTest, GetBackgroundColorsWithOpacity) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div style="background-color: rgba(0,0,0,0.75);">
      <div style="background-color: rgba(0,0,0,0.75);">
        <div id="target" style="color: white; background-color: rgba(0,0,0,0.75);">
          test
        </div>
      </div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Element* target = GetDocument().getElementById(AtomicString("target"));
  InspectorContrast contrast(&GetDocument());
  float fg_opacity = 1.0f;
  Vector<Color> colors = contrast.GetBackgroundColors(target, &fg_opacity);
  EXPECT_EQ(1u, colors.size());
  EXPECT_EQ("rgb(4, 4, 4)", colors.at(0).SerializeAsCSSColor());
  EXPECT_EQ(1.0f, fg_opacity);
}

TEST_F(InspectorContrastTest, GetContrast) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id="target1" style="color: red; background-color: red;">
      test
    </div>
    <div id="target2" style="color: hsla(200,0%,0%,0.701960784313725); background-color: white;">
      test
    </div>
    <div id="target3" style="color: black; opacity: 0.1;">
      test
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  InspectorContrast contrast(&GetDocument());
  ContrastInfo contrast_info_1 = contrast.GetContrast(
      GetDocument().getElementById(AtomicString("target1")));
  EXPECT_EQ(true, contrast_info_1.able_to_compute_contrast);
  EXPECT_EQ(4.5, contrast_info_1.threshold_aa);
  EXPECT_EQ(7.0, contrast_info_1.threshold_aaa);
  EXPECT_FLOAT_EQ(1, contrast_info_1.contrast_ratio);
  ContrastInfo contrast_info_2 = contrast.GetContrast(
      GetDocument().getElementById(AtomicString("target3")));
  EXPECT_EQ(true, contrast_info_2.able_to_compute_contrast);
  EXPECT_EQ(4.5, contrast_info_2.threshold_aa);
  EXPECT_EQ(7.0, contrast_info_2.threshold_aaa);
  EXPECT_NEAR(1.25, contrast_info_2.contrast_ratio, 0.01);
}

TEST_F(InspectorContrastTest, GetContrastEmptyNodes) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id="target1" style="color: red; background-color: red;">	 </div>
    <div id="target2" style="color: red; background-color: red;"></div>
    <div id="target3" style="color: red; background-color: red;">

    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  InspectorContrast contrast(&GetDocument());
  ContrastInfo contrast_info_1 = contrast.GetContrast(
      GetDocument().getElementById(AtomicString("target1")));
  EXPECT_EQ(false, contrast_info_1.able_to_compute_contrast);
  ContrastInfo contrast_info_2 = contrast.GetContrast(
      GetDocument().getElementById(AtomicString("target2")));
  EXPECT_EQ(false, contrast_info_2.able_to_compute_contrast);
  ContrastInfo contrast_info_3 = contrast.GetContrast(
      GetDocument().getElementById(AtomicString("target3")));
  EXPECT_EQ(false, contrast_info_3.able_to_compute_contrast);
}

TEST_F(InspectorContrastTest, GetContrastMultipleNodes) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id="target1" style="color: red; background-color: red;">
      A <i>B</i>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  InspectorContrast contrast(&GetDocument());
  ContrastInfo contrast_info_1 = contrast.GetContrast(
      GetDocument().getElementById(AtomicString("target1")));
  EXPECT_EQ(false, contrast_info_1.able_to_compute_contrast);
}

}  // namespace blink
