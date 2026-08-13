// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/highlight/highlight_registry.h"

#include "base/functional/function_ref.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_highlight_hit_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_highlights_from_point_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_static_range_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/static_range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/highlight/highlight.h"
#include "third_party/blink/renderer/core/highlight/highlight_style_utils.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/replaced_painter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

class HighlightRegistryTest : public PageTestBase {
 public:
  Highlight* CreateHighlight(Text* node_start,
                             int start,
                             Text* node_end,
                             int end) {
    auto* range = MakeGarbageCollected<Range>(GetDocument(), node_start, start,
                                              node_end, end);
    HeapVector<Member<AbstractRange>> range_vector;
    range_vector.push_back(range);
    return Highlight::Create(range_vector);
  }

  // Builds a Highlight whose only range is a child-index StaticRange of
  // `container` from `start_offset` to `end_offset`. Replaced-element
  // tracking tests use this shape because it is the most direct way to
  // anchor a highlight at the parent of a replaced element (e.g. <img>)
  // without depending on the element having any text content of its own.
  Highlight* CreateChildIndexStaticRangeHighlight(Element* container,
                                                  int start_offset,
                                                  int end_offset) {
    auto* init = StaticRangeInit::Create();
    init->setStartContainer(container);
    init->setStartOffset(start_offset);
    init->setEndContainer(container);
    init->setEndOffset(end_offset);
    auto* static_range = StaticRange::Create(init, ASSERT_NO_EXCEPTION);
    HeapVector<Member<AbstractRange>> ranges;
    ranges.push_back(static_range);
    return Highlight::Create(ranges);
  }

  // Sets `inner_html` on the body, registers a highlight named "h" anchored
  // at child indices (0, 1) of the host's parent (i.e. covering just the
  // host), runs the lifecycle, and EXPECTs that the host with id `host_id`
  // is NOT in the replaced-element active set.
  //
  // Used by the SkipsXxx tests: they all share this shape and only differ
  // in the HTML fragment that produces a non-trackable replaced element.
  // Kept as a helper (not TEST_P) so each case stays a named TEST_F and
  // failures point at the specific element shape that regressed.
  void ExpectNonTrackable(const char* inner_html, const char* host_id) {
    GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(inner_html);
    auto* host =
        To<Element>(GetDocument().getElementById(AtomicString(host_id)));
    ASSERT_TRUE(host);
    auto* wrapper = host->parentElement();
    ASSERT_TRUE(wrapper);
    auto* highlight = CreateChildIndexStaticRangeHighlight(wrapper, 0, 1);
    HighlightRegistry* registry =
        HighlightRegistry::From(*GetDocument().domWindow());
    registry->SetForTesting(AtomicString("h"), highlight);
    UpdateAllLifecyclePhasesForTest();
    EXPECT_TRUE(registry->GetActiveHighlightsForReplacedElement(*host).empty());
  }
};

class HighlightsFromPointTest : public HighlightRegistryTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kHighlightsFromPoint);
    HighlightRegistryTest::SetUp();
  }

  void GetMarkerCenterPoint(DocumentMarker* marker,
                            Text* text,
                            float& x,
                            float& y) {
    Position marker_start_position = Position(text, marker->StartOffset());
    Position marker_end_position = Position(text, marker->EndOffset());
    EphemeralRange range(marker_start_position, marker_end_position);
    gfx::Rect rect = ComputeTextRect(range);
    x = rect.x() + rect.width() / 2.0;
    y = rect.y() + rect.height() / 2.0;
    // Scale coordinates from physical pixels to CSS pixels.
    x /= GetDocument().DevicePixelRatio();
    y /= GetDocument().DevicePixelRatio();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class HighlightRegistryFrameTest : public RenderingTest {
 public:
  HighlightRegistryFrameTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}
};

TEST_F(HighlightRegistryTest, CompareStacking) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);

  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* highlight1 = CreateHighlight(text, 0, text, 4);
  AtomicString highlight1_name("TestHighlight1");

  auto* highlight2 = CreateHighlight(text, 2, text, 4);
  AtomicString highlight2_name("TestHighlight2");

  registry->SetForTesting(highlight1_name, highlight1);
  registry->SetForTesting(highlight2_name, highlight2);

  EXPECT_EQ(HighlightRegistry::kOverlayStackingPositionEquivalent,
            registry->CompareOverlayStackingPosition(highlight1_name,
                                                     highlight1_name));
  EXPECT_EQ(HighlightRegistry::kOverlayStackingPositionBelow,
            registry->CompareOverlayStackingPosition(highlight1_name,
                                                     highlight2_name));
  EXPECT_EQ(HighlightRegistry::kOverlayStackingPositionAbove,
            registry->CompareOverlayStackingPosition(highlight2_name,
                                                     highlight1_name));
  highlight1->setPriority(2);
  highlight1->setPriority(1);
  EXPECT_EQ(HighlightRegistry::kOverlayStackingPositionAbove,
            registry->CompareOverlayStackingPosition(highlight1_name,
                                                     highlight2_name));
}

