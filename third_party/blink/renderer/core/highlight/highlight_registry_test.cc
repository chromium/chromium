// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/highlight/highlight_registry.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/highlight/highlight.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

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

TEST_F(HighlightRegistryTest, CompareStacking) {
  GetDocument().body()->setInnerHTML("1234");
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
            registry->CompareOverlayStackingPosition(
                highlight1_name, highlight1, highlight1_name, highlight1));
  EXPECT_EQ(HighlightRegistry::kOverlayStackingPositionBelow,
            registry->CompareOverlayStackingPosition(
                highlight1_name, highlight1, highlight2_name, highlight2));
  EXPECT_EQ(HighlightRegistry::kOverlayStackingPositionAbove,
            registry->CompareOverlayStackingPosition(
                highlight2_name, highlight2, highlight1_name, highlight1));
  highlight1->setPriority(2);
  highlight1->setPriority(1);
  EXPECT_EQ(HighlightRegistry::kOverlayStackingPositionAbove,
            registry->CompareOverlayStackingPosition(
                highlight1_name, highlight1, highlight2_name, highlight2));
}

TEST_F(HighlightRegistryTest, ValidateMarkers) {
  GetDocument().body()->setInnerHTML(
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

}  // namespace blink
