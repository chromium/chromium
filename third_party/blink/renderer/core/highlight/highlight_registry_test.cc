// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/highlight/highlight_registry.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_highlight_hit_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_highlights_from_point_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/highlight/highlight.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
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

}  // namespace blink