TEST_F(HighlightRegistryTest, ValidateMarkers) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<p>aaaaaaaaaa</p><p>bbbbbbbbbb</p><p>cccccccccc</p>");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);

  auto* node_a = GetDocument().body()->firstChild();
  auto* node_b = node_a->nextSibling();
  auto* node_c = node_b->nextSibling();
  auto* text_a = To<Text>(node_a->firstChild());
  auto* text_b = To<Text>(node_b->firstChild());
  auto* text_c = To<Text>(node_c->firstChild());

  // Create several ranges, including those crossing multiple nodes
  HeapVector<Member<AbstractRange>> range_vector_1;
  auto* range_aa =
      MakeGarbageCollected<Range>(GetDocument(), text_a, 0, text_a, 4);
  range_vector_1.push_back(range_aa);
  auto* range_ab =
      MakeGarbageCollected<Range>(GetDocument(), text_a, 5, text_b, 4);
  range_vector_1.push_back(range_ab);
  auto* highlight1 = Highlight::Create(range_vector_1);
  AtomicString highlight1_name("TestHighlight1");

  HeapVector<Member<AbstractRange>> range_vector_2;
  auto* range_bb =
      MakeGarbageCollected<Range>(GetDocument(), text_b, 5, text_b, 8);
  range_vector_2.push_back(range_bb);
  auto* highlight2 = Highlight::Create(range_vector_2);
  AtomicString highlight2_name("TestHighlight2");

  HeapVector<Member<AbstractRange>> range_vector_3;
  auto* range_bc =
      MakeGarbageCollected<Range>(GetDocument(), text_b, 9, text_c, 4);
  range_vector_3.push_back(range_bc);
  auto* range_cc =
      MakeGarbageCollected<Range>(GetDocument(), text_c, 5, text_c, 9);
  range_vector_3.push_back(range_cc);
  auto* highlight3 = Highlight::Create(range_vector_3);
  AtomicString highlight3_name("TestHighlight3");

  registry->SetForTesting(highlight1_name, highlight1);
  registry->SetForTesting(highlight2_name, highlight2);
  registry->SetForTesting(highlight3_name, highlight3);

  // When the document lifecycle runs, marker invalidation should
  // happen and create markers. Verify that it happens.
  UpdateAllLifecyclePhasesForTest();

  DocumentMarkerController& marker_controller = GetDocument().Markers();
  DocumentMarkerVector text_a_markers = marker_controller.MarkersFor(
      *text_a, DocumentMarker::MarkerTypes::CustomHighlight());
  DocumentMarkerVector text_b_markers = marker_controller.MarkersFor(
      *text_b, DocumentMarker::MarkerTypes::CustomHighlight());
  DocumentMarkerVector text_c_markers = marker_controller.MarkersFor(
      *text_c, DocumentMarker::MarkerTypes::CustomHighlight());

  EXPECT_EQ(2u, text_a_markers.size());
  EXPECT_EQ(3u, text_b_markers.size());
  EXPECT_EQ(2u, text_c_markers.size());

  int index = 0;
  for (auto& marker : text_a_markers) {
    auto* custom_marker = To<CustomHighlightMarker>(marker.Get());
    switch (index) {
      case 0: {
        EXPECT_EQ(0u, custom_marker->StartOffset());
        EXPECT_EQ(4u, custom_marker->EndOffset());
        EXPECT_EQ(highlight1_name, custom_marker->GetPseudoArgument());
      } break;
      case 1: {
        EXPECT_EQ(5u, custom_marker->StartOffset());
        EXPECT_EQ(10u, custom_marker->EndOffset());
        EXPECT_EQ(highlight1_name, custom_marker->GetPseudoArgument());
      } break;
      default:
        EXPECT_TRUE(false);
    }
    ++index;
  }
  index = 0;
  for (auto& marker : text_b_markers) {
    auto* custom_marker = To<CustomHighlightMarker>(marker.Get());
    switch (index) {
      case 0: {
        EXPECT_EQ(0u, custom_marker->StartOffset());
        EXPECT_EQ(4u, custom_marker->EndOffset());
        EXPECT_EQ(highlight1_name, custom_marker->GetPseudoArgument());
      } break;
      case 1: {
        EXPECT_EQ(5u, custom_marker->StartOffset());
        EXPECT_EQ(8u, custom_marker->EndOffset());
        EXPECT_EQ(highlight2_name, custom_marker->GetPseudoArgument());
      } break;
      case 2: {
        EXPECT_EQ(9u, custom_marker->StartOffset());
        EXPECT_EQ(10u, custom_marker->EndOffset());
        EXPECT_EQ(highlight3_name, custom_marker->GetPseudoArgument());
      } break;
      default:
        EXPECT_TRUE(false);
    }
    ++index;
  }
  index = 0;
  for (auto& marker : text_c_markers) {
    auto* custom_marker = To<CustomHighlightMarker>(marker.Get());
    switch (index) {
      case 0: {
        EXPECT_EQ(0u, custom_marker->StartOffset());
        EXPECT_EQ(4u, custom_marker->EndOffset());
        EXPECT_EQ(highlight3_name, custom_marker->GetPseudoArgument());
      } break;
      case 1: {
        EXPECT_EQ(5u, custom_marker->StartOffset());
        EXPECT_EQ(9u, custom_marker->EndOffset());
        EXPECT_EQ(highlight3_name, custom_marker->GetPseudoArgument());
      } break;
      default:
        EXPECT_TRUE(false);
    }
    ++index;
  }

  registry->RemoveForTesting(highlight2_name, highlight2);
  UpdateAllLifecyclePhasesForTest();

  text_a_markers = marker_controller.MarkersFor(
      *text_a, DocumentMarker::MarkerTypes::CustomHighlight());
  text_b_markers = marker_controller.MarkersFor(
      *text_b, DocumentMarker::MarkerTypes::CustomHighlight());
  text_c_markers = marker_controller.MarkersFor(
      *text_c, DocumentMarker::MarkerTypes::CustomHighlight());

  EXPECT_EQ(2u, text_a_markers.size());
  EXPECT_EQ(2u, text_b_markers.size());
  EXPECT_EQ(2u, text_c_markers.size());

  index = 0;
  for (auto& marker : text_a_markers) {
    auto* custom_marker = To<CustomHighlightMarker>(marker.Get());
    switch (index) {
      case 0: {
        EXPECT_EQ(0u, custom_marker->StartOffset());
        EXPECT_EQ(4u, custom_marker->EndOffset());
        EXPECT_EQ(highlight1_name, custom_marker->GetPseudoArgument());
      } break;
      case 1: {
        EXPECT_EQ(5u, custom_marker->StartOffset());
        EXPECT_EQ(10u, custom_marker->EndOffset());
        EXPECT_EQ(highlight1_name, custom_marker->GetPseudoArgument());
      } break;
      default:
        EXPECT_TRUE(false);
    }
    ++index;
  }
  index = 0;
  for (auto& marker : text_b_markers) {
    auto* custom_marker = To<CustomHighlightMarker>(marker.Get());
    switch (index) {
      case 0: {
        EXPECT_EQ(0u, custom_marker->StartOffset());
        EXPECT_EQ(4u, custom_marker->EndOffset());
        EXPECT_EQ(highlight1_name, custom_marker->GetPseudoArgument());
      } break;
      case 1: {
        EXPECT_EQ(9u, custom_marker->StartOffset());
        EXPECT_EQ(10u, custom_marker->EndOffset());
        EXPECT_EQ(highlight3_name, custom_marker->GetPseudoArgument());
      } break;
      default:
        EXPECT_TRUE(false);
    }
    ++index;
  }
  index = 0;
  for (auto& marker : text_c_markers) {
    auto* custom_marker = To<CustomHighlightMarker>(marker.Get());
    switch (index) {
      case 0: {
        EXPECT_EQ(0u, custom_marker->StartOffset());
        EXPECT_EQ(4u, custom_marker->EndOffset());
        EXPECT_EQ(highlight3_name, custom_marker->GetPseudoArgument());
      } break;
      case 1: {
        EXPECT_EQ(5u, custom_marker->StartOffset());
        EXPECT_EQ(9u, custom_marker->EndOffset());
        EXPECT_EQ(highlight3_name, custom_marker->GetPseudoArgument());
      } break;
      default:
        EXPECT_TRUE(false);
    }
    ++index;
  }
}

TEST_F(HighlightRegistryTest, TracksReplacedElementAndUnregisterDrops) {
  // Baseline positive + round-trip: a wrapper-anchored highlight covering a
  // childless <img> must add the image to the replaced-element active set
  // so ReplacedPainter can tint it, and removing the highlight from the
  // registry must drop the image again so a stale tint doesn't survive a
  // deleted ::highlight() rule.
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='w'><img id='img' style='width:50px;height:50px;'></div>");
  auto* wrapper = GetDocument().getElementById(AtomicString("w"));
  auto* img = To<Element>(GetDocument().getElementById(AtomicString("img")));
  ASSERT_TRUE(wrapper && img);

  auto* highlight = CreateChildIndexStaticRangeHighlight(wrapper, 0, 1);
  AtomicString name("h");
  HighlightRegistry* registry =
      HighlightRegistry::From(*GetDocument().domWindow());
  registry->SetForTesting(name, highlight);
  UpdateAllLifecyclePhasesForTest();

  const auto& active = registry->GetActiveHighlightsForReplacedElement(*img);
  EXPECT_EQ(1u, active.size());
  EXPECT_TRUE(active.Contains(name));

  registry->RemoveForTesting(name, highlight);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(registry->GetActiveHighlightsForReplacedElement(*img).empty());
}

TEST_F(HighlightRegistryTest, TracksReplacedElementInRangeSpanningText) {
  // A live Range that spans text content with an <img> as a descendant
  // (not a direct child of the range's container) must still flag the
  // image for replaced-element painting. The traversal in
  // ValidateHighlightMarkers() walks the EphemeralRange built from the
  // abstract range, so the range source (live Range vs StaticRange) and
  // the depth of the <img> below the range's container are irrelevant.
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<p id='p'>before<img id='img' style='width:10px;height:10px;'>after"
      "</p>");
  auto* p = GetDocument().getElementById(AtomicString("p"));
  auto* before = To<Text>(p->firstChild());
  auto* after = To<Text>(p->lastChild());
  auto* img = To<Element>(GetDocument().getElementById(AtomicString("img")));
  ASSERT_TRUE(img);

  auto* range = MakeGarbageCollected<Range>(GetDocument(), before, 0, after,
                                            after->length());
  HeapVector<Member<AbstractRange>> ranges;
  ranges.push_back(range);
  auto* highlight = Highlight::Create(ranges);
  AtomicString name("h");
  HighlightRegistry* registry =
      HighlightRegistry::From(*GetDocument().domWindow());
  registry->SetForTesting(name, highlight);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(
      registry->GetActiveHighlightsForReplacedElement(*img).Contains(name));
}

