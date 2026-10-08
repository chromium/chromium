// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/element_timing.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/timing/media_record_id.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_test_base.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/performance_element_timing.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using testing::AllOf;
using testing::ElementsAre;
using testing::IsEmpty;

namespace blink {

namespace {

// Simple matcher for matching the id of a `PerformanceElementTiming` entry.
MATCHER_P(ForId, id, "") {
  CHECK_EQ(arg->EntryTypeEnum(), PerformanceEntry::EntryType::kElement);
  return static_cast<PerformanceElementTiming*>(arg.Get())->id() == id;
}

MATCHER_P(ForElement, element, "") {
  CHECK_EQ(arg->EntryTypeEnum(), PerformanceEntry::EntryType::kElement);
  return static_cast<PerformanceElementTiming*>(arg.Get())->element() ==
         element;
}

MATCHER_P(ForIdentifier, identifier, "") {
  CHECK_EQ(arg->EntryTypeEnum(), PerformanceEntry::EntryType::kElement);
  return static_cast<PerformanceElementTiming*>(arg.Get())->identifier() ==
         identifier;
}

}  // namespace

class ElementTimingTest : public PaintTimingTestBase,
                          public PaintTestConfigurations {
 protected:
  // Returns true if the LayoutObject/Image with the given hash was recorded,
  // meaning it was processed. Note that this does not mean an entry will be
  // emitted for it.
  bool IsRecorded(const char* id, const MediaTiming* timing) {
    return IsRecorded(GetLayoutObjectById(id), timing);
  }

  bool IsRecorded(const LayoutObject* object, const MediaTiming* timing) {
    return ElementTiming::From(*GetDocument().domWindow())
        .recorded_images_.Contains(MediaRecordId::GenerateHash(object, timing));
  }

  unsigned RecordedImagesSize() {
    return ElementTiming::From(*GetDocument().domWindow())
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

INSTANTIATE_PAINT_TEST_SUITE_P(ElementTimingTest);

TEST_P(ElementTimingTest, TestIsRegisteredForElementTiming) {
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

  Element* without_attribute = GetElementById("missing-attribute");
  bool actual = ElementTiming::IsRegisteredForElementTiming(without_attribute);
  EXPECT_FALSE(actual) << "Nodes without an 'elementtiming' attribute should "
                          "not be registered.";

  Element* with_undefined_attribute = GetElementById("unset-attribute");
  actual =
      ElementTiming::IsRegisteredForElementTiming(with_undefined_attribute);
  EXPECT_TRUE(actual) << "Nodes with undefined 'elementtiming' attribute "
                         "should be registered.";

  Element* with_empty_attribute = GetElementById("empty-attribute");
  actual = ElementTiming::IsRegisteredForElementTiming(with_empty_attribute);
  EXPECT_TRUE(actual) << "Nodes with an empty 'elementtiming' attribute "
                         "should be registered.";

  Element* with_explicit_element_timing = GetElementById("valid-attribute");
  actual =
      ElementTiming::IsRegisteredForElementTiming(with_explicit_element_timing);
  EXPECT_TRUE(actual) << "Nodes with a non-empty 'elementtiming' attribute "
                         "should be registered.";
}

TEST_P(ElementTimingTest, IgnoresUnmarkedElement) {
  // Tests that, if the 'elementtiming' attribute is missing, the element is
  // ignored by `ElementTiming`.
  SetMainFrameBodyContent(R"HTML(
    <img id="target" style='width: 100px; height: 100px;'/>
  )HTML");
  ImageResourceContent* image = SetImageContent("target", 100, 100);
  SimulateRenderingAndPresentationTime();
  EXPECT_TRUE(IsRecorded("target", image));
  EXPECT_THAT(GetElementTimingEntries(), IsEmpty());
}

TEST_P(ElementTimingTest, ImageInsideSVG) {
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

TEST_P(ElementTimingTest, ImageInsideNonRenderedSVG) {
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

TEST_P(ElementTimingTest, ImageRemoved) {
  SetMainFrameBodyContent(R"HTML(
    <img elementtiming="will-be-removed" id="target"
         style='width: 100px; height: 100px;'/>
  )HTML");
  ImageResourceContent* image = SetImageContent("target", 100, 100);
  SimulateRenderingAndPresentationTime();
  EXPECT_TRUE(IsRecorded("target", image));
  EXPECT_THAT(GetElementTimingEntries(), ElementsAre(ForId("target")));

  GetDocument().getElementById(AtomicString("target"))->remove();
  // `image` should no longer be part of `recorded_images_` since it will
  // be destroyed.
  EXPECT_EQ(RecordedImagesSize(), 0u);
}

TEST_P(ElementTimingTest, SVGImageRemoved) {
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
  // `image` should no longer be part of `recorded_images_` since it will be
  // destroyed.
  EXPECT_EQ(RecordedImagesSize(), 0u);
}

TEST_P(ElementTimingTest, BackgroundImageRemoved) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      #target {
        width: 100px;
        height: 100px;
        background: url()HTML" SIMPLE_IMAGE R"HTML();
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

TEST_P(ElementTimingTest, PseudoElementBackgroundImageRemoved) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      #target::before {
        content: "";
        display: block;
        width: 100px;
        height: 100px;
        background: url()HTML" SIMPLE_IMAGE R"HTML();
      }
    </style>
    <div elementtiming="time-my-background-image" id="target"></div>
  )HTML");
  SimulateRenderingAndPresentationTime();
  Element* target = GetElementById("target");
  ASSERT_TRUE(target);
  Element* before = target->GetPseudoElement(kPseudoIdBefore);
  ASSERT_TRUE(before);
  LayoutObject* object = before->GetLayoutObject();
  ASSERT_TRUE(object);
  ImageResourceContent* content =
      object->StyleRef().BackgroundLayers().GetImage()->CachedImage();
  EXPECT_EQ(RecordedImagesSize(), 1u);
  EXPECT_TRUE(IsRecorded(object, content));
  EXPECT_THAT(GetElementTimingEntries(), ElementsAre(ForId("target")));

  target->remove();
  EXPECT_EQ(RecordedImagesSize(), 0u);
}

TEST_P(ElementTimingTest, PseudoElementBackgroundImage_IgnoreDisplayContents) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      #target {
        display: contents;
      }
      #target::before {
        content: "";
        display: block;
        width: 100px;
        height: 100px;
        background: url()HTML" SIMPLE_IMAGE R"HTML();
      }
    </style>
    <div elementtiming="time-my-background-image" id="target"></div>
  )HTML");
  SimulateRenderingAndPresentationTime();
  Element* target = GetElementById("target");
  ASSERT_TRUE(target);
  EXPECT_FALSE(target->GetLayoutObject());
  ASSERT_TRUE(target->GetPseudoElement(kPseudoIdBefore));
  EXPECT_TRUE(target->GetPseudoElement(kPseudoIdBefore)->GetLayoutObject());
  EXPECT_EQ(RecordedImagesSize(), 0u);
  EXPECT_THAT(GetElementTimingEntries(), IsEmpty());
}

