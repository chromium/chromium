// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/text_fragment_finder.h"

#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

using testing::_;
using testing::Mock;

namespace blink {

class MockTextFragmentFinder : public TextFragmentFinder {
 public:
  MockTextFragmentFinder(Client& client,
                         const TextFragmentSelector& selector,
                         Document* document,
                         FindBufferRunnerType runner_type,
                         bool manual_step_through = false)
      : TextFragmentFinder(client, selector, document, runner_type) {
    manual_step_through_ = manual_step_through;
  }

  MockTextFragmentFinder(Client& client,
                         const TextFragmentSelector& selector,
                         Range* range,
                         FindBufferRunnerType runner_type,
                         bool manual_step_through = false)
      : TextFragmentFinder(client, selector, range, runner_type) {
    manual_step_through_ = manual_step_through;
  }

 private:
  bool manual_step_through_;
  void GoToStep(SelectorMatchStep step) override {
    step_ = step;
    if (!manual_step_through_) {
      TextFragmentFinder::GoToStep(step);
    }
  }
};

class MockTextFragmentFinderClient : public TextFragmentFinder::Client {
 public:
  MOCK_METHOD(void,
              DidFindMatch,
              (const RangeInFlatTree& match, bool is_unique),
              (override));
  MOCK_METHOD(void, NoMatchFound, (), (override));
};

class TextFragmentFinderTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  }
};

MATCHER_P(RangeContainedBy, element, "") {
  return FlatTreeTraversal::Contains(
             *element, *arg.StartPosition().ComputeContainerNode()) &&
         FlatTreeTraversal::Contains(*element,
                                     *arg.EndPosition().ComputeContainerNode());
}

// Tests that Find tasks will fail gracefully when DOM mutations invalidate the
// Find task properties.
TEST_F(TextFragmentFinderTest, DOMMutation) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <input id="input" type='submit' value="button text">
    <p id='first'>First paragraph prefix to unique snippet of text.</p>
  )HTML");

  TextFragmentSelector selector(TextFragmentSelector::SelectorType::kExact,
                                "First paragraph", "", "button text",
                                "prefix to unique");

  MockTextFragmentFinderClient client;

  MockTextFragmentFinder* finder = MakeGarbageCollected<MockTextFragmentFinder>(
      client, selector, &GetDocument(),
      TextFragmentFinder::FindBufferRunnerType::kSynchronous,
      /*manual_step_through=*/true);
  EXPECT_CALL(client, DidFindMatch(_, _)).Times(0);

  {
    EXPECT_CALL(client, NoMatchFound()).Times(0);
    finder->FindMatch();
    finder->FindPrefix();
    Mock::VerifyAndClearExpectations(&client);
  }

  {
    EXPECT_CALL(client, NoMatchFound()).Times(0);
    finder->FindTextStart();
    Mock::VerifyAndClearExpectations(&client);
  }

  {
    EXPECT_CALL(client, NoMatchFound()).Times(1);
    Node* input = GetDocument().getElementById(AtomicString("input"));
    input->remove();
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

    finder->FindSuffix();
    Mock::VerifyAndClearExpectations(&client);
  }
}

// Tests that a text match is found in the document.
TEST_F(TextFragmentFinderTest, TextMatchInDocument) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph start text to unique snippet of text.</p>
  )HTML");

  TextFragmentSelector selector(TextFragmentSelector::SelectorType::kExact,
                                "First paragraph", "", "", "");

  MockTextFragmentFinderClient client;

  MockTextFragmentFinder* finder = MakeGarbageCollected<MockTextFragmentFinder>(
      client, selector, &GetDocument(),
      TextFragmentFinder::FindBufferRunnerType::kSynchronous);

  EXPECT_CALL(client, NoMatchFound()).Times(0);
  EXPECT_CALL(client, DidFindMatch(_, true)).Times(1);
  finder->FindMatch();
  Mock::VerifyAndClearExpectations(&client);
}