TEST_F(HighlightRegistryTest, CollectorDoesNotChangeTextMarkers) {
  // Supplying the on_element_node collector only turns on
  // object-replacement-character emission so the single TextIterator pass can
  // surface replaced elements; it must not change the text markers produced.
  // Build the same text+<img> range with and without the collector and assert
  // the resulting custom-highlight markers are identical.
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<p id='p'>before<img id='img' style='width:10px;height:10px;'>after"
      "</p>");
  auto* p = GetDocument().getElementById(AtomicString("p"));
  ASSERT_TRUE(p);
  auto* before = To<Text>(p->firstChild());
  auto* after = To<Text>(p->lastChild());
  auto* img = To<Element>(GetDocument().getElementById(AtomicString("img")));
  ASSERT_TRUE(before && after && img);
  UpdateAllLifecyclePhasesForTest();

  const EphemeralRange range(MakeGarbageCollected<Range>(
      GetDocument(), before, 0, after, after->length()));
  AtomicString name("h");
  Highlight* highlight = CreateHighlight(before, 0, after, after->length());

  DocumentMarkerController& markers = GetDocument().Markers();
  const DocumentMarker::MarkerTypes custom =
      DocumentMarker::MarkerTypes::CustomHighlight();
  // Flattened (start, end) offsets of the custom-highlight markers on `text`.
  auto snapshot = [&](const Text& text) {
    Vector<unsigned> offsets;
    for (const auto& marker : markers.MarkersFor(text, custom)) {
      offsets.push_back(marker->StartOffset());
      offsets.push_back(marker->EndOffset());
    }
    return offsets;
  };

  // Without the collector.
  markers.AddCustomHighlightMarker(range, name, highlight);
  const Vector<unsigned> before_without = snapshot(*before);
  const Vector<unsigned> after_without = snapshot(*after);
  ASSERT_FALSE(before_without.empty())
      << "precondition: the range should mark the 'before' text node";
  markers.RemoveMarkersOfTypes(custom);

  // With the collector.
  bool collector_saw_img = false;
  auto on_element_node = [&](const Element& element) {
    collector_saw_img |= (&element == img);
  };
  base::FunctionRef<void(const Element&)> on_element_ref(on_element_node);
  markers.AddCustomHighlightMarker(range, name, highlight, &on_element_ref);

  EXPECT_EQ(before_without, snapshot(*before));
  EXPECT_EQ(after_without, snapshot(*after));
  EXPECT_TRUE(collector_saw_img)
      << "the collector must surface the <img> the range crosses";
}

// Negative cases for the IsTrackableReplacedElement gate. Each demonstrates
// a different layout-tree shape that must NOT be tracked, even though the
// hosting element is in the highlight's range. See ExpectNonTrackable for
// the shared setup pattern.

TEST_F(HighlightRegistryTest, SkipsChildlessReplacedThatIsNotASelectionLeaf) {
  // A childless replaced element whose layout class does not override
  // CanBeSelectionLeafInternal() must not be tracked. LayoutHTMLCanvas
  // (<canvas>) and LayoutSVGRoot (inline <svg>) are both replaced but neither
  // opts in as a selection leaf, matching how ::selection (Blink) and
  // ::highlight() (Gecko) leave scripted/structured content unobscured. The
  // child-bearing case (a replaced element that opts in but has painted
  // descendants) is covered by SkipsVideoWithUAShadowChildren.
  {
    SCOPED_TRACE("canvas");
    ExpectNonTrackable(
        "<div id='w'><canvas id='c' width='100' height='50'></canvas></div>",
        "c");
  }
  {
    SCOPED_TRACE("svg");
    ExpectNonTrackable(
        "<div id='w'><svg id='s' xmlns='http://www.w3.org/2000/svg' "
        "width='100' height='50'></svg></div>",
        "s");
  }
}

TEST_F(HighlightRegistryTest, SkipsLineBreak) {
  // Negative case for the IsTrackableReplacedElement gate: a <br> covered by a
  // highlight range must not be tracked. LayoutBR is a LayoutText subclass --
  // a selection leaf, but not replaced -- so the IsLayoutReplaced() half of the
  // gate is what excludes it. Without that gate the <br> would be tracked as
  // dead weight, since ReplacedPainter::PaintCustomHighlights (the consumer)
  // only paints LayoutReplaced boxes.
  ExpectNonTrackable("<div id='w'><br id='b'></div>", "b");
}

TEST_F(HighlightRegistryTest, TracksReplacedWithPlumbingChild) {
  // A replaced element whose only DOM children are "authoring plumbing"
  // (display:none, no layout object) must still be tracked. <param> inside
  // <object> is the canonical example: HTMLObjectElement ignores <param>
  // children when deciding whether to render fallback, and html.css sets
  // `param { display: none }` so no child layout object is produced.
  //
  // Gating on the layout tree (via CanBeSelectionLeaf()'s SlowFirstChild()
  // check) accepts this case because the host's layout object has no
  // painted descendants. Gating on the flat tree would incorrectly skip it
  // because <param> is a flat-tree child of <object>.
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='w'><object id='o' type='image/png' data='nonexistent.png' "
      "style='width:50px;height:50px;'>"
      "<param name='movie' value='ignored'>"
      "</object></div>");
  auto* wrapper = GetDocument().getElementById(AtomicString("w"));
  auto* object = To<Element>(GetDocument().getElementById(AtomicString("o")));
  ASSERT_TRUE(wrapper && object);
  UpdateAllLifecyclePhasesForTest();

  LayoutObject* object_layout = object->GetLayoutObject();
  ASSERT_TRUE(object_layout);
  ASSERT_FALSE(object_layout->SlowFirstChild());
  ASSERT_TRUE(object_layout->CanBeSelectionLeaf());

  auto* highlight = CreateChildIndexStaticRangeHighlight(wrapper, 0, 1);
  AtomicString name("h");
  HighlightRegistry* registry =
      HighlightRegistry::From(*GetDocument().domWindow());
  registry->SetForTesting(name, highlight);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(
      registry->GetActiveHighlightsForReplacedElement(*object).Contains(name));
}

TEST_F(HighlightRegistryTest, SkipsVideoWithUAShadowChildren) {
  // A <video> is a replaced element (LayoutVideo, a LayoutImage subclass),
  // but its user-agent shadow tree (the media-controls container) produces
  // child layout objects, so its layout object is not a selection leaf and
  // must not be tracked -- the same painted-children rule that excludes a
  // <canvas> with fallback content. This documents that the gate is not
  // "<img>-only": it turns on IsLayoutReplaced() + CanBeSelectionLeaf() for
  // any element, and <img> in the positive tests is representative of that
  // contract rather than a special case.
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='w'><video id='v' style='width:100px;height:50px;'></video>"
      "</div>");
  auto* wrapper = GetDocument().getElementById(AtomicString("w"));
  auto* video = To<Element>(GetDocument().getElementById(AtomicString("v")));
  ASSERT_TRUE(wrapper && video);
  UpdateAllLifecyclePhasesForTest();

  // Precondition: the <video> is replaced but has painted children, so it is
  // not a selection leaf. If a future change removes the UA child layout
  // objects, this test stops exercising the painted-children exclusion.
  LayoutObject* video_layout = video->GetLayoutObject();
  ASSERT_TRUE(video_layout);
  ASSERT_TRUE(video_layout->IsLayoutReplaced());
  ASSERT_TRUE(video_layout->SlowFirstChild());
  ASSERT_FALSE(video_layout->CanBeSelectionLeaf());

  auto* highlight = CreateChildIndexStaticRangeHighlight(wrapper, 0, 1);
  AtomicString name("h");
  HighlightRegistry* registry =
      HighlightRegistry::From(*GetDocument().domWindow());
  registry->SetForTesting(name, highlight);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(registry->GetActiveHighlightsForReplacedElement(*video).empty());
}

TEST_F(HighlightRegistryTest, MultipleHighlightsAppearInActiveSet) {
  // Two highlights covering the same replaced element must both register
  // in the active set so a stacked `::highlight(a), ::highlight(b)` rule
  // paints both tints.
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='w'><img id='i' style='width:50px;height:50px;'></div>");
  auto* wrapper = GetDocument().getElementById(AtomicString("w"));
  auto* img = To<Element>(GetDocument().getElementById(AtomicString("i")));
  ASSERT_TRUE(wrapper && img);

  AtomicString name_a("a");
  AtomicString name_b("b");
  HighlightRegistry* registry =
      HighlightRegistry::From(*GetDocument().domWindow());
  registry->SetForTesting(name_a,
                          CreateChildIndexStaticRangeHighlight(wrapper, 0, 1));
  registry->SetForTesting(name_b,
                          CreateChildIndexStaticRangeHighlight(wrapper, 0, 1));
  UpdateAllLifecyclePhasesForTest();

  const auto& active = registry->GetActiveHighlightsForReplacedElement(*img);
  EXPECT_EQ(2u, active.size());
  EXPECT_TRUE(active.Contains(name_a));
  EXPECT_TRUE(active.Contains(name_b));
}

