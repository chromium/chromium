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
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class HighlightTest : public PageTestBase {};

TEST_F(HighlightTest, Creation) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
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
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
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

TEST_F(HighlightTest, LiveIterationBasic) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* range1 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 1);
  auto* range2 = MakeGarbageCollected<Range>(GetDocument(), text, 1, text, 2);
  auto* range3 = MakeGarbageCollected<Range>(GetDocument(), text, 2, text, 3);

  HeapVector<Member<AbstractRange>> ranges;
  ranges.push_back(range1);
  ranges.push_back(range2);
  ranges.push_back(range3);
  auto* highlight = Highlight::Create(ranges);

  auto* iter = MakeGarbageCollected<Highlight::IterationSource>(*highlight);
  AbstractRange* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range1);
  EXPECT_TRUE(iter->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range2);
  EXPECT_TRUE(iter->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range3);
  EXPECT_FALSE(iter->FetchNextItem(nullptr, value));
}

TEST_F(HighlightTest, LiveIterationAddToEmpty) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* text = To<Text>(GetDocument().body()->firstChild());

  HeapVector<Member<AbstractRange>> empty;
  auto* highlight = Highlight::Create(empty);

  auto* iter = MakeGarbageCollected<Highlight::IterationSource>(*highlight);

  auto* range1 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 1);
  DummyExceptionStateForTesting exception_state;
  highlight->addForBinding(nullptr, range1, exception_state);

  AbstractRange* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range1);
  EXPECT_FALSE(iter->FetchNextItem(nullptr, value));
}

TEST_F(HighlightTest, LiveIterationAddDuringIteration) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* range1 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 1);
  auto* range2 = MakeGarbageCollected<Range>(GetDocument(), text, 1, text, 2);

  HeapVector<Member<AbstractRange>> ranges;
  ranges.push_back(range1);
  ranges.push_back(range2);
  auto* highlight = Highlight::Create(ranges);

  auto* iter = MakeGarbageCollected<Highlight::IterationSource>(*highlight);
  AbstractRange* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range1);

  // Add range3 after fetching range1 but before fetching range2.
  auto* range3 = MakeGarbageCollected<Range>(GetDocument(), text, 2, text, 3);
  DummyExceptionStateForTesting exception_state;
  highlight->addForBinding(nullptr, range3, exception_state);

  EXPECT_TRUE(iter->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range2);
  EXPECT_TRUE(iter->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range3);
  EXPECT_FALSE(iter->FetchNextItem(nullptr, value));
}

TEST_F(HighlightTest, LiveIterationDeleteOnlyItemBeforeVisit) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* range1 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 1);

  HeapVector<Member<AbstractRange>> ranges;
  ranges.push_back(range1);
  auto* highlight = Highlight::Create(ranges);

  auto* iter = MakeGarbageCollected<Highlight::IterationSource>(*highlight);

  DummyExceptionStateForTesting exception_state;
  highlight->deleteForBinding(nullptr, range1, exception_state);

  AbstractRange* value;
  EXPECT_FALSE(iter->FetchNextItem(nullptr, value));
}

TEST_F(HighlightTest, LiveIterationDeleteNextBeforeVisit) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* range1 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 1);
  auto* range2 = MakeGarbageCollected<Range>(GetDocument(), text, 1, text, 2);

  HeapVector<Member<AbstractRange>> ranges;
  ranges.push_back(range1);
  ranges.push_back(range2);
  auto* highlight = Highlight::Create(ranges);

  auto* iter = MakeGarbageCollected<Highlight::IterationSource>(*highlight);

  // Delete range2 (the next-to-be-visited after range1) before visiting it.
  DummyExceptionStateForTesting exception_state;
  highlight->deleteForBinding(nullptr, range2, exception_state);

  AbstractRange* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range1);
  EXPECT_FALSE(iter->FetchNextItem(nullptr, value));
}

TEST_F(HighlightTest, LiveIterationDeleteAlreadyVisited) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* range1 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 1);
  auto* range2 = MakeGarbageCollected<Range>(GetDocument(), text, 1, text, 2);

  HeapVector<Member<AbstractRange>> ranges;
  ranges.push_back(range1);
  ranges.push_back(range2);
  auto* highlight = Highlight::Create(ranges);

  auto* iter = MakeGarbageCollected<Highlight::IterationSource>(*highlight);
  AbstractRange* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range1);

  // Delete range1 (already visited) - should not affect continued iteration.
  DummyExceptionStateForTesting exception_state;
  highlight->deleteForBinding(nullptr, range1, exception_state);

  EXPECT_TRUE(iter->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range2);
  EXPECT_FALSE(iter->FetchNextItem(nullptr, value));
}

