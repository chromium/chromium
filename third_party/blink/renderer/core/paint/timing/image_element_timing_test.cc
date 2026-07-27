// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/image_element_timing.h"

#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/timing/media_record_id.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_test_base.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"

namespace blink {

namespace internal {
extern bool IsExplicitlyRegisteredForElementTiming(
    const LayoutObject& layout_object);
}

class ImageElementTimingTest : public PaintTimingTestBase,
                               public PaintTestConfigurations {
 protected:
  // Sets an image resource for the LayoutImage with the given |id| and return
  // the LayoutImage.
  LayoutImage* SetImageResource(const char* id, int width, int height) {
    ImageResourceContent* content = CreateImageForTest(width, height);
    if (auto* layout_image = DynamicTo<LayoutImage>(GetLayoutObjectById(id))) {
      layout_image->ImageResource()->SetImageResource(content);
      return layout_image;
    }
    return nullptr;
  }

  // Similar to above but for a LayoutSVGImage.
  LayoutSVGImage* SetSVGImageResource(const char* id, int width, int height) {
    ImageResourceContent* content = CreateImageForTest(width, height);
    if (auto* layout_image =
            DynamicTo<LayoutSVGImage>(GetLayoutObjectById(id))) {
      layout_image->ImageResource()->SetImageResource(content);
      return layout_image;
    }
    return nullptr;
  }

  bool ImagesNotifiedContains(MediaRecordIdHash record_id_hash) {
    return ImageElementTiming::From(*GetDocument().domWindow())
        .images_notified_.Contains(record_id_hash);
  }

  unsigned ImagesNotifiedSize() {
    return ImageElementTiming::From(*GetDocument().domWindow())
        .images_notified_.size();
  }

  LayoutObject* GetLayoutObjectById(const char* id) {
    Element* element = GetElementById(id);
    return element ? element->GetLayoutObject() : nullptr;
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(ImageElementTimingTest);

TEST_P(ImageElementTimingTest, TestIsExplicitlyRegisteredForElementTiming) {
  SetBodyInnerHTML(R"HTML(
    <img id="missing-attribute" style='width: 100px; height: 100px;'/>
    <img id="unset-attribute" elementtiming
         style='width: 100px; height: 100px;'/>
    <img id="empty-attribute" elementtiming=""
         style='width: 100px; height: 100px;'/>
    <img id="valid-attribute" elementtiming="valid-id"
         style='width: 100px; height: 100px;'/>
  )HTML");

  LayoutObject* without_attribute = GetLayoutObjectById("missing-attribute");
  bool actual =
      internal::IsExplicitlyRegisteredForElementTiming(*without_attribute);
  EXPECT_FALSE(actual) << "Nodes without an 'elementtiming' attribute should "
                          "not be explicitly registered.";

  LayoutObject* with_undefined_attribute =
      GetLayoutObjectById("unset-attribute");
  actual = internal::IsExplicitlyRegisteredForElementTiming(
      *with_undefined_attribute);
  EXPECT_TRUE(actual) << "Nodes with undefined 'elementtiming' attribute "
                         "should be explicitly registered.";

  LayoutObject* with_empty_attribute = GetLayoutObjectById("empty-attribute");
  actual =
      internal::IsExplicitlyRegisteredForElementTiming(*with_empty_attribute);
  EXPECT_TRUE(actual) << "Nodes with an empty 'elementtiming' attribute "
                         "should be explicitly registered.";

  LayoutObject* with_explicit_element_timing =
      GetLayoutObjectById("valid-attribute");
  actual = internal::IsExplicitlyRegisteredForElementTiming(
      *with_explicit_element_timing);
  EXPECT_TRUE(actual) << "Nodes with a non-empty 'elementtiming' attribute "
                         "should be explicitly registered.";
}

TEST_P(ImageElementTimingTest, IgnoresUnmarkedElement) {
  // Tests that, if the 'elementtiming' attribute is missing, the element isn't
  // considered by ImageElementTiming.
  SetBodyInnerHTML(R"HTML(
    <img id="target" style='width: 100px; height: 100px;'/>
  )HTML");
  LayoutImage* layout_image = SetImageResource("target", 5, 5);
  ASSERT_TRUE(layout_image);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ImagesNotifiedContains(
      MediaRecordId::GenerateHash(layout_image, layout_image->CachedImage())));
}

TEST_P(ImageElementTimingTest, ImageInsideSVG) {
  SetBodyInnerHTML(R"HTML(
    <svg>
      <foreignObject width="100" height="100">
        <img elementtiming="image-inside-svg" id="target"
             style='width: 100px; height: 100px;'/>
      </foreignObject>
    </svg>
  )HTML");
  LayoutImage* layout_image = SetImageResource("target", 5, 5);
  ASSERT_TRUE(layout_image);
  UpdateAllLifecyclePhasesForTest();

  // |layout_image| should have had its paint notified to ImageElementTiming.
  EXPECT_TRUE(ImagesNotifiedContains(
      MediaRecordId::GenerateHash(layout_image, layout_image->CachedImage())));
}

TEST_P(ImageElementTimingTest, ImageInsideNonRenderedSVG) {
  SetBodyInnerHTML(R"HTML(
    <svg mask="url(#mask)">
      <mask id="mask">
        <foreignObject width="100" height="100">
          <img elementtiming="image-inside-svg" id="target"
               style='width: 100px; height: 100px;'/>
        </foreignObject>
      </mask>
      <rect width="100" height="100" fill="green"/>
    </svg>
  )HTML");

  // HTML inside foreignObject in a non-rendered SVG subtree should not generate
  // layout objects. Generating layout objects for caused crashes
  // (crbug.com/905850) as well as correctness issues.
  EXPECT_FALSE(GetLayoutObjectById("target"));
}

TEST_P(ImageElementTimingTest, ImageRemoved) {
  SetBodyInnerHTML(R"HTML(
    <img elementtiming="will-be-removed" id="target"
         style='width: 100px; height: 100px;'/>
  )HTML");
  LayoutImage* layout_image = SetImageResource("target", 5, 5);
  ASSERT_TRUE(layout_image);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(ImagesNotifiedContains(
      MediaRecordId::GenerateHash(layout_image, layout_image->CachedImage())));

  GetDocument().getElementById(AtomicString("target"))->remove();
  // |layout_image| should no longer be part of |images_notified| since it will
  // be destroyed.
  EXPECT_EQ(ImagesNotifiedSize(), 0u);
}

TEST_P(ImageElementTimingTest, SVGImageRemoved) {
  SetBodyInnerHTML(R"HTML(
    <svg>
      <image elementtiming="svg-will-be-removed" id="target"
             style='width: 100px; height: 100px;'/>
    </svg>
  )HTML");
  LayoutSVGImage* layout_image = SetSVGImageResource("target", 5, 5);
  ASSERT_TRUE(layout_image);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(ImagesNotifiedContains(MediaRecordId::GenerateHash(
      layout_image, layout_image->ImageResource()->CachedImage())));

  GetDocument().getElementById(AtomicString("target"))->remove();
  // |layout_image| should no longer be part of |images_notified| since it will
  // be destroyed.
  EXPECT_EQ(ImagesNotifiedSize(), 0u);
}

TEST_P(ImageElementTimingTest, BackgroundImageRemoved) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        width: 100px;
        height: 100px;
        background: url(data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==);
      }
    </style>
    <div elementtiming="time-my-background-image" id="target"></div>
  )HTML");
  LayoutObject* object = GetLayoutObjectById("target");
  ImageResourceContent* content =
      object->StyleRef().BackgroundLayers().GetImage()->CachedImage();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(ImagesNotifiedSize(), 1u);
  EXPECT_TRUE(
      ImagesNotifiedContains(MediaRecordId::GenerateHash(object, content)));

  GetDocument().getElementById(AtomicString("target"))->remove();
  EXPECT_EQ(ImagesNotifiedSize(), 0u);
}

}  // namespace blink
