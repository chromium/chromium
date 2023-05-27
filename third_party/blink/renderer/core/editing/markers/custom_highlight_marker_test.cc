// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/highlight/highlight.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class CustomHighlightMarkerTest : public PageTestBase {};

TEST_F(CustomHighlightMarkerTest, CreationAndProperties) {
  GetDocument().body()->setInnerHTML("1234");
  auto* text = To<Text>(GetDocument().body()->firstChild());
  auto* range04 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 4);
  HeapVector<Member<AbstractRange>> range_vector;
  range_vector.push_back(range04);
  auto* highlight = Highlight::Create(range_vector);

  DocumentMarker* marker = MakeGarbageCollected<CustomHighlightMarker>(
      0, 4, "TestHighlight", highlight);
  // Check downcast operator.
  CustomHighlightMarker* custom_marker = To<CustomHighlightMarker>(marker);
  EXPECT_EQ(DocumentMarker::kCustomHighlight, custom_marker->GetType());
  EXPECT_EQ(kPseudoIdHighlight, custom_marker->GetPseudoId());
  EXPECT_EQ("TestHighlight", custom_marker->GetPseudoArgument());
}

}  // namespace blink