TEST_P(ElementTimingTest, LateAddedElementTimingBeforePaint) {
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

TEST_P(ElementTimingTest, LateAddedElementTimingAfterPaint) {
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

TEST_P(ElementTimingTest, VideoImage_DefaultPosterIgnored) {
  GetDocument().GetSettings()->SetDefaultVideoPosterURL(
      AtomicString(SIMPLE_IMAGE));
  SetBodyInnerHTML(R"HTML(
    <video id="target" elementtiming="video-et" width="100" height="100"></video>
  )HTML");
  test::RunPendingTasks();
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(RecordedImagesSize(), 0u);
  EXPECT_THAT(GetElementTimingEntries(), IsEmpty());
}

TEST_P(ElementTimingTest, VideoImage_ExplicitPosterRecordedWhenDefaultSet) {
  GetDocument().GetSettings()->SetDefaultVideoPosterURL(
      AtomicString(SIMPLE_IMAGE));
  SetBodyInnerHTML(R"HTML(
    <video id="target" elementtiming="video-et"
           width="100" height="100"></video>
  )HTML");
  test::RunPendingTasks();
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(RecordedImagesSize(), 0u);
  EXPECT_THAT(GetElementTimingEntries(), IsEmpty());

  // Changing the poster to an explicit image after the default poster loaded
  // should record the new explicit poster.
  Element* video_element = GetElementById("target");
  ASSERT_TRUE(video_element);
  video_element->setAttribute(html_names::kPosterAttr,
                              AtomicString(LARGE_IMAGE));
  test::RunPendingTasks();
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(RecordedImagesSize(), 1u);
  EXPECT_THAT(GetElementTimingEntries(), ElementsAre(ForId("target")));

  // Removing the explicit poster attribute reverts to the default poster,
  // which should remove the explicit poster record and not record the default.
  video_element->removeAttribute(html_names::kPosterAttr);
  test::RunPendingTasks();
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(RecordedImagesSize(), 0u);
}

TEST_P(ElementTimingTest, TextElementTimingBasic) {
  SetMainFrameBodyContent(R"HTML(
    <p id="target" elementtiming="paragraph">Sample paragraph text</p>
    <p id="unmarked">Unmarked paragraph text</p>
  )HTML");
  SimulateRenderingAndPresentationTime();

  EXPECT_THAT(GetElementTimingEntries(), ElementsAre(ForId("target")));
}

TEST_P(ElementTimingTest, TextElementTimingInShadowTreeIgnored) {
  SetMainFrameBodyContent(R"HTML(
    <div id="host"></div>
  )HTML");
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <p id="shadow-target" elementtiming="shadow-para">Shadow paragraph</p>
  )HTML");
  SimulateRenderingAndPresentationTime();

  EXPECT_THAT(GetElementTimingEntries(), IsEmpty());
}

TEST_P(ElementTimingTest, ElementAndContainerTimingDecoupled) {
  SetMainFrameBodyContent(R"HTML(
    <p containertiming="foo">Sample paragraph text</p>
    <img id="target" containertiming="bar" style="width: 100px; height: 100px;"/>
  )HTML");
  SetImageContent("target", 100, 100);
  SimulateRenderingAndPresentationTime();

  EXPECT_THAT(GetElementTimingEntries(), IsEmpty());
}

TEST_P(ElementTimingTest,
       TextElementRemovedAndGCedBetweenPaintAndPresentation) {
  SetMainFrameBodyContent(R"HTML(
    <p id="target" elementtiming>Text</p>
  )HTML");
  WeakPersistent<Element> target(GetElementById("target"));
  EXPECT_TRUE(target);

  SimulateRendering();
  EXPECT_THAT(GetElementTimingEntries(), IsEmpty());

  GetElementById("target")->remove();
  // The layout system might hold onto `target` until the next paint, so
  // simulate another frame. Also flush any tasks in case anything is holding
  // onto the node.
  SimulateRendering();
  test::RunPendingTasks();

  // Run GC. This should collect `target`, but an entry should still be emitted
  // because the frame was presented.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(target);

  SimulatePresentationTime();

  EXPECT_THAT(GetElementTimingEntries(),
              ElementsAre(AllOf(ForId("target"), ForElement(nullptr))));
}

TEST_P(ElementTimingTest,
       ImageElementRemovedAndGCedBetweenPaintAndPresentation) {
  SetMainFrameBodyContent(R"HTML(
    <img id="target" elementtiming="img-id" style="width: 100px; height: 100px;"/>
  )HTML");
  SetImageContent("target", 100, 100);
  WeakPersistent<Element> target(GetElementById("target"));
  EXPECT_TRUE(target);

  SimulateRendering();
  EXPECT_THAT(GetElementTimingEntries(), IsEmpty());

  GetElementById("target")->remove();
  // Run any pending tasks to clear references to the target node.
  test::RunPendingTasks();
  // Run GC. This should collect `target`, but an entry should still be emitted
  // because the frame was presented.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(target);

  SimulatePresentationTime();

  EXPECT_THAT(GetElementTimingEntries(),
              ElementsAre(AllOf(ForId("target"), ForElement(nullptr))));
}

TEST_P(ElementTimingTest, AttributeChangedBetweenPaintAndPresentation) {
  SetMainFrameBodyContent(R"HTML(
    <p id="initial-id" elementtiming="initial-et">Text</p>
  )HTML");

  SimulateRendering();
  EXPECT_THAT(GetElementTimingEntries(), IsEmpty());

  Element* target = GetElementById("initial-id");
  target->setAttribute(html_names::kIdAttr, AtomicString("mutated-id"));
  target->setAttribute(html_names::kElementtimingAttr,
                       AtomicString("mutated-et"));

  SimulatePresentationTime();

  EXPECT_THAT(
      GetElementTimingEntries(),
      ElementsAre(AllOf(ForId("initial-id"), ForIdentifier("initial-et"),
                        ForElement(target))));
}

TEST_P(ElementTimingTest, AttributeRemovedBetweenPaintAndPresentation) {
  SetMainFrameBodyContent(R"HTML(
    <p id="target" elementtiming="initial">Text</p>
  )HTML");

  SimulateRendering();
  EXPECT_THAT(GetElementTimingEntries(), IsEmpty());

  Element* target = GetElementById("target");
  target->removeAttribute(html_names::kElementtimingAttr);

  SimulatePresentationTime();

  EXPECT_THAT(GetElementTimingEntries(),
              ElementsAre(AllOf(ForId("target"), ForIdentifier("initial"),
                                ForElement(target))));
}

}  // namespace blink