TEST_F(HighlightTest, LiveIterationClear) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* range1 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 1);

  HeapVector<Member<AbstractRange>> ranges;
  ranges.push_back(range1);
  auto* highlight = Highlight::Create(ranges);

  auto* iter = MakeGarbageCollected<Highlight::IterationSource>(*highlight);

  DummyExceptionStateForTesting exception_state;
  highlight->clearForBinding(nullptr, exception_state);

  AbstractRange* value;
  EXPECT_FALSE(iter->FetchNextItem(nullptr, value));
}

TEST_F(HighlightTest, LiveIterationAddAfterExhaustion) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* range1 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 1);
  auto* range2 = MakeGarbageCollected<Range>(GetDocument(), text, 1, text, 2);

  HeapVector<Member<AbstractRange>> ranges;
  ranges.push_back(range1);
  ranges.push_back(range2);
  auto* highlight = Highlight::Create(ranges);

  auto* iter = MakeGarbageCollected<Highlight::IterationSource>(*highlight);

  AbstractRange* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, value));
  EXPECT_TRUE(iter->FetchNextItem(nullptr, value));
  EXPECT_FALSE(iter->FetchNextItem(nullptr, value));

  // Add a new range after the iterator was exhausted. Per Set live iteration
  // semantics, an exhausted iterator stays done permanently.
  auto* range3 = MakeGarbageCollected<Range>(GetDocument(), text, 2, text, 3);
  DummyExceptionStateForTesting exception_state;
  highlight->addForBinding(nullptr, range3, exception_state);

  EXPECT_FALSE(iter->FetchNextItem(nullptr, value));
}

TEST_F(HighlightTest, LiveIterationDeleteLastReturnedThenAdd) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* range1 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 1);

  HeapVector<Member<AbstractRange>> ranges;
  ranges.push_back(range1);
  auto* highlight = Highlight::Create(ranges);

  auto* iter = MakeGarbageCollected<Highlight::IterationSource>(*highlight);

  AbstractRange* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range1);
  EXPECT_FALSE(iter->FetchNextItem(nullptr, value));

  // Delete the last-returned range, then add a new one. The iterator was
  // already exhausted, so it stays done permanently.
  DummyExceptionStateForTesting exception_state;
  highlight->deleteForBinding(nullptr, range1, exception_state);
  auto* range2 = MakeGarbageCollected<Range>(GetDocument(), text, 1, text, 2);
  highlight->addForBinding(nullptr, range2, exception_state);

  EXPECT_FALSE(iter->FetchNextItem(nullptr, value));
}

TEST_F(HighlightTest, LiveIterationPausedDeleteLastReturnedThenAdd) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* range1 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 1);

  HeapVector<Member<AbstractRange>> ranges;
  ranges.push_back(range1);
  auto* highlight = Highlight::Create(ranges);

  auto* iter = MakeGarbageCollected<Highlight::IterationSource>(*highlight);

  // Fetch range1 but do NOT call FetchNextItem again (iterator is paused,
  // not exhausted).
  AbstractRange* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range1);

  // Delete the last-returned range, then add a new one. Since the iterator
  // is paused (not exhausted), WillRemoveItem updates last_returned_ so the
  // iterator can find the newly added item.
  DummyExceptionStateForTesting exception_state;
  highlight->deleteForBinding(nullptr, range1, exception_state);
  auto* range2 = MakeGarbageCollected<Range>(GetDocument(), text, 1, text, 2);
  highlight->addForBinding(nullptr, range2, exception_state);

  EXPECT_TRUE(iter->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range2);
  EXPECT_FALSE(iter->FetchNextItem(nullptr, value));
}

TEST_F(HighlightTest, LiveIterationMultipleConcurrentIterators) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* range1 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 1);
  auto* range2 = MakeGarbageCollected<Range>(GetDocument(), text, 1, text, 2);
  auto* range3 = MakeGarbageCollected<Range>(GetDocument(), text, 2, text, 3);

  HeapVector<Member<AbstractRange>> ranges;
  ranges.push_back(range1);
  ranges.push_back(range2);
  ranges.push_back(range3);
  auto* highlight = Highlight::Create(ranges);

  auto* iter1 = MakeGarbageCollected<Highlight::IterationSource>(*highlight);
  auto* iter2 = MakeGarbageCollected<Highlight::IterationSource>(*highlight);
  AbstractRange* value;

  // Advance iter1 past range1. iter1.last_returned_ = range1.
  EXPECT_TRUE(iter1->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range1);

  // Delete range2 - iter1 skips it (last_returned_ is range1, so next lookup
  // advances past range1 to range3). iter2 hasn't started so is unaffected.
  DummyExceptionStateForTesting exception_state;
  highlight->deleteForBinding(nullptr, range2, exception_state);

  EXPECT_TRUE(iter1->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range3);
  EXPECT_FALSE(iter1->FetchNextItem(nullptr, value));

  // iter2 should see range1 and range3 (range2 was deleted).
  EXPECT_TRUE(iter2->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range1);
  EXPECT_TRUE(iter2->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range3);
  EXPECT_FALSE(iter2->FetchNextItem(nullptr, value));
}