TEST_F(HighlightRegistryTest,
       ReplacedElementTrackingCurrentColorResolvesAgainstPreviousLayer) {
  // A higher-priority ::highlight() rule with `background-color: currentColor`
  // must resolve currentColor against the *previous* layer's resolved current
  // color, matching the text-marker pipeline (HighlightOverlay::ComputeParts),
  // not against the host's own ComputedStyle::color. Without the fix, an <img>
  // covered by the same range as surrounding text gets a different visible
  // color.
  InsertStyleElement(
      "body { color: blue; }"
      "::highlight(low) { background-color: red; color: green; }"
      "::highlight(high) { background-color: currentColor; }");
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='w'><img id='i' style='width:50px;height:50px;'></div>");
  auto* wrapper = GetDocument().getElementById(AtomicString("w"));
  ASSERT_TRUE(wrapper);
  auto* img = To<Element>(GetDocument().getElementById(AtomicString("i")));
  ASSERT_TRUE(img);

  UpdateAllLifecyclePhasesForTest();

  auto build_wrapper_anchored_highlight = [&]() -> Highlight* {
    auto* init = StaticRangeInit::Create();
    init->setStartContainer(wrapper);
    init->setStartOffset(0);
    init->setEndContainer(wrapper);
    init->setEndOffset(1);
    auto* static_range = StaticRange::Create(init, ASSERT_NO_EXCEPTION);
    HeapVector<Member<AbstractRange>> ranges;
    ranges.push_back(static_range);
    return Highlight::Create(ranges);
  };

  auto* low = build_wrapper_anchored_highlight();
  auto* high = build_wrapper_anchored_highlight();
  AtomicString low_name("low");
  AtomicString high_name("high");

  HighlightRegistry* registry =
      HighlightRegistry::From(*GetDocument().domWindow());
  registry->SetForTesting(low_name, low);
  registry->SetForTesting(high_name, high);
  // Force `high` to paint on top of `low` regardless of registration order.
  low->setPriority(0);
  high->setPriority(1);

  UpdateAllLifecyclePhasesForTest();

  // Sanity-check that the highlight pseudo styles resolved as expected.
  const ComputedStyle& host_style = img->ComputedStyleRef();
  ASSERT_TRUE(HighlightStyleUtils::HighlightPseudoStyle(
      host_style, kPseudoIdHighlight, low_name));
  ASSERT_TRUE(HighlightStyleUtils::HighlightPseudoStyle(
      host_style, kPseudoIdHighlight, high_name));

  auto* layout_replaced = DynamicTo<LayoutReplaced>(img->GetLayoutObject());
  ASSERT_TRUE(layout_replaced);

  // The painter sorts active names by overlay stacking position
  // (lowest-priority first) before passing them into the helper. We
  // build the same order explicitly so the assertion is deterministic
  // and doesn't depend on the registry sort.
  Vector<AtomicString> sorted_names{low_name, high_name};

  PaintController controller;
  GraphicsContext context(controller);
  PaintInfo paint_info(context, CullRect(), PaintPhase::kForeground,
                       /*descendant_painting_blocked=*/false);

  Vector<Color> backgrounds =
      ReplacedPainter::ResolveStackedCustomHighlightBackgrounds(
          *layout_replaced, sorted_names, paint_info);

  ASSERT_EQ(2u, backgrounds.size())
      << "Both highlight layers must produce a non-transparent tint over the "
         "<img>.";
  // The low layer keeps its explicit `background-color: red`.
  EXPECT_EQ(Color::FromRGB(255, 0, 0), backgrounds[0]);
  // The high layer's `background-color: currentColor` must resolve to
  // the low layer's resolved current color (green), NOT the host's
  // own `color` (blue).
  EXPECT_EQ(Color::FromRGB(0, 128, 0), backgrounds[1])
      << "currentColor on the high layer must resolve against the previous "
         "layer's resolved color (green), matching the text-marker pipeline. ";
}

// ---------------------------------------------------------------------------
// Dynamic-mutation coverage for the replaced-element tracking logic.
//
// These tests pin down that the sideband stays in sync with the canonical
// marker-derived state across the mutations a real page can perform on a
// tracked replaced element. All paths funnel through
// ScheduleRepaintsInContainingHighlightRegistries() ->
// HighlightRegistry::ScheduleRepaint() -> force_markers_validation_ ->
// next ValidateHighlightMarkers() -> swap old/new map -> diff loop, so the
// active set updates without an explicit per-mutation hook.
// ---------------------------------------------------------------------------

TEST_F(HighlightRegistryTest, RangeMutationUpdatesActiveSet) {
  // Mutating a live Range so it now spans a different replaced element must
  // re-key the active set: the old image stops being tracked and the new
  // image starts being tracked. This exercises the diff loop via a DOM
  // version change driven by the Range mutation.
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='w'>"
      "<img id='img1' style='width:50px;height:50px;'>"
      "<img id='img2' style='width:50px;height:50px;'>"
      "</div>");
  auto* wrapper = GetDocument().getElementById(AtomicString("w"));
  auto* img1 = To<Element>(GetDocument().getElementById(AtomicString("img1")));
  auto* img2 = To<Element>(GetDocument().getElementById(AtomicString("img2")));
  ASSERT_TRUE(wrapper && img1 && img2);

  // Cover only img1 initially: (wrapper, 0, wrapper, 1).
  auto* range =
      MakeGarbageCollected<Range>(GetDocument(), wrapper, 0, wrapper, 1);
  HeapVector<Member<AbstractRange>> ranges;
  ranges.push_back(range);
  auto* highlight = Highlight::Create(ranges);
  AtomicString name("h");
  HighlightRegistry* registry =
      HighlightRegistry::From(*GetDocument().domWindow());
  registry->SetForTesting(name, highlight);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(
      registry->GetActiveHighlightsForReplacedElement(*img1).Contains(name));
  EXPECT_TRUE(registry->GetActiveHighlightsForReplacedElement(*img2).empty());

  // Move the range to cover only img2: (wrapper, 1, wrapper, 2).
  range->setStart(wrapper, 1);
  range->setEnd(wrapper, 2);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(registry->GetActiveHighlightsForReplacedElement(*img1).empty());
  EXPECT_TRUE(
      registry->GetActiveHighlightsForReplacedElement(*img2).Contains(name));
}

TEST_F(HighlightRegistryTest, DetachDoesNotCrash) {
  // Detaching a tracked replaced element must not crash the validator.
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='w'><img id='img' style='width:50px;height:50px;'></div>");
  auto* wrapper = GetDocument().getElementById(AtomicString("w"));
  auto* img = To<Element>(GetDocument().getElementById(AtomicString("img")));
  ASSERT_TRUE(wrapper && img);

  AtomicString name("h");
  HighlightRegistry* registry =
      HighlightRegistry::From(*GetDocument().domWindow());
  registry->SetForTesting(name,
                          CreateChildIndexStaticRangeHighlight(wrapper, 0, 1));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(
      registry->GetActiveHighlightsForReplacedElement(*img).Contains(name));

  // Detach the image from the DOM. The static range still holds onto the
  // img via its endpoint, but the img has no layout object and no parent.
  img->remove();

  // Re-validate; this must not crash. The active set lookup on the
  // detached img returns the empty set.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(registry->GetActiveHighlightsForReplacedElement(*img).empty());
}

