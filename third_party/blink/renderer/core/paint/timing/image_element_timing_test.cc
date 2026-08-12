// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/image_element_timing.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/timing/media_record_id.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_test_base.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/performance_element_timing.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"

using testing::ElementsAre;
using testing::IsEmpty;

namespace blink {

namespace internal {
extern bool IsExplicitlyRegisteredForElementTiming(
    const LayoutObject& layout_object);
}

namespace {

// Simple matcher for matching the id of a `PerformanceElementTiming` entry.
MATCHER_P(ForId, id, "") {
  CHECK_EQ(arg->EntryTypeEnum(), PerformanceEntry::EntryType::kElement);
  return static_cast<PerformanceElementTiming*>(arg.Get())->id() == id;
}

}  // namespace

class ImageElementTimingTest : public PaintTimingTestBase,
                               public PaintTestConfigurations {
 protected:
  // Returns true if the LayoutObject/Image with the given hash was recorded,
  // meaning it was processed. Note that this does not mean it an entry will be
  // emitted for it.
  bool IsRecorded(const char* id, const MediaTiming* timing) {
    return IsRecorded(GetLayoutObjectById(id), timing);
  }

  bool IsRecorded(const LayoutObject* object, const MediaTiming* timing) {
    return ImageElementTiming::From(*GetDocument().domWindow())
        .recorded_images_.Contains(MediaRecordId::GenerateHash(object, timing));
  }

  unsigned RecordedImagesSize() {
    return ImageElementTiming::From(*GetDocument().domWindow())
        .recorded_images_.size();
  }

  LayoutObject* GetLayoutObjectById(const char* id) {
    Element* element = GetElementById(id);
    return element ? element->GetLayoutObject() : nullptr;
  }

  PerformanceEntryVector GetElementTimingEntries() {
    return DOMWindowPerformance::performance(*GetDocument().domWindow())
        ->getBufferedEntriesByType(performance_entry_names::kElement);
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(ImageElementTimingTest);

TEST_P(ImageElementTimingTest, TestIsExplicitlyRegisteredForElementTiming) {
  SetMainFrameBodyContent(R"HTML(
    <img id="missing-attribute" style='width: 100px; height: 100px;'/>
    <img id="unset-attribute" elementtiming
         style='width: 100px; height: 100px;'/>
    <img id="empty-attribute" elementtiming=""
         style='width: 100px; height: 100px;'/>
    <img id="valid-attribute" elementtiming="valid-id"
         style='width: 100px; height: 100px;'/>
  )HTML");
  SimulateRenderingAndPresentationTime();

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
  // Tests that, if the 'elementtiming' attribute is missing, the element is
  // ignored by `ImageElementTiming`.
  SetMainFrameBodyContent(R"HTML(
    <img id="target" style='width: 100px; height: 100px;'/>
  )HTML");
  ImageResourceContent* image = SetImageContent("target", 100, 100);
  SimulateRenderingAndPresentationTime();
  EXPECT_TRUE(IsRecorded("target", image));
  EXPECT_THAT(GetElementTimingEntries(), IsEmpty());
}

TEST_P(ImageElementTimingTest, ImageInsideSVG) {
  SetMainFrameBodyContent(R"HTML(
    <svg>
      <foreignObject width="100" height="100">
        <img elementtiming="image-inside-svg" id="target"
             style='width: 100px; height: 100px;'/>
      </foreignObject>
    </svg>
  )HTML");
  ImageResourceContent* image = SetImageContent("target", 100, 100);
  SimulateRenderingAndPresentationTime();

  // An entry should have been emitted for the image.
  EXPECT_TRUE(IsRecorded("target", image));
  EXPECT_THAT(GetElementTimingEntries(), ElementsAre(ForId("target")));
}

TEST_P(ImageElementTimingTest, ImageInsideNonRenderedSVG) {
  SetMainFrameBodyContent(R"HTML(
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
  SimulateRenderingAndPresentationTime();

  // HTML inside foreignObject in a non-rendered SVG subtree should not generate
  // layout objects. Generating layout objects for caused crashes
  // (crbug.com/905850) as well as correctness issues.
  EXPECT_FALSE(GetLayoutObjectById("target"));
  EXPECT_THAT(GetElementTimingEntries(), IsEmpty());
}

TEST_P(ImageElementTimingTest, ImageRemoved) {
  SetMainFrameBodyContent(R"HTML(
    <img elementtiming="will-be-removed" id="target"
         style='width: 100px; height: 100px;'/>
  )HTML");
  ImageResourceContent* image = SetImageContent("target", 100, 100);
  SimulateRenderingAndPresentationTime();
  EXPECT_TRUE(IsRecorded("target", image));
  EXPECT_THAT(GetElementTimingEntries(), ElementsAre(ForId("target")));

  GetDocument().getElementById(AtomicString("target"))->remove();
  // `image` should no longer be part of `images_notified_` since it will
  // be destroyed.
  EXPECT_EQ(RecordedImagesSize(), 0u);
}

TEST_P(ImageElementTimingTest, SVGImageRemoved) {
  SetMainFrameBodyContent(R"HTML(
    <svg>
      <image elementtiming="svg-will-be-removed" id="target"
             style='width: 100px; height: 100px;'/>
    </svg>
  )HTML");
  ImageResourceContent* image = SetImageContent("target", 100, 100);
  SimulateRenderingAndPresentationTime();
  EXPECT_TRUE(IsRecorded("target", image));
  EXPECT_THAT(GetElementTimingEntries(), ElementsAre(ForId("target")));

  GetDocument().getElementById(AtomicString("target"))->remove();
  // `image` should no longer be part of `images_notified_` since it will be
  // destroyed.
  EXPECT_EQ(RecordedImagesSize(), 0u);
}

TEST_P(ImageElementTimingTest, BackgroundImageRemoved) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      #target {
        width: 100px;
        height: 100px;
        background: url(data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==);
      }
    </style>
    <div elementtiming="time-my-background-image" id="target"></div>
  )HTML");
  SimulateRenderingAndPresentationTime();
  LayoutObject* object = GetLayoutObjectById("target");
  ImageResourceContent* content =
      object->StyleRef().BackgroundLayers().GetImage()->CachedImage();
  EXPECT_EQ(RecordedImagesSize(), 1u);
  EXPECT_TRUE(IsRecorded(object, content));
  EXPECT_THAT(GetElementTimingEntries(), ElementsAre(ForId("target")));

  GetDocument().getElementById(AtomicString("target"))->remove();
  EXPECT_EQ(RecordedImagesSize(), 0u);
}

TEST_P(ImageElementTimingTest, LateAddedElementTimingBeforePaint) {
  SetMainFrameBodyContent(R"HTML(
    <img id="target" style='width: 100px; height: 100px;'/>
  )HTML");
  SimulateRenderingAndPresentationTime();
  // This image should not be tracked because it hasn't finished loading yet.
  EXPECT_EQ(RecordedImagesSize(), 0u);

  // Add the elementtiming attribute dynamically after the image before the
  // image has finished loading and is ready for to be rendered.
  GetElementById("target")->setAttribute(html_names::kElementtimingAttr,
                                         AtomicString("test"));
  ImageResourceContent* image = SetImageContent("target", 100, 100);
  SimulateRenderingAndPresentationTime();
  // The image should be recorded and an entry should have been emitted.
  EXPECT_EQ(RecordedImagesSize(), 1u);
  EXPECT_TRUE(IsRecorded("target", image));
  EXPECT_THAT(GetElementTimingEntries(), ElementsAre(ForId("target")));
}

TEST_P(ImageElementTimingTest, LateAddedElementTimingAfterPaint) {
  SetBodyInnerHTML(R"HTML(
    <div id="to-be-removed">Text</div>
    <img id="target" style='width: 100px; height: 100px;'/>
  )HTML");
  ImageResourceContent* image = SetImageContent("target", 100, 100);
  SimulateRenderingAndPresentationTime();
  // This image should have been recorded, but no entry should have been
  // emitted.
  EXPECT_EQ(RecordedImagesSize(), 1u);
  EXPECT_TRUE(IsRecorded("target", image));
  EXPECT_THAT(GetElementTimingEntries(), IsEmpty());

  // Add the elementtiming attribute dynamically after the image was already
  // rendered.
  GetElementById("target")->setAttribute(html_names::kElementtimingAttr,
                                         AtomicString("test"));
  // Remove the <div>. This causes a layout shift which should cause the image
  // to repaint, but this should not trigger an elementtiming entry.
  GetElementById("to-be-removed")->remove();
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(RecordedImagesSize(), 1u);
  EXPECT_TRUE(IsRecorded("target", image));
  EXPECT_THAT(GetElementTimingEntries(), IsEmpty());
}

}  // namespace blink
