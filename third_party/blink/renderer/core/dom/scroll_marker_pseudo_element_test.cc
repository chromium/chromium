// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scroll_marker_pseudo_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

using ScrollMarkerPseudoElementTest = PageTestBase;

TEST_F(ScrollMarkerPseudoElementTest, NoNestedMarkersCount) {
  SetBodyContent(R"HTML(
    <style>
      #scroller {
        overflow: scroll;
        width: 100px;
        height: 100px;
        scroll-marker-group: after;
      }
      .marker::scroll-marker {
        content: "";
      }
    </style>
    <div id="scroller">
      <div class="marker"></div>
      <div class="marker"></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kNestedScrollMarkers));
}

TEST_F(ScrollMarkerPseudoElementTest, NestedMarkersCounted) {
  SetBodyContent(R"HTML(
    <style>
      #scroller {
        overflow: scroll;
        width: 100px;
        height: 100px;
        scroll-marker-group: after;
      }
      .marker::scroll-marker {
        content: "";
      }
    </style>
    <div id="scroller">
      <div class="marker">
        <div class="marker"></div>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kNestedScrollMarkers));
}

}  // namespace blink