TEST_F(HighlightRegistryTest, ReplaceWithNewElement) {
  // The range stays anchored at (wrapper, 0, wrapper, 1) and the DOM child at
  // that index is swapped. The diff loop must notice the old element dropped
  // out and the new one joined.
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='w'><img id='img1' style='width:50px;height:50px;'></div>");
  auto* wrapper = GetDocument().getElementById(AtomicString("w"));
  auto* img1 = To<Element>(GetDocument().getElementById(AtomicString("img1")));
  ASSERT_TRUE(wrapper && img1);

  AtomicString name("h");
  HighlightRegistry* registry =
      HighlightRegistry::From(*GetDocument().domWindow());
  registry->SetForTesting(name,
                          CreateChildIndexStaticRangeHighlight(wrapper, 0, 1));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(
      registry->GetActiveHighlightsForReplacedElement(*img1).Contains(name));

  // Swap img1 out for a fresh img2 in the same slot.
  auto* img2 = GetDocument().CreateElementForBinding(AtomicString("img"));
  img2->setAttribute(AtomicString("style"),
                     AtomicString("width:50px;height:50px;"));
  wrapper->ReplaceChild(img2, img1);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(registry->GetActiveHighlightsForReplacedElement(*img1).empty());
  EXPECT_TRUE(
      registry->GetActiveHighlightsForReplacedElement(*img2).Contains(name));
}

TEST_F(HighlightRegistryTest, DisplayNoneDropsActiveSet) {
  // Hiding the tracked replaced element with `display: none` drops its
  // layout object, so it stops being a selection leaf and must be removed
  // from the active set.
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='w'><img id='img' style='width:50px;height:50px;'></div>");
  auto* wrapper = GetDocument().getElementById(AtomicString("w"));
  auto* img = To<Element>(GetDocument().getElementById(AtomicString("img")));
  ASSERT_TRUE(wrapper && img);

  AtomicString name("h");
  HighlightRegistry* registry =
      HighlightRegistry::From(*GetDocument().domWindow());
  registry->SetForTesting(name,
                          CreateChildIndexStaticRangeHighlight(wrapper, 0, 1));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(
      registry->GetActiveHighlightsForReplacedElement(*img).Contains(name));

  img->setAttribute(AtomicString("style"),
                    AtomicString("display:none;width:50px;height:50px;"));
  UpdateAllLifecyclePhasesForTest();
  ASSERT_FALSE(img->GetLayoutObject());
  EXPECT_TRUE(registry->GetActiveHighlightsForReplacedElement(*img).empty());
}

TEST_F(HighlightRegistryTest,
       PriorityChangeInvalidatesPaintWithoutMembershipChange) {
  // Regression guard for the force_invalidate_replaced path. A
  // Highlight::setPriority() change does not alter which highlights cover a
  // replaced element, so the active-set diff in ValidateHighlightMarkers()
  // cannot witness it. The forced-invalidation path must therefore still flag
  // the replaced element for full paint invalidation, so its cached tint is
  // repainted in the new stacking order. The same path covers ::highlight()
  // style changes that alter the painted result without changing the active
  // set.
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='w'><img id='img' style='width:50px;height:50px;'></div>");
  auto* wrapper = GetDocument().getElementById(AtomicString("w"));
  auto* img = To<Element>(GetDocument().getElementById(AtomicString("img")));
  ASSERT_TRUE(wrapper && img);

  AtomicString name("h");
  auto* highlight = CreateChildIndexStaticRangeHighlight(wrapper, 0, 1);
  HighlightRegistry* registry =
      HighlightRegistry::From(*GetDocument().domWindow());
  registry->SetForTesting(name, highlight);
  UpdateAllLifecyclePhasesForTest();

  // Precondition: the img is tracked and paint has been flushed, so its layout
  // object no longer carries a pending full paint invalidation.
  LayoutObject* img_layout = img->GetLayoutObject();
  ASSERT_TRUE(img_layout);
  ASSERT_TRUE(
      registry->GetActiveHighlightsForReplacedElement(*img).Contains(name));
  ASSERT_FALSE(img_layout->ShouldDoFullPaintInvalidation());

  // Change only the priority and re-validate. This keeps the active
  // highlight-name set identical but forces re-validation.
  highlight->setPriority(1);
  registry->ValidateHighlightMarkers();

  // The active set is unchanged...
  const auto& active = registry->GetActiveHighlightsForReplacedElement(*img);
  EXPECT_EQ(1u, active.size());
  EXPECT_TRUE(active.Contains(name));
  // ...but the replaced element was still flagged for full paint invalidation.
  EXPECT_TRUE(img_layout->ShouldDoFullPaintInvalidation())
      << "A priority change that leaves the active set unchanged must still "
         "invalidate the replaced element's paint.";
}

TEST_F(HighlightRegistryTest, TracksSlottedImage) {
  // An <img> distributed through a <slot> must still be tracked. The
  // marker pipeline walks the DOM tree (NodeTraversal) over the
  // EphemeralRange's endpoints; slot distribution does not move the
  // <img> out of its parent's light-DOM children, so the walk visits it.
  // If this test starts failing, the traversal model and the
  // abstract-range endpoint model have diverged.
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='host'><img id='img' style='width:50px;height:50px;'></div>");
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ASSERT_TRUE(host);
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes("<slot></slot>");
  auto* img = To<Element>(GetDocument().getElementById(AtomicString("img")));
  ASSERT_TRUE(img);

  AtomicString name("h");
  HighlightRegistry* registry =
      HighlightRegistry::From(*GetDocument().domWindow());
  registry->SetForTesting(name,
                          CreateChildIndexStaticRangeHighlight(host, 0, 1));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(
      registry->GetActiveHighlightsForReplacedElement(*img).Contains(name));
}

TEST_F(HighlightRegistryFrameTest, AdoptedStaticRangePaintsInCurrentDocument) {
  SetBodyInnerHTML("<iframe></iframe><span id='span'>adopted highlight</span>");
  SetChildFrameHTML("");
  auto* span = GetDocument().getElementById(AtomicString("span"));
  auto* text = To<Text>(span->firstChild());
  auto* static_range =
      MakeGarbageCollected<StaticRange>(text, 0u, text, text->length());

  ChildDocument().adoptNode(span, ASSERT_NO_EXCEPTION);
  ChildDocument().body()->AppendChild(span);
  HeapVector<Member<AbstractRange>> ranges;
  ranges.push_back(static_range);
  auto* highlight = Highlight::Create(ranges);
  auto* registry = HighlightRegistry::From(*ChildDocument().domWindow());
  registry->SetForTesting(AtomicString("h"), highlight);
  RunDocumentLifecycle();

  DocumentMarkerVector markers = ChildDocument().Markers().MarkersFor(
      *text, DocumentMarker::MarkerTypes::CustomHighlight());
  EXPECT_FALSE(markers.empty());
}

TEST_F(HighlightRegistryFrameTest, MutateRegistryOfDetachedWindowDoesNotCrash) {
  SetBodyInnerHTML("<iframe id='f'></iframe><span id='span'>text</span>");
  SetChildFrameHTML("");
  auto* child_window = ChildDocument().domWindow();
  ASSERT_TRUE(child_window->GetFrame());

  // Detach the child frame without ever creating its HighlightRegistry, so the
  // registry below is created for a window that no longer has a frame.
  GetDocument().getElementById(AtomicString("f"))->remove();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_FALSE(child_window->GetFrame());

  auto* span = GetDocument().getElementById(AtomicString("span"));
  auto* text = To<Text>(span->firstChild());
  HeapVector<Member<AbstractRange>> ranges;
  ranges.push_back(
      MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 1));
  auto* highlight = Highlight::Create(ranges);

  auto* registry = HighlightRegistry::From(*child_window);
  AtomicString name("h");
  registry->SetForTesting(name, highlight);
  EXPECT_EQ(registry->size(), 1u);

  // Modifying a Highlight registered in the registry schedules a repaint on it
  // too.
  highlight->setPriority(1);

  // Hit testing has no frame to resolve coordinates against.
  EXPECT_TRUE(registry->highlightsFromPoint(0, 0, nullptr).empty());

  registry->RemoveForTesting(name, highlight);
  EXPECT_EQ(registry->size(), 0u);

  registry->SetForTesting(name, highlight);
  registry->clearForBinding(nullptr, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(registry->size(), 0u);
}

