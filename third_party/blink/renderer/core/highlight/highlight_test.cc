// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/highlight/highlight.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class HighlightTest : public PageTestBase {};

TEST_F(HighlightTest, Creation) {
  GetDocument().body()->setInnerHTML("1234");
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* range04 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 4);
  auto* range02 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 2);
  auto* range22 = MakeGarbageCollected<Range>(GetDocument(), text, 2, text, 2);

  HeapVector<Member<AbstractRange>> range_vector;
  range_vector.push_back(range04);
  range_vector.push_back(range02);
  range_vector.push_back(range22);

  auto* highlight = Highlight::Create(range_vector);
  CHECK_EQ(3u, highlight->size());
  CHECK_EQ(3u, highlight->GetRanges().size());
  EXPECT_TRUE(highlight->Contains(range04));
  EXPECT_TRUE(highlight->Contains(range02));
  EXPECT_TRUE(highlight->Contains(range22));
}

TEST_F(HighlightTest, Properties) {
  GetDocument().body()->setInnerHTML("1234");
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* range04 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 4);

  HeapVector<Member<AbstractRange>> range_vector;
  range_vector.push_back(range04);

  auto* highlight = Highlight::Create(range_vector);
  highlight->setPriority(1);
  highlight->setType(V8HighlightType(V8HighlightType::Enum::kSpellingError));

  CHECK_EQ(1, highlight->priority());
  CHECK_EQ("spelling-error", highlight->type().AsString());
}

}  // namespace blink