// Tests that a text match is found in the given range.
TEST_F(TextFragmentFinderTest, TextMatchInRange) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>
      <p id='p1'>First paragraph start text to unique snippet of text.</p>
      <p id='p2'>Second paragraph start text to unique snippet of text.</p>
      <p id='p3'>Third paragraph start text to unique snippet of text.</p>
    </div>
  )HTML");

  Element* const p1 = GetDocument().getElementById(AtomicString("p1"));
  Element* const p3 = GetDocument().getElementById(AtomicString("p3"));

  auto* new_range = MakeGarbageCollected<Range>(GetDocument(), Position(p1, 0),
                                                Position(p3, 0));
  EXPECT_EQ(
      "First paragraph start text to unique snippet of text.\n\n"
      "Second paragraph start text to unique snippet of text.\n\n",
      new_range->GetText());

  TextFragmentSelector selector(TextFragmentSelector::SelectorType::kExact,
                                "First paragraph start text",
                                "Second paragraph", "", "");
  MockTextFragmentFinderClient client;

  MockTextFragmentFinder* finder = MakeGarbageCollected<MockTextFragmentFinder>(
      client, selector, new_range,
      TextFragmentFinder::FindBufferRunnerType::kSynchronous);

  EXPECT_CALL(client, NoMatchFound()).Times(0);
  EXPECT_CALL(client, DidFindMatch(_, true)).Times(1);
  finder->FindMatch();
  Mock::VerifyAndClearExpectations(&client);
}

// Tests that a selector whose start term is in the search range but end term
// is not does not match.
TEST_F(TextFragmentFinderTest, TextEndMatchNotInRange) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>
      <p id='p1'>First paragraph start text to unique snippet of text.</p>
      <p id='p2'>Second paragraph start text to unique snippet of text.</p>
      <p id='p3'>Third paragraph start text to unique snippet of text.</p>
    </div>
  )HTML");

  Element* const p2 = GetDocument().getElementById(AtomicString("p2"));
  Element* const p3 = GetDocument().getElementById(AtomicString("p3"));

  auto* new_range = MakeGarbageCollected<Range>(GetDocument(), Position(p2, 0),
                                                Position(p3, 0));
  EXPECT_EQ("Second paragraph start text to unique snippet of text.\n\n",
            new_range->GetText());

  TextFragmentSelector selector(TextFragmentSelector::SelectorType::kExact,
                                "Second paragraph", "Third paragraph", "", "");
  MockTextFragmentFinderClient client;

  MockTextFragmentFinder* finder = MakeGarbageCollected<MockTextFragmentFinder>(
      client, selector, new_range,
      TextFragmentFinder::FindBufferRunnerType::kSynchronous);

  EXPECT_CALL(client, NoMatchFound()).Times(1);
  EXPECT_CALL(client, DidFindMatch(_, _)).Times(0);
  finder->FindMatch();
  Mock::VerifyAndClearExpectations(&client);
}

// Tests that a text match is not found in the given range even though it is in
// the document, but it is before the range.
TEST_F(TextFragmentFinderTest, TextMatchNotFoundBeforeRange) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>
      <p id='p1'>First paragraph start text to unique snippet of text.</p>
      <p id='p2'>Second paragraph start text to unique snippet of text.</p>
      <p id='p3'>Third paragraph start text to unique snippet of text.</p>
    </div>
  )HTML");

  Element* const p2 = GetDocument().getElementById(AtomicString("p2"));
  Element* const p3 = GetDocument().getElementById(AtomicString("p3"));

  auto* range = MakeGarbageCollected<Range>(GetDocument(), Position(p2, 0),
                                            Position(p3, 0));
  EXPECT_EQ("Second paragraph start text to unique snippet of text.\n\n",
            range->GetText());

  TextFragmentSelector selector(TextFragmentSelector::SelectorType::kExact,
                                "First paragraph start text", "", "", "");
  MockTextFragmentFinderClient client;

  MockTextFragmentFinder* finder = MakeGarbageCollected<MockTextFragmentFinder>(
      client, selector, range,
      TextFragmentFinder::FindBufferRunnerType::kSynchronous);

  EXPECT_CALL(client, NoMatchFound()).Times(1);
  EXPECT_CALL(client, DidFindMatch(_, _)).Times(0);
  finder->FindMatch();
  Mock::VerifyAndClearExpectations(&client);
}