TEST_F(HighlightsFromPointTest, HighlightsFromPoint) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");

  auto* text = To<Text>(GetDocument().body()->firstChild());
  auto* highlight1 = CreateHighlight(text, 0, text, 4);
  AtomicString highlight1_name("TestHighlight1");
  auto* highlight2 = CreateHighlight(text, 3, text, 4);
  AtomicString highlight2_name("TestHighlight2");
  auto* range1 = highlight1->GetRanges().begin()->Get();
  auto* range2 = highlight2->GetRanges().begin()->Get();

  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  registry->SetForTesting(highlight1_name, highlight1);
  registry->SetForTesting(highlight2_name, highlight2);

  // When the document lifecycle runs, marker invalidation should
  // happen and create markers.
  UpdateAllLifecyclePhasesForTest();

  // Get markers at text node sorted by starting position.
  DocumentMarkerVector highlight_markers = GetDocument().Markers().MarkersFor(
      *text, DocumentMarker::MarkerTypes::CustomHighlight());

  // There's one marker from '1' to '4' and another one from '3' to '4'.
  EXPECT_EQ(highlight_markers.size(), 2u);

  // Test point in first marker, between '2' and '3', only |highlight1|.
  float x, y;
  GetMarkerCenterPoint(highlight_markers[0], text, x, y);
  auto highlight_hit_results_at_point =
      registry->highlightsFromPoint(x, y, nullptr);
  EXPECT_EQ(highlight_hit_results_at_point.size(), 1u);
  EXPECT_EQ(highlight_hit_results_at_point[0]->highlight(), highlight1);
  EXPECT_EQ(highlight_hit_results_at_point[0]->ranges().size(), 1u);
  EXPECT_EQ(highlight_hit_results_at_point[0]->ranges()[0], range1);

  // Test point in second marker, both highlights, same priority, break tie by
  // order of registration.
  GetMarkerCenterPoint(highlight_markers[1], text, x, y);
  highlight_hit_results_at_point = registry->highlightsFromPoint(x, y, nullptr);
  EXPECT_EQ(highlight_hit_results_at_point.size(), 2u);
  EXPECT_EQ(highlight_hit_results_at_point[0]->highlight(), highlight2);
  EXPECT_EQ(highlight_hit_results_at_point[0]->ranges().size(), 1u);
  EXPECT_EQ(highlight_hit_results_at_point[0]->ranges()[0], range2);
  EXPECT_EQ(highlight_hit_results_at_point[1]->highlight(), highlight1);
  EXPECT_EQ(highlight_hit_results_at_point[1]->ranges().size(), 1u);
  EXPECT_EQ(highlight_hit_results_at_point[1]->ranges()[0], range1);

  // Test point in second marker, both highlights, ordered by priority.
  highlight1->setPriority(2);
  highlight2->setPriority(1);
  highlight_hit_results_at_point = registry->highlightsFromPoint(x, y, nullptr);
  EXPECT_EQ(highlight_hit_results_at_point.size(), 2u);
  EXPECT_EQ(highlight_hit_results_at_point[0]->highlight(), highlight1);
  EXPECT_EQ(highlight_hit_results_at_point[0]->ranges().size(), 1u);
  EXPECT_EQ(highlight_hit_results_at_point[0]->ranges()[0], range1);
  EXPECT_EQ(highlight_hit_results_at_point[1]->highlight(), highlight2);
  EXPECT_EQ(highlight_hit_results_at_point[1]->ranges().size(), 1u);
  EXPECT_EQ(highlight_hit_results_at_point[1]->ranges()[0], range2);

  // Test points outside of markers.
  EXPECT_EQ(registry->highlightsFromPoint(-1, -1, nullptr).size(), 0u);
  EXPECT_EQ(registry->highlightsFromPoint(0, 0, nullptr).size(), 0u);
  EXPECT_EQ(registry->highlightsFromPoint(x * 3.0, y * 3.0, nullptr).size(),
            0u);

  // Test hitting multiple ranges from the same highlight.
  HeapVector<Member<AbstractRange>> range_vector;
  range_vector.push_back(range2);
  range_vector.push_back(range1);
  auto* highlight3 = Highlight::Create(range_vector);
  AtomicString highlight3_name("TestHighlight3");
  registry->RemoveForTesting(highlight1_name, highlight1);
  registry->RemoveForTesting(highlight2_name, highlight2);
  registry->SetForTesting(highlight3_name, highlight3);
  UpdateAllLifecyclePhasesForTest();

  // x and y are still set between '3' and '4', so we should get the two ranges
  // from highlight3.
  highlight_hit_results_at_point = registry->highlightsFromPoint(x, y, nullptr);
  EXPECT_EQ(highlight_hit_results_at_point.size(), 1u);
  EXPECT_EQ(highlight_hit_results_at_point[0]->highlight(), highlight3);
  EXPECT_EQ(highlight_hit_results_at_point[0]->ranges().size(), 2u);
  EXPECT_EQ(highlight_hit_results_at_point[0]->ranges()[0], range2);
  EXPECT_EQ(highlight_hit_results_at_point[0]->ranges()[1], range1);
}

TEST_F(HighlightsFromPointTest, HighlightsFromPointShadowRoot) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="host"></div>
  )HTML");
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes("<div>aaaa</div>");

  auto* text = To<Text>(shadow_root.firstChild()->firstChild());
  auto* highlight = CreateHighlight(text, 1, text, 3);
  auto* range = highlight->GetRanges().begin()->Get();
  AtomicString highlight_name("TestHighlight");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  registry->SetForTesting(highlight_name, highlight);

  // When the document lifecycle runs, marker invalidation should
  // happen and create markers.
  UpdateAllLifecyclePhasesForTest();

  // Get markers at text node.
  DocumentMarkerVector highlight_markers = GetDocument().Markers().MarkersFor(
      *text, DocumentMarker::MarkerTypes::CustomHighlight());

  // There's only one marker that looks like this: a[aa]a
  EXPECT_EQ(highlight_markers.size(), 1u);

  // Test point inside marker, no shadowRoots passed to function.
  float x, y;
  GetMarkerCenterPoint(highlight_markers[0], text, x, y);
  auto highlight_hit_results_at_point =
      registry->highlightsFromPoint(x, y, nullptr);
  EXPECT_EQ(highlight_hit_results_at_point.size(), 0u);

  // Test point inside marker, shadowRoot passed to function.
  HighlightsFromPointOptions* options =
      MakeGarbageCollected<HighlightsFromPointOptions>();
  options->setShadowRoots({&shadow_root});
  highlight_hit_results_at_point = registry->highlightsFromPoint(x, y, options);
  EXPECT_EQ(highlight_hit_results_at_point.size(), 1u);
  EXPECT_EQ(highlight_hit_results_at_point[0]->highlight(), highlight);
  EXPECT_EQ(highlight_hit_results_at_point[0]->ranges().size(), 1u);
  EXPECT_EQ(highlight_hit_results_at_point[0]->ranges()[0], range);

  // Test point outside marker but inside shadow root.
  x /= 3.0;
  highlight_hit_results_at_point = registry->highlightsFromPoint(x, y, options);
  EXPECT_EQ(highlight_hit_results_at_point.size(), 0u);
  highlight_hit_results_at_point = registry->highlightsFromPoint(x, y, nullptr);
  EXPECT_EQ(highlight_hit_results_at_point.size(), 0u);
}

TEST_F(HighlightRegistryTest, LiveIterationBasic) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* highlight1 = CreateHighlight(text, 0, text, 1);
  auto* highlight2 = CreateHighlight(text, 1, text, 2);
  auto* highlight3 = CreateHighlight(text, 2, text, 3);

  registry->SetForTesting(AtomicString("h1"), highlight1);
  registry->SetForTesting(AtomicString("h2"), highlight2);
  registry->SetForTesting(AtomicString("h3"), highlight3);

  auto* iter =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);
  String key;
  Highlight* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h1");
  EXPECT_EQ(value, highlight1);
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h2");
  EXPECT_EQ(value, highlight2);
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h3");
  EXPECT_EQ(value, highlight3);
  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));
}

TEST_F(HighlightRegistryTest, LiveIterationAddToEmpty) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* iter =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);

  auto* highlight1 = CreateHighlight(text, 0, text, 1);
  registry->SetForTesting(AtomicString("h1"), highlight1);

  String key;
  Highlight* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h1");
  EXPECT_EQ(value, highlight1);
  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));
}

