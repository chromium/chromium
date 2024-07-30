// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/same_block_word_iterator.h"

#include <gtest/gtest.h>

#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/fragment_directive/same_block_word_iterator.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class SameBlockWordIteratorTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  }
};

// Basic case for forward iterator->
TEST_F(SameBlockWordIteratorTest, GetNextWord) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
    <p>new block</p>
  )HTML");
  Node* node =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  ForwardSameBlockWordIterator* iterator =
      MakeGarbageCollected<ForwardSameBlockWordIterator>(
          PositionInFlatTree(*node, 0));
  iterator->AdvanceNextWord();
  EXPECT_EQ("First", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("First paragraph", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("First paragraph text", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("First paragraph text", iterator->TextFromStart());
}

// Check the case when following text contains collapsible space.
TEST_F(SameBlockWordIteratorTest, GetNextWord_ExtraSpace) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph


     text</p>
  )HTML");
  Node* node =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  ForwardSameBlockWordIterator* iterator =
      MakeGarbageCollected<ForwardSameBlockWordIterator>(
          PositionInFlatTree(*node, 6));
  iterator->AdvanceNextWord();
  EXPECT_EQ("paragraph", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("paragraph text", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("paragraph text", iterator->TextFromStart());
}

// Check the case when there is a commented block which should be skipped.
TEST_F(SameBlockWordIteratorTest, GetNextWord_WithComment) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p>new block</p>
    <div id='first'>
      <span>First</span>
      <!--
      multiline comment that should be ignored.
      //-->
      <span id='span'>paragraph text</span>
    </div>
    <p>new block</p>
  )HTML");
  Node* node = GetDocument().getElementById(AtomicString("span"))->firstChild();
  BackwardSameBlockWordIterator* iterator =
      MakeGarbageCollected<BackwardSameBlockWordIterator>(
          PositionInFlatTree(*node, 9));
  iterator->AdvanceNextWord();
  EXPECT_EQ("paragraph", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("First paragraph", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("First paragraph", iterator->TextFromStart());
}

// Check the case when following text contains non-block tag(e.g. <b>).
TEST_F(SameBlockWordIteratorTest, GetNextWord_NestedTextNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First <b>bold text</b> paragraph text</p>
  )HTML");
  Node* node =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  ForwardSameBlockWordIterator* iterator =
      MakeGarbageCollected<ForwardSameBlockWordIterator>(
          PositionInFlatTree(*node, 5));
  iterator->AdvanceNextWord();
  EXPECT_EQ("bold", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("bold text", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("bold text paragraph", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("bold text paragraph text", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("bold text paragraph text", iterator->TextFromStart());
}

// Check the case when following text is interrupted by a nested block.
TEST_F(SameBlockWordIteratorTest, GetNextWord_NestedBlock) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First paragraph <div id='div'>div</div> text</div>
  )HTML");
  Node* node =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  ForwardSameBlockWordIterator* iterator =
      MakeGarbageCollected<ForwardSameBlockWordIterator>(
          PositionInFlatTree(*node, 5));
  iterator->AdvanceNextWord();
  EXPECT_EQ("paragraph", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("paragraph", iterator->TextFromStart());
}

// Check the case when following text includes non-block element but is
// interrupted by a nested block.
TEST_F(SameBlockWordIteratorTest, GetNextWord_NestedBlockInNestedText) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First <b>bold<div id='div'>div</div></b> paragraph text</div>
  )HTML");
  Node* node =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  ForwardSameBlockWordIterator* iterator =
      MakeGarbageCollected<ForwardSameBlockWordIterator>(
          PositionInFlatTree(*node, 5));
  iterator->AdvanceNextWord();
  EXPECT_EQ("bold", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("bold", iterator->TextFromStart());
}

// Check the case when following text includes invisible block.
TEST_F(SameBlockWordIteratorTest, GetNextWord_NestedInvisibleBlock) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First <div id='div' style='display:none'>invisible</div> paragraph text</div>
  )HTML");
  Node* node =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  ForwardSameBlockWordIterator* iterator =
      MakeGarbageCollected<ForwardSameBlockWordIterator>(
          PositionInFlatTree(*node, 5));
  iterator->AdvanceNextWord();
  EXPECT_EQ("paragraph", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("paragraph text", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("paragraph text", iterator->TextFromStart());
}

// Basic case for backward iterator->
TEST_F(SameBlockWordIteratorTest, GetPreviousWord) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p>new block</p>
    <p id='first'>First paragraph next word</p>
  )HTML");
  Node* node =
      GetDocument().getElementById(AtomicString("first"))->firstChild();

  BackwardSameBlockWordIterator* iterator =
      MakeGarbageCollected<BackwardSameBlockWordIterator>(
          PositionInFlatTree(*node, 16));
  iterator->AdvanceNextWord();
  EXPECT_EQ("paragraph", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("First paragraph", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("First paragraph", iterator->TextFromStart());
}

// Check the case when available text has extra space.
TEST_F(SameBlockWordIteratorTest, GetPreviousWord_ExtraSpace) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p>new block</p>
    <p id='first'>First

         paragraph text</p>
  )HTML");
  Node* node =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  BackwardSameBlockWordIterator* iterator =
      MakeGarbageCollected<BackwardSameBlockWordIterator>(
          PositionInFlatTree(*node, 25));
  iterator->AdvanceNextWord();
  EXPECT_EQ("paragraph", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("First paragraph", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("First paragraph", iterator->TextFromStart());
}

// Check the case when there is a commented block which should be skipped.
TEST_F(SameBlockWordIteratorTest, GetPreviousWord_WithComment) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p>new block</p>
    <div id='first'>
      <span>First</span>
      <!--
        multiline comment that should be ignored.
      //-->
      <span id='span'>paragraph text</span>
    </div>
    <p>new block</p>
  )HTML");
  Node* node = GetDocument().getElementById(AtomicString("span"))->firstChild();
  BackwardSameBlockWordIterator* iterator =
      MakeGarbageCollected<BackwardSameBlockWordIterator>(
          PositionInFlatTree(*node, 9));
  iterator->AdvanceNextWord();
  iterator->TextFromStart();

  iterator->AdvanceNextWord();
  EXPECT_EQ("First paragraph", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("First paragraph", iterator->TextFromStart());
}

// Check the case when available text contains non-block tag(e.g. <b>).
TEST_F(SameBlockWordIteratorTest, GetPreviousWord_NestedTextNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First <b>bold text</b> paragraph text</p>
  )HTML");
  Node* node = GetDocument().getElementById(AtomicString("first"))->lastChild();
  BackwardSameBlockWordIterator* iterator =
      MakeGarbageCollected<BackwardSameBlockWordIterator>(
          PositionInFlatTree(*node, 11));
  iterator->AdvanceNextWord();
  EXPECT_EQ("paragraph", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("text paragraph", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("bold text paragraph", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("First bold text paragraph", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("First bold text paragraph", iterator->TextFromStart());
}

// Check the case when available text is interrupted by a nested block.
TEST_F(SameBlockWordIteratorTest, GetPreviousWord_NestedBlock) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First <div id='div'>div</div> paragraph text</div>
  )HTML");
  Node* node = GetDocument().getElementById(AtomicString("div"))->nextSibling();
  BackwardSameBlockWordIterator* iterator =
      MakeGarbageCollected<BackwardSameBlockWordIterator>(
          PositionInFlatTree(*node, 11));
  iterator->AdvanceNextWord();
  EXPECT_EQ("paragraph", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("paragraph", iterator->TextFromStart());
}

// Check the case when available text includes non-block element but is
// interrupted by a nested block.
TEST_F(SameBlockWordIteratorTest, GetPreviousWord_NestedBlockInNestedText) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First <b><div id='div'>div</div>bold</b> paragraph text</div>
  )HTML");
  Node* node = GetDocument().getElementById(AtomicString("first"))->lastChild();
  BackwardSameBlockWordIterator* iterator =
      MakeGarbageCollected<BackwardSameBlockWordIterator>(
          PositionInFlatTree(*node, 11));
  iterator->AdvanceNextWord();
  EXPECT_EQ("paragraph", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("bold paragraph", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("bold paragraph", iterator->TextFromStart());
}

// Check the case when available text includes invisible block.
TEST_F(SameBlockWordIteratorTest, GetPreviousWord_NestedInvisibleBlock) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First <div id='div' style='display:none'>invisible</div> paragraph text</div>
  )HTML");
  Node* node = GetDocument().getElementById(AtomicString("div"))->nextSibling();
  BackwardSameBlockWordIterator* iterator =
      MakeGarbageCollected<BackwardSameBlockWordIterator>(
          PositionInFlatTree(*node, 0));
  iterator->AdvanceNextWord();
  EXPECT_EQ("First", iterator->TextFromStart());

  iterator->AdvanceNextWord();
  EXPECT_EQ("First", iterator->TextFromStart());
}

// Check the case when given start position is in a middle of a word.
TEST_F(SameBlockWordIteratorTest, GetNextWord_HalfWord) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
    <p>new block</p>
  )HTML");
  Node* node =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  ForwardSameBlockWordIterator* iterator =
      MakeGarbageCollected<ForwardSameBlockWordIterator>(
          PositionInFlatTree(*node, 2));
  iterator->AdvanceNextWord();
  EXPECT_EQ("rst", iterator->TextFromStart());
}

}  // namespace blink