// Tests that a text match is not found in the given range even though it is in
// the document, but it is after the range.
TEST_F(TextFragmentFinderTest, TextMatchNotFoundAfterRange) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>
      <p id='p1'>First paragraph start text to unique snippet of text.</p>
      <p id='p2'>Second paragraph start text to unique snippet of text.</p>
      <p id='p3'>Third paragraph start text to unique snippet of text.</p>
    </div>
  )HTML");

  Element* const p2 = GetDocument().getElementById(AtomicString("p2"));
  Element* const p3 = GetDocument().getElementById(AtomicString("p3"));

  auto* range = MakeGarbageCollected<Range>(GetDocument(), Position(p2, 0),
                                            Position(p3, 0));
  EXPECT_EQ("Second paragraph start text to unique snippet of text.\n\n",
            range->GetText());

  TextFragmentSelector selector(TextFragmentSelector::SelectorType::kExact,
                                "Third paragraph start text", "", "", "");
  MockTextFragmentFinderClient client;

  MockTextFragmentFinder* finder = MakeGarbageCollected<MockTextFragmentFinder>(
      client, selector, range,
      TextFragmentFinder::FindBufferRunnerType::kSynchronous);

  EXPECT_CALL(client, NoMatchFound()).Times(1);
  EXPECT_CALL(client, DidFindMatch(_, _)).Times(0);
  finder->FindMatch();
  Mock::VerifyAndClearExpectations(&client);
}

// Tests that when there are multiple text matches in the page, and the first
// match is returned in the given range.
TEST_F(TextFragmentFinderTest, TextDidFindMatchInRange) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>
      <p id='p1'>Paragraph start text 1 to unique snippet of text.</p>
      <p id='p2'>Paragraph start text 2 to unique snippet of text.</p>
      <p id='p3'>Paragraph start text 3 to unique snippet of text.</p>
    </div>
  )HTML");

  Element* const p1 = GetDocument().getElementById(AtomicString("p1"));
  Element* const p3 = GetDocument().getElementById(AtomicString("p3"));

  auto* range = MakeGarbageCollected<Range>(GetDocument(), Position(p1, 0),
                                            Position(p3, 0));
  EXPECT_EQ(
      "Paragraph start text 1 to unique snippet of text.\n\n"
      "Paragraph start text 2 to unique snippet of text.\n\n",
      range->GetText());

  TextFragmentSelector selector(TextFragmentSelector::SelectorType::kExact,
                                "Paragraph start text", "", "", "");
  MockTextFragmentFinderClient client;

  MockTextFragmentFinder* finder = MakeGarbageCollected<MockTextFragmentFinder>(
      client, selector, range,
      TextFragmentFinder::FindBufferRunnerType::kSynchronous);

  EXPECT_CALL(client, NoMatchFound()).Times(0);
  bool is_unique_expected = false;
  EXPECT_CALL(client, DidFindMatch(RangeContainedBy(p1), is_unique_expected))
      .Times(1);
  finder->FindMatch();
  Mock::VerifyAndClearExpectations(&client);
}

// Tests that a text match is found within a defined suffix in the given range.
TEST_F(TextFragmentFinderTest, TextMatchWithSuffixInRange) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>
      <p id='p1'>First paragraph start text to unique snippet of text.</p>
      <p id='p2'>Second paragraph start text to unique snippet of text.</p>
      <p id='p3'>Third paragraph start text to unique snippet of text.</p>
    </div>
  )HTML");

  Element* const p1 = GetDocument().getElementById(AtomicString("p1"));
  Element* const p2 = GetDocument().getElementById(AtomicString("p2"));

  auto* new_range = MakeGarbageCollected<Range>(GetDocument(), Position(p1, 0),
                                                Position(p2, 0));
  EXPECT_EQ("First paragraph start text to unique snippet of text.\n\n",
            new_range->GetText());

  TextFragmentSelector selector(TextFragmentSelector::SelectorType::kExact,
                                "First paragraph", "", "", "start text");
  MockTextFragmentFinderClient client;

  MockTextFragmentFinder* finder = MakeGarbageCollected<MockTextFragmentFinder>(
      client, selector, new_range,
      TextFragmentFinder::FindBufferRunnerType::kSynchronous);

  EXPECT_CALL(client, NoMatchFound()).Times(0);
  EXPECT_CALL(client, DidFindMatch(_, true)).Times(1);
  finder->FindMatch();
  Mock::VerifyAndClearExpectations(&client);
}