TEST_F(HighlightRegistryTest, LiveIterationAddDuringIteration) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* highlight1 = CreateHighlight(text, 0, text, 1);
  auto* highlight2 = CreateHighlight(text, 1, text, 2);
  registry->SetForTesting(AtomicString("h1"), highlight1);
  registry->SetForTesting(AtomicString("h2"), highlight2);

  auto* iter =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);
  String key;
  Highlight* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h1");

  // Add h3 during iteration.
  auto* highlight3 = CreateHighlight(text, 2, text, 3);
  registry->SetForTesting(AtomicString("h3"), highlight3);

  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h2");
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h3");
  EXPECT_EQ(value, highlight3);
  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));
}

TEST_F(HighlightRegistryTest, LiveIterationDeleteOnlyItemBeforeVisit) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* highlight1 = CreateHighlight(text, 0, text, 1);
  registry->SetForTesting(AtomicString("h1"), highlight1);

  auto* iter =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);

  registry->RemoveForTesting(AtomicString("h1"), highlight1);

  String key;
  Highlight* value;
  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));
}

TEST_F(HighlightRegistryTest, LiveIterationDeleteNextBeforeVisit) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* highlight1 = CreateHighlight(text, 0, text, 1);
  auto* highlight2 = CreateHighlight(text, 1, text, 2);
  registry->SetForTesting(AtomicString("h1"), highlight1);
  registry->SetForTesting(AtomicString("h2"), highlight2);

  auto* iter =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);

  // Delete h2 before visiting it.
  registry->RemoveForTesting(AtomicString("h2"), highlight2);

  String key;
  Highlight* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h1");
  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));
}

TEST_F(HighlightRegistryTest, LiveIterationDeleteAlreadyVisited) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* highlight1 = CreateHighlight(text, 0, text, 1);
  auto* highlight2 = CreateHighlight(text, 1, text, 2);
  registry->SetForTesting(AtomicString("h1"), highlight1);
  registry->SetForTesting(AtomicString("h2"), highlight2);

  auto* iter =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);
  String key;
  Highlight* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h1");

  // Delete h1 (already visited) - should not affect continued iteration.
  registry->RemoveForTesting(AtomicString("h1"), highlight1);

  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h2");
  EXPECT_EQ(value, highlight2);
  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));
}

TEST_F(HighlightRegistryTest, LiveIterationClear) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* highlight1 = CreateHighlight(text, 0, text, 1);
  registry->SetForTesting(AtomicString("h1"), highlight1);

  auto* iter =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);

  DummyExceptionStateForTesting exception_state;
  registry->clearForBinding(nullptr, exception_state);

  String key;
  Highlight* value;
  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));
}

TEST_F(HighlightRegistryTest, LiveIterationAddAfterExhaustion) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* highlight1 = CreateHighlight(text, 0, text, 1);
  auto* highlight2 = CreateHighlight(text, 1, text, 2);
  registry->SetForTesting(AtomicString("h1"), highlight1);
  registry->SetForTesting(AtomicString("h2"), highlight2);

  auto* iter =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);
  String key;
  Highlight* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));

  // Add a new entry after the iterator was exhausted. Per Map live iteration
  // semantics, an exhausted iterator stays done permanently.
  auto* highlight3 = CreateHighlight(text, 2, text, 3);
  registry->SetForTesting(AtomicString("h3"), highlight3);

  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));
}

TEST_F(HighlightRegistryTest, LiveIterationDeleteLastReturnedThenAdd) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* highlight1 = CreateHighlight(text, 0, text, 1);
  registry->SetForTesting(AtomicString("h1"), highlight1);

  auto* iter =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);

  String key;
  Highlight* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h1");
  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));

  // Delete the last-returned entry, then add a new one. The iterator was
  // already exhausted, so it stays done permanently.
  registry->RemoveForTesting(AtomicString("h1"), highlight1);
  auto* highlight2 = CreateHighlight(text, 1, text, 2);
  registry->SetForTesting(AtomicString("h2"), highlight2);

  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));
}

TEST_F(HighlightRegistryTest, LiveIterationPausedDeleteLastReturnedThenAdd) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* highlight1 = CreateHighlight(text, 0, text, 1);
  registry->SetForTesting(AtomicString("h1"), highlight1);

  auto* iter =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);

  // Fetch h1 but do NOT call FetchNextItem again (iterator is paused,
  // not exhausted).
  String key;
  Highlight* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h1");

  // Delete the last-returned entry, then add a new one. Since the iterator
  // is paused (not exhausted), WillRemoveEntry updates last_returned_ so the
  // iterator can find the newly added entry.
  registry->RemoveForTesting(AtomicString("h1"), highlight1);
  auto* highlight2 = CreateHighlight(text, 1, text, 2);
  registry->SetForTesting(AtomicString("h2"), highlight2);

  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h2");
  EXPECT_EQ(value, highlight2);
  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));
}

TEST_F(HighlightRegistryTest, LiveIterationMultipleConcurrentIterators) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* highlight1 = CreateHighlight(text, 0, text, 1);
  auto* highlight2 = CreateHighlight(text, 1, text, 2);
  auto* highlight3 = CreateHighlight(text, 2, text, 3);
  registry->SetForTesting(AtomicString("h1"), highlight1);
  registry->SetForTesting(AtomicString("h2"), highlight2);
  registry->SetForTesting(AtomicString("h3"), highlight3);

  auto* iter1 =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);
  auto* iter2 =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);
  String key;
  Highlight* value;

  // Advance iter1 past h1. iter1.last_returned_ = h1's entry.
  EXPECT_TRUE(iter1->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h1");

  // Delete h2 - iter1 skips it (last_returned_ is h1's entry, so next lookup
  // advances past h1 to h3). iter2 hasn't started so is unaffected.
  registry->RemoveForTesting(AtomicString("h2"), highlight2);

  EXPECT_TRUE(iter1->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h3");
  EXPECT_FALSE(iter1->FetchNextItem(nullptr, key, value));

  // iter2 should see h1 and h3 (h2 was deleted).
  EXPECT_TRUE(iter2->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h1");
  EXPECT_TRUE(iter2->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h3");
  EXPECT_FALSE(iter2->FetchNextItem(nullptr, key, value));
}

TEST_F(HighlightRegistryTest, LiveIterationClearWithMultipleIterators) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* highlight1 = CreateHighlight(text, 0, text, 1);
  auto* highlight2 = CreateHighlight(text, 1, text, 2);
  registry->SetForTesting(AtomicString("h1"), highlight1);
  registry->SetForTesting(AtomicString("h2"), highlight2);

  auto* iter1 =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);
  auto* iter2 =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);

  // Advance iter1 past h1.
  String key;
  Highlight* value;
  EXPECT_TRUE(iter1->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h1");

  DummyExceptionStateForTesting exception_state;
  registry->clearForBinding(nullptr, exception_state);

  EXPECT_FALSE(iter1->FetchNextItem(nullptr, key, value));
  EXPECT_FALSE(iter2->FetchNextItem(nullptr, key, value));
}

TEST_F(HighlightRegistryTest, LiveIterationAddAfterDoneFromEmpty) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* iter =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);

  // Iterator is done on the empty registry - permanently exhausted.
  String key;
  Highlight* value;
  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));

  // Add an entry. The iterator was already exhausted, so it stays done.
  auto* highlight1 = CreateHighlight(text, 0, text, 1);
  registry->SetForTesting(AtomicString("h1"), highlight1);

  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));
}

TEST_F(HighlightRegistryTest, LiveIterationClearThenAdd) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* highlight1 = CreateHighlight(text, 0, text, 1);
  registry->SetForTesting(AtomicString("h1"), highlight1);

  auto* iter =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);

  DummyExceptionStateForTesting exception_state;
  registry->clearForBinding(nullptr, exception_state);

  // Add a new entry after clear. The iterator was not yet exhausted (it never
  // returned done=true), so it sees the newly added entry - matching JS Map
  // semantics where clear() marks entries as empty but the iterator continues.
  auto* highlight2 = CreateHighlight(text, 1, text, 2);
  registry->SetForTesting(AtomicString("h2"), highlight2);

  String key;
  Highlight* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h2");
  EXPECT_EQ(value, highlight2);
  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));
}