TEST_F(HighlightTest, LiveIterationClearWithMultipleIterators) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* range1 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 1);
  auto* range2 = MakeGarbageCollected<Range>(GetDocument(), text, 1, text, 2);

  HeapVector<Member<AbstractRange>> ranges;
  ranges.push_back(range1);
  ranges.push_back(range2);
  auto* highlight = Highlight::Create(ranges);

  auto* iter1 = MakeGarbageCollected<Highlight::IterationSource>(*highlight);
  auto* iter2 = MakeGarbageCollected<Highlight::IterationSource>(*highlight);

  // Advance iter1 past range1.
  AbstractRange* value;
  EXPECT_TRUE(iter1->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range1);

  DummyExceptionStateForTesting exception_state;
  highlight->clearForBinding(nullptr, exception_state);

  EXPECT_FALSE(iter1->FetchNextItem(nullptr, value));
  EXPECT_FALSE(iter2->FetchNextItem(nullptr, value));
}

TEST_F(HighlightTest, LiveIterationAddAfterDoneFromEmpty) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* text = To<Text>(GetDocument().body()->firstChild());

  HeapVector<Member<AbstractRange>> empty;
  auto* highlight = Highlight::Create(empty);

  auto* iter = MakeGarbageCollected<Highlight::IterationSource>(*highlight);

  // Iterator is done on the empty set - permanently exhausted.
  AbstractRange* value;
  EXPECT_FALSE(iter->FetchNextItem(nullptr, value));

  // Add a range. The iterator was already exhausted, so it stays done.
  auto* range1 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 1);
  DummyExceptionStateForTesting exception_state;
  highlight->addForBinding(nullptr, range1, exception_state);

  EXPECT_FALSE(iter->FetchNextItem(nullptr, value));
}

TEST_F(HighlightTest, LiveIterationClearThenAdd) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* range1 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 1);

  HeapVector<Member<AbstractRange>> ranges;
  ranges.push_back(range1);
  auto* highlight = Highlight::Create(ranges);

  auto* iter = MakeGarbageCollected<Highlight::IterationSource>(*highlight);

  DummyExceptionStateForTesting exception_state;
  highlight->clearForBinding(nullptr, exception_state);

  // Add a new range after clear. The iterator was not yet exhausted (it never
  // returned done=true), so it sees the newly added item - matching JS Set
  // semantics where clear() marks entries as empty but the iterator continues.
  auto* range2 = MakeGarbageCollected<Range>(GetDocument(), text, 1, text, 2);
  highlight->addForBinding(nullptr, range2, exception_state);

  AbstractRange* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range2);
  EXPECT_FALSE(iter->FetchNextItem(nullptr, value));
}

TEST_F(HighlightTest, LiveIterationClearNoAdd) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* range1 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 1);

  HeapVector<Member<AbstractRange>> ranges;
  ranges.push_back(range1);
  auto* highlight = Highlight::Create(ranges);

  auto* iter = MakeGarbageCollected<Highlight::IterationSource>(*highlight);

  DummyExceptionStateForTesting exception_state;
  highlight->clearForBinding(nullptr, exception_state);

  // After clear with no subsequent add, the iterator is exhausted. Further
  // adds should not revive it.
  AbstractRange* value;
  EXPECT_FALSE(iter->FetchNextItem(nullptr, value));

  auto* range2 = MakeGarbageCollected<Range>(GetDocument(), text, 1, text, 2);
  highlight->addForBinding(nullptr, range2, exception_state);

  EXPECT_FALSE(iter->FetchNextItem(nullptr, value));
}

TEST_F(HighlightTest, LiveIterationDeleteAndReaddVisited) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("1234");
  auto* text = To<Text>(GetDocument().body()->firstChild());

  auto* range1 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 1);
  auto* range2 = MakeGarbageCollected<Range>(GetDocument(), text, 1, text, 2);

  HeapVector<Member<AbstractRange>> ranges;
  ranges.push_back(range1);
  ranges.push_back(range2);
  auto* highlight = Highlight::Create(ranges);

  auto* iter = MakeGarbageCollected<Highlight::IterationSource>(*highlight);
  AbstractRange* value;
  EXPECT_TRUE(iter->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range1);

  // Delete range1 (already visited) and re-add it. Per JS Set semantics,
  // re-adding appends to the end and the iterator sees it again.
  DummyExceptionStateForTesting exception_state;
  highlight->deleteForBinding(nullptr, range1, exception_state);
  highlight->addForBinding(nullptr, range1, exception_state);

  EXPECT_TRUE(iter->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range2);
  EXPECT_TRUE(iter->FetchNextItem(nullptr, value));
  EXPECT_EQ(value, range1);
  EXPECT_FALSE(iter->FetchNextItem(nullptr, value));
}

}  // namespace blink