// Tests that a text match is not found when the defined prefix is not in the
// given range.
TEST_F(TextFragmentFinderTest, TextMatchWithPrefixNotInRange) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>
      <p id='p1'>First paragraph start text to unique snippet of text.</p>
      <p id='p2'>Second paragraph start text to unique snippet of text.</p>
      <p id='p3'>Third paragraph start text to unique snippet of text.</p>
    </div>
  )HTML");

  Element* const p2 = GetDocument().getElementById(AtomicString("p2"));
  Element* const p3 = GetDocument().getElementById(AtomicString("p3"));

  auto* new_range = MakeGarbageCollected<Range>(GetDocument(), Position(p2, 0),
                                                Position(p3, 0));
  EXPECT_EQ("Second paragraph start text to unique snippet of text.\n\n",
            new_range->GetText());

  TextFragmentSelector selector(TextFragmentSelector::SelectorType::kExact,
                                "Second paragraph", "", "snippet of text",
                                "start text");
  MockTextFragmentFinderClient client;

  MockTextFragmentFinder* finder = MakeGarbageCollected<MockTextFragmentFinder>(
      client, selector, new_range,
      TextFragmentFinder::FindBufferRunnerType::kSynchronous);

  EXPECT_CALL(client, NoMatchFound()).Times(1);
  EXPECT_CALL(client, DidFindMatch(_, _)).Times(0);
  finder->FindMatch();
  Mock::VerifyAndClearExpectations(&client);
}

// Tests that a text match is not found before the defined surfix which is not
// in the given range.
TEST_F(TextFragmentFinderTest, TextMatchWithSuffixNotInRange) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>
      <p id='p1'>First paragraph start text to unique snippet of text.</p>
      <p id='p2'>Second paragraph start text to unique snippet of text.</p>
      <p id='p3'>Third paragraph start text to unique snippet of text.</p>
    </div>
  )HTML");

  Element* const p1 = GetDocument().getElementById(AtomicString("p1"));
  Element* const p2 = GetDocument().getElementById(AtomicString("p2"));

  auto* new_range = MakeGarbageCollected<Range>(GetDocument(), Position(p1, 0),
                                                Position(p2, 0));
  EXPECT_EQ("First paragraph start text to unique snippet of text.\n\n",
            new_range->GetText());

  TextFragmentSelector selector(TextFragmentSelector::SelectorType::kExact,
                                "snippet of text", "", "", "Second paragraph");
  MockTextFragmentFinderClient client;

  MockTextFragmentFinder* finder = MakeGarbageCollected<MockTextFragmentFinder>(
      client, selector, new_range,
      TextFragmentFinder::FindBufferRunnerType::kSynchronous);

  EXPECT_CALL(client, NoMatchFound()).Times(1);
  EXPECT_CALL(client, DidFindMatch(_, _)).Times(0);
  finder->FindMatch();
  Mock::VerifyAndClearExpectations(&client);
}

// Test that if the range contains partial text of a node, the text should match
// in the range not the full text of the node.
TEST_F(TextFragmentFinderTest, TextMatchPartialNodeNotInRange) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>
      <span id='p1'>First paragraph start text to unique snippet of text 1.</span>
      <span id='p2'>Second paragraph start text to unique snippet of text 2.</span>
      <span id='p3'>Third paragraph start text to unique snippet of text 3.</span>
    </div>
  )HTML");

  Element* const p1 = GetDocument().getElementById(AtomicString("p1"));
  Element* const p2 = GetDocument().getElementById(AtomicString("p2"));

  auto* new_range = MakeGarbageCollected<Range>(
      GetDocument(), Position(*p1->firstChild(), 6), Position(p2, 0));
  EXPECT_EQ("paragraph start text to unique snippet of text 1. ",
            new_range->GetText());

  TextFragmentSelector selector(TextFragmentSelector::SelectorType::kExact,
                                "First paragraph", "", "", "");
  MockTextFragmentFinderClient client;

  MockTextFragmentFinder* finder = MakeGarbageCollected<MockTextFragmentFinder>(
      client, selector, new_range,
      TextFragmentFinder::FindBufferRunnerType::kSynchronous);

  EXPECT_CALL(client, NoMatchFound()).Times(1);
  EXPECT_CALL(client, DidFindMatch(_, _)).Times(0);
  finder->FindMatch();
  Mock::VerifyAndClearExpectations(&client);
}

}  // namespace blink