TEST_F(HighlightRegistryTest, LiveIterationClearNoAdd) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* highlight1 = CreateHighlight(text, 0, text, 1);
  registry->SetForTesting(AtomicString("h1"), highlight1);

  auto* iter =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);

  DummyExceptionStateForTesting exception_state;
  registry->clearForBinding(nullptr, exception_state);

  // After clear with no subsequent add, the iterator is exhausted. Further
  // adds should not revive it.
  String key;
  Highlight* value;
  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));

  auto* highlight2 = CreateHighlight(text, 1, text, 2);
  registry->SetForTesting(AtomicString("h2"), highlight2);

  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));
}

TEST_F(HighlightRegistryTest, LiveIterationDeleteAndReaddVisited) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* highlight1 = CreateHighlight(text, 0, text, 1);
  auto* highlight2 = CreateHighlight(text, 1, text, 2);
  registry->SetForTesting(AtomicString("h1"), highlight1);
  registry->SetForTesting(AtomicString("h2"), highlight2);

  auto* iter =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);
  String key;
  Highlight* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h1");

  // Delete h1 (already visited) and re-add it. Per JS Map semantics,
  // re-adding appends to the end and the iterator sees it again.
  registry->RemoveForTesting(AtomicString("h1"), highlight1);
  registry->SetForTesting(AtomicString("h1"), highlight1);

  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h2");
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h1");
  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));
}

TEST_F(HighlightRegistryTest, SetExistingKeyPreservesIterationOrder) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* highlight1 = CreateHighlight(text, 0, text, 1);
  auto* highlight2 = CreateHighlight(text, 1, text, 2);
  auto* highlight3 = CreateHighlight(text, 2, text, 3);
  registry->SetForTesting(AtomicString("h1"), highlight1);
  registry->SetForTesting(AtomicString("h2"), highlight2);
  registry->SetForTesting(AtomicString("h3"), highlight3);

  // Overwriting an existing key replaces the value in place: the size and the
  // iteration order stay the same, following Map semantics.
  auto* new_highlight1 = CreateHighlight(text, 0, text, 2);
  registry->SetForTesting(AtomicString("h1"), new_highlight1);
  EXPECT_EQ(registry->size(), 3u);

  auto* iter =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);
  String key;
  Highlight* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h1");
  EXPECT_EQ(value, new_highlight1);
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h2");
  EXPECT_EQ(value, highlight2);
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h3");
  EXPECT_EQ(value, highlight3);
  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));
}

TEST_F(HighlightRegistryTest, SetExistingKeyUpdatesStackingPosition) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  AtomicString highlight1_name("h1");
  AtomicString highlight2_name("h2");
  registry->SetForTesting(highlight1_name, CreateHighlight(text, 0, text, 4));
  registry->SetForTesting(highlight2_name, CreateHighlight(text, 2, text, 4));

  EXPECT_EQ(HighlightRegistry::kOverlayStackingPositionBelow,
            registry->CompareOverlayStackingPosition(highlight1_name,
                                                     highlight2_name));

  // Overwriting an existing key registers the Highlight again, so it keeps
  // stacking above the previously registered ones even though its position in
  // the iteration order doesn't change.
  registry->SetForTesting(highlight1_name, CreateHighlight(text, 0, text, 4));
  EXPECT_EQ(HighlightRegistry::kOverlayStackingPositionAbove,
            registry->CompareOverlayStackingPosition(highlight1_name,
                                                     highlight2_name));
}

TEST_F(HighlightRegistryTest, SetExistingKeyKeepsRegistrationBalanced) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  // Registering the same Highlight twice under the same name must not
  // increase the number of times it's considered registered in the registry,
  // otherwise removing it once would leave it registered forever.
  auto* highlight = CreateHighlight(text, 0, text, 1);
  AtomicString name("h1");
  registry->SetForTesting(name, highlight);
  registry->SetForTesting(name, highlight);
  registry->RemoveForTesting(name, highlight);
  EXPECT_EQ(registry->size(), 0u);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(registry->GetForceMarkersValidationForTesting());

  // The Highlight is no longer in the registry, so modifying it must not
  // schedule a repaint.
  highlight->setPriority(1);
  EXPECT_FALSE(registry->GetForceMarkersValidationForTesting());
}

TEST_F(HighlightRegistryTest, SetExistingKeyKeepsOtherNamesRegistered) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  // The same Highlight registered under two names is registered twice.
  auto* highlight = CreateHighlight(text, 0, text, 1);
  AtomicString name1("h1");
  AtomicString name2("h2");
  registry->SetForTesting(name1, highlight);
  registry->SetForTesting(name2, highlight);

  // Overwriting one of the names with a different Highlight leaves it
  // registered under the other name.
  registry->SetForTesting(name1, CreateHighlight(text, 1, text, 2));
  UpdateAllLifecyclePhasesForTest();
  ASSERT_FALSE(registry->GetForceMarkersValidationForTesting());
  highlight->setPriority(1);
  EXPECT_TRUE(registry->GetForceMarkersValidationForTesting());

  // Removing the remaining name deregisters it completely.
  registry->RemoveForTesting(name2, highlight);
  UpdateAllLifecyclePhasesForTest();
  ASSERT_FALSE(registry->GetForceMarkersValidationForTesting());
  highlight->setPriority(2);
  EXPECT_FALSE(registry->GetForceMarkersValidationForTesting());
}

TEST_F(HighlightRegistryTest, LiveIterationSetExistingKeyDuringIteration) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  registry->SetForTesting(AtomicString("h1"),
                          CreateHighlight(text, 0, text, 1));
  registry->SetForTesting(AtomicString("h2"),
                          CreateHighlight(text, 1, text, 2));
  registry->SetForTesting(AtomicString("h3"),
                          CreateHighlight(text, 2, text, 3));

  // Overwriting the entry the iterator is pointing at must not move it, so the
  // iteration visits every key exactly once and terminates.
  auto* iter =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);
  String key;
  Highlight* value;
  Vector<String> visited;
  while (iter->FetchNextItem(nullptr, key, value)) {
    visited.push_back(key);
    ASSERT_LE(visited.size(), 10u) << "iteration doesn't terminate";
    registry->SetForTesting(AtomicString(key),
                            CreateHighlight(text, 0, text, 4));
  }

  ASSERT_EQ(visited.size(), 3u);
  EXPECT_EQ(visited[0], "h1");
  EXPECT_EQ(visited[1], "h2");
  EXPECT_EQ(visited[2], "h3");
}

TEST_F(HighlightRegistryTest, LiveIterationSetExistingKeyBehindCursor) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* highlight3 = CreateHighlight(text, 2, text, 3);
  registry->SetForTesting(AtomicString("h1"),
                          CreateHighlight(text, 0, text, 1));
  registry->SetForTesting(AtomicString("h2"),
                          CreateHighlight(text, 1, text, 2));
  registry->SetForTesting(AtomicString("h3"), highlight3);

  auto* iter =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);
  String key;
  Highlight* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h1");
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h2");

  // Overwriting a key the iterator has already moved past doesn't move it to
  // the end, so the iterator doesn't visit it a second time.
  registry->SetForTesting(AtomicString("h1"),
                          CreateHighlight(text, 0, text, 4));

  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h3");
  EXPECT_EQ(value, highlight3);
  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));
}

TEST_F(HighlightRegistryTest, LiveIterationSetExistingKeyAheadOfCursor) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* highlight3 = CreateHighlight(text, 2, text, 3);
  registry->SetForTesting(AtomicString("h1"),
                          CreateHighlight(text, 0, text, 1));
  registry->SetForTesting(AtomicString("h2"),
                          CreateHighlight(text, 1, text, 2));
  registry->SetForTesting(AtomicString("h3"), highlight3);

  auto* iter =
      MakeGarbageCollected<HighlightRegistry::IterationSource>(*registry);
  String key;
  Highlight* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h1");

  // Overwriting a key the iterator hasn't reached yet keeps it in place, so it
  // is still visited in its original position with the new Highlight.
  auto* new_highlight2 = CreateHighlight(text, 0, text, 4);
  registry->SetForTesting(AtomicString("h2"), new_highlight2);

  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h2");
  EXPECT_EQ(value, new_highlight2);
  EXPECT_TRUE(iter->FetchNextItem(nullptr, key, value));
  EXPECT_EQ(key, "h3");
  EXPECT_EQ(value, highlight3);
  EXPECT_FALSE(iter->FetchNextItem(nullptr, key, value));
}

}  // namespace blink
