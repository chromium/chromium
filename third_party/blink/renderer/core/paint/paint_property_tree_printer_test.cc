// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_property_tree_printer.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"

#if DCHECK_IS_ON()

namespace blink {

class PaintPropertyTreePrinterTest : public PaintControllerPaintTest {
 public:
  PaintPropertyTreePrinterTest()
      : PaintControllerPaintTest(
            MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

 private:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(PaintPropertyTreePrinterTest);

TEST_P(PaintPropertyTreePrinterTest, SimpleTransformTree) {
  SetBodyInnerHTML("hello world");
  String transform_tree_as_string =
      TransformPropertyTreeAsString(*GetDocument().View());
  EXPECT_THAT(transform_tree_as_string.Ascii(),
              testing::MatchesRegex("root .*"
                                    "  .*Translation \\(.*\\) .*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleClipTree) {
  SetBodyInnerHTML("hello world");
  String clip_tree_as_string = ClipPropertyTreeAsString(*GetDocument().View());
  EXPECT_THAT(clip_tree_as_string.Ascii().c_str(),
              testing::MatchesRegex("root .*"
                                    "  .*Clip \\(.*\\) .*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleEffectTree) {
  SetBodyInnerHTML("<div style='opacity: 0.9;'>hello world</div>");
  String effect_tree_as_string =
      EffectPropertyTreeAsString(*GetDocument().View());
  EXPECT_THAT(
      effect_tree_as_string.Ascii().c_str(),
      testing::MatchesRegex(
          "root .*"
          "  Effect \\(LayoutN?G?BlockFlow \\(children-inline\\) DIV\\) .*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleScrollTree) {
  SetBodyInnerHTML("<div style='height: 4000px;'>hello world</div>");
  String scroll_tree_as_string =
      ScrollPropertyTreeAsString(*GetDocument().View());
  EXPECT_THAT(scroll_tree_as_string.Ascii().c_str(),
              testing::MatchesRegex("root .*"
                                    "  Scroll \\(.*\\) .*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleTransformTreePath) {
  SetBodyInnerHTML(
      "<div id='transform' style='transform: translate3d(10px, 10px, 10px);'>"
      "</div>");
  LayoutObject* transformed_object =
      GetDocument()
          .getElementById(AtomicString("transform"))
          ->GetLayoutObject();
  const auto* transformed_object_properties =
      transformed_object->FirstFragment().PaintProperties();
  String transform_path_as_string =
      transformed_object_properties->Transform()->ToTreeString();
  EXPECT_THAT(transform_path_as_string.Ascii().c_str(),
              testing::MatchesRegex("root .*\"scroll\".*"
                                    "  .*\"in_subtree_of_page_scale\".*"
                                    "    .*\"translation2d\".*"
                                    "      .*\"matrix\".*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleClipTreePath) {
  SetBodyInnerHTML(
      "<div id='clip' style='position: absolute; clip: rect(10px, 80px, 70px, "
      "40px);'></div>");
  LayoutObject* clipped_object =
      GetDocument().getElementById(AtomicString("clip"))->GetLayoutObject();
  const auto* clipped_object_properties =
      clipped_object->FirstFragment().PaintProperties();
  String clip_path_as_string =
      clipped_object_properties->CssClip()->ToTreeString();
  EXPECT_THAT(clip_path_as_string.Ascii().c_str(),
              testing::MatchesRegex("root .*\"rect\".*"
                                    "  .*\"rect\".*"
                                    "    .*\"rect\".*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleEffectTreePath) {
  SetBodyInnerHTML("<div id='effect' style='opacity: 0.9;'></div>");
  LayoutObject* effect_object =
      GetDocument().getElementById(AtomicString("effect"))->GetLayoutObject();
  const auto* effect_object_properties =
      effect_object->FirstFragment().PaintProperties();
  String effect_path_as_string =
      effect_object_properties->Effect()->ToTreeString();
  EXPECT_THAT(effect_path_as_string.Ascii().c_str(),
              testing::MatchesRegex("root .*\"outputClip\".*"
                                    "  .*\"opacity\".*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleScrollTreePath) {
  SetBodyInnerHTML(R"HTML(
    <div id='scroll' style='overflow: scroll; height: 100px;'>
      <div id='forceScroll' style='height: 4000px;'></div>
    </div>
  )HTML");
  LayoutObject* scroll_object =
      GetDocument().getElementById(AtomicString("scroll"))->GetLayoutObject();
  const auto* scroll_object_properties =
      scroll_object->FirstFragment().PaintProperties();
  String scroll_path_as_string = scroll_object_properties->ScrollTranslation()
                                     ->ScrollNode()
                                     ->ToTreeString();
  EXPECT_THAT(scroll_path_as_string.Ascii().c_str(),
              testing::MatchesRegex("root .*"
                                    "  Scroll.*"));
}

}  // namespace blink

#endif  // if DCHECK_IS_ON()
