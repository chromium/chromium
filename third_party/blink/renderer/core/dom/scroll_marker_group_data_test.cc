// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scroll_marker_group_data.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/column_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_group_pseudo_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class ScrollMarkerGroupDataTest : public RenderingTest {
 protected:
  void SetupSampleHTML(const char* main_html) {
    SetBodyInnerHTML(String::FromUtf8(main_html));
  }
};

TEST_F(ScrollMarkerGroupDataTest, SelectedMarkerReturnsNullIfDisconnected) {
  SetupSampleHTML(R"(
    <style>
      #container {
        overflow: scroll;
        scroll-marker-group: before;
      }
      #container::scroll-marker-group {
        display: flex;
      }
      #item::scroll-marker {
        content: ' ';
      }
    </style>
    <div id='container'>
      <div id='item'></div>
    </div>
  )");
  UpdateAllLifecyclePhasesForTest();

  Element* container = GetDocument().QuerySelector(AtomicString("#container"));
  Element* item = GetDocument().QuerySelector(AtomicString("#item"));
  PseudoElement* smg =
      container->GetPseudoElement(kPseudoIdScrollMarkerGroupBefore);
  PseudoElement* sm = item->GetPseudoElement(kPseudoIdScrollMarker);

  EXPECT_TRUE(smg);
  EXPECT_TRUE(sm);

  auto* smg_pseudo = To<ScrollMarkerGroupPseudoElement>(smg);
  smg_pseudo->SetSelected(*To<ScrollMarkerPseudoElement>(sm));

  EXPECT_EQ(sm, smg_pseudo->Selected());

  // Remove item from DOM to disconnect the marker.
  item->remove();

  // Now sm is disconnected. Selected() should return null.
  EXPECT_EQ(nullptr, smg_pseudo->Selected());
}

// Activating a ::column::scroll-marker must select and pin that very marker,
// regardless of whether the ::scroll-marker-group is laid out before or after
// the scroller. See crbug.com/566965154.
TEST_F(ScrollMarkerGroupDataTest, ActivateColumnScrollMarkerSelectsAndPinsIt) {
  SetupSampleHTML(R"(
    <style>
      .scroller {
        overflow: hidden;
        columns: 1;
        column-fill: auto;
        gap: 0;
        width: 300px;
        height: 100px;
      }
      .scroller::scroll-marker-group {
        display: flex;
      }
      .scroller::column::scroll-marker {
        content: '';
        width: 10px;
        height: 10px;
      }
      #before {
        scroll-marker-group: before;
      }
      #after {
        scroll-marker-group: after;
      }
      .content {
        height: 300px;
      }
    </style>
    <div id='before' class='scroller'><div class='content'></div></div>
    <div id='after' class='scroller'><div class='content'></div></div>
  )");
  UpdateAllLifecyclePhasesForTest();

  struct {
    const char* id;
    PseudoId group_pseudo_id;
  } cases[] = {{"before", kPseudoIdScrollMarkerGroupBefore},
               {"after", kPseudoIdScrollMarkerGroupAfter}};
  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.id);
    Element* scroller = GetElementById(test_case.id);
    auto* group = To<ScrollMarkerGroupPseudoElement>(
        scroller->GetPseudoElement(test_case.group_pseudo_id));
    ASSERT_TRUE(group);
    const ColumnPseudoElementsVector* columns =
        scroller->GetColumnPseudoElements();
    ASSERT_TRUE(columns);
    ASSERT_EQ(3u, columns->size());
    auto* second_marker = To<ScrollMarkerPseudoElement>(
        columns->at(1)->GetPseudoElement(kPseudoIdScrollMarker));
    ASSERT_TRUE(second_marker);
    EXPECT_NE(second_marker, group->Selected());

    group->ActivateScrollMarker(second_marker);
    UpdateAllLifecyclePhasesForTest();

    EXPECT_EQ(second_marker, group->Selected());
    EXPECT_TRUE(group->SelectedMarkerIsPinned());
  }
}

}  // namespace blink
