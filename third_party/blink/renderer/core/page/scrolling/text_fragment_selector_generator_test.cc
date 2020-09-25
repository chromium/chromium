// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector_generator.h"

#include <gtest/gtest.h>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/link_to_text/link_to_text.mojom-blink.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class TextFragmentSelectorGeneratorTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  }

  void GenerateAndVerifySelector(Position selected_start,
                                 Position selected_end,
                                 String expected_selector) {
    GetDocument()
        .GetFrame()
        ->GetTextFragmentSelectorGenerator()
        ->UpdateSelection(GetDocument().GetFrame(),
                          ToEphemeralRangeInFlatTree(
                              EphemeralRange(selected_start, selected_end)));

    bool callback_called = false;
    auto lambda = [](bool& callback_called, const String& expected_selector,
                     const String& selector) {
      EXPECT_EQ(selector, expected_selector);
      callback_called = true;
    };
    auto callback =
        WTF::Bind(lambda, std::ref(callback_called), expected_selector);
    GetDocument()
        .GetFrame()
        ->GetTextFragmentSelectorGenerator()
        ->GenerateSelector(std::move(callback));
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(callback_called);
  }
};

// Basic exact selector case.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextSelector) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer than 20 chars</p>
    <p id='second'>Second paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(first_paragraph, 28);
  ASSERT_EQ("First paragraph text that is",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end,
                            "First%20paragraph%20text%20that%20is");
}

// Exact selector test where selection contains nested <i> node.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextWithNestedTextNodes) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is <i>longer than 20</i> chars</p>
    <p id='second'>Second paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first");
  const auto& selected_start = Position(first_paragraph->firstChild(), 0);
  const auto& selected_end =
      Position(first_paragraph->firstChild()->nextSibling()->firstChild(), 6);
  ASSERT_EQ("First paragraph text that is longer",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end,
                            "First%20paragraph%20text%20that%20is%20longer");
}

// Exact selector test where selection contains multiple spaces.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextWithExtraSpace) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer than 20 chars</p>
    <p id='second'>Second paragraph
      text</p>
  )HTML");
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& selected_start = Position(second_paragraph, 0);
  const auto& selected_end = Position(second_paragraph, 27);
  ASSERT_EQ("Second paragraph text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end,
                            "Second%20paragraph%20text");
}

// Exact selector where selection is too short, in which case context is
// required.
TEST_F(TextFragmentSelectorGeneratorTest,
       ExactTextSelector_TooShortNeedsContext) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph prefix to unique snippet of text.</p>
    <p id='second'>Second paragraph</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 26);
  const auto& selected_end = Position(first_paragraph, 40);
  ASSERT_EQ("unique snippet",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end,
                            "to-,unique%20snippet,-of");
}

// Exact selector with context test. Case when only one word for prefix and
// suffix is enough to disambiguate the selection.
TEST_F(TextFragmentSelectorGeneratorTest,
       ExactTextSelector_WithOneWordContext) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer than 20 chars</p>
    <p id='second'>Second paragraph text that is short</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 6);
  const auto& selected_end = Position(first_paragraph, 28);
  ASSERT_EQ("paragraph text that is",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end,
                            "First-,paragraph%20text%20that%20is,-longer");
}

// Exact selector with context test. Case when multiple words for prefix and
// suffix is necessary to disambiguate the selection.
TEST_F(TextFragmentSelectorGeneratorTest,
       ExactTextSelector_MultipleWordContext) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First prefix to not unique snippet of text followed by suffix</p>
    <p id='second'>Second prefix to not unique snippet of text followed by suffix</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 16);
  const auto& selected_end = Position(first_paragraph, 42);
  ASSERT_EQ("not unique snippet of text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end,
                            "First%20prefix%20to-,not%20unique%20snippet%20of%"
                            "20text,-followed%20by%20suffix");
}

// Exact selector with context test. Case when multiple words for prefix and
// suffix is necessary to disambiguate the selection and prefix and suffix
// contain extra space.
TEST_F(TextFragmentSelectorGeneratorTest,
       ExactTextSelector_MultipleWordContext_ExtraSpace) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First prefix      to not unique snippet of text followed       by suffix</p>
    <p id='second'>Second prefix to not unique snippet of text followed by suffix</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 21);
  const auto& selected_end = Position(first_paragraph, 47);
  ASSERT_EQ("not unique snippet of text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end,
                            "First%20prefix%20to-,not%20unique%20snippet%20of%"
                            "20text,-followed%20by%20suffix");
}

// Exact selector with context test. Case when available prefix for all the
// occurrences of selected text is the same. In this case suffix should be
// extended until unique selector is found.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextSelector_SamePrefix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>Prefix to not unique snippet of text followed by different suffix</p>
    <p id='second'>Prefix to not unique snippet of text followed by suffix</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 10);
  const auto& selected_end = Position(first_paragraph, 36);
  ASSERT_EQ("not unique snippet of text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end,
                            "Prefix%20to-,not%20unique%20snippet%20of%20text,-"
                            "followed%20by%20different");
}

// Exact selector with context test. Case when available suffix for all the
// occurrences of selected text is the same. In this case prefix should be
// extended until unique selector is found.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextSelector_SameSuffix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph prefix to not unique snippet of text followed by suffix</p>
    <p id='second'>Second paragraph prefix to not unique snippet of text followed by suffix</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 26);
  const auto& selected_end = Position(first_paragraph, 52);
  ASSERT_EQ("not unique snippet of text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end,
                            "First%20paragraph%20prefix%20to-,not%20unique%"
                            "20snippet%20of%20text,-followed%20by%20suffix");
}

// Exact selector with context test. Case when available prefix and suffix for
// all the occurrences of selected text are the same. In this case generation
// should be unsuccessful.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextSelector_SamePreffixSuffix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>Same paragraph prefix to not unique snippet of text followed by suffix</p>
    <p id='second'>Same paragraph prefix to not unique snippet of text followed by suffix</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 25);
  const auto& selected_end = Position(first_paragraph, 51);
  ASSERT_EQ("not unique snippet of text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end, "");
}

// Exact selector with context test. Case when available prefix and suffix for
// all the occurrences of selected text are the same for the first 10 words. In
// this case generation should be unsuccessful.
TEST_F(TextFragmentSelectorGeneratorTest,
       ExactTextSelector_SimilarLongPreffixSuffix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph prefix one two three four five six seven
     eight nine ten to not unique snippet of text followed by suffix</p>
    <p id='second'>Second paragraph prefix one two three four five six seven
     eight nine ten to not unique snippet of text followed by suffix</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 80);
  const auto& selected_end = Position(first_paragraph, 106);
  ASSERT_EQ("not unique snippet of text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end, "");
}

// Exact selector with context test. Case when no prefix is available.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextSelector_NoPrefix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>Not unique snippet of text followed by first suffix</p>
    <p id='second'>Not unique snippet of text followed by second suffix</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(first_paragraph, 26);
  ASSERT_EQ("Not unique snippet of text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(
      selected_start, selected_end,
      "Not%20unique%20snippet%20of%20text,-followed%20by%20first");
}

// Exact selector with context test. Case when no suffix is available.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextSelector_NoSuffix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First prefix to not unique snippet of text</p>
    <p id='second'>Second prefix to not unique snippet of text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& selected_start = Position(first_paragraph, 17);
  const auto& selected_end = Position(first_paragraph, 43);
  ASSERT_EQ("not unique snippet of text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end,
                            "Second%20prefix%20to-,not%20unique%20snippet%20of%"
                            "20text");
}

// Exact selector with context test. Case when available prefix is the
// preceding block.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextSelector_PrevNodePrefix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph with not unique snippet</p>
    <p id='second'>not unique snippet of text</p>
  )HTML");
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& selected_start = Position(second_paragraph, 0);
  const auto& selected_end = Position(second_paragraph, 18);
  ASSERT_EQ("not unique snippet",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end,
                            "snippet-,not%20unique%20snippet,-of");
}

// Exact selector with context test. Case when available prefix is the
// preceding block, which is a text node.
TEST_F(TextFragmentSelectorGeneratorTest,
       ExactTextSelector_PrevTextNodePrefix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph with not unique snippet</p>
    text
    <p id='second'>not unique snippet of text</p>
  )HTML");
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& selected_start = Position(second_paragraph, 0);
  const auto& selected_end = Position(second_paragraph, 18);
  ASSERT_EQ("not unique snippet",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end,
                            "text-,not%20unique%20snippet,-of");
}

// Exact selector with context test. Case when available suffix is the next
// block.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextSelector_NextNodeSuffix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph with not unique snippet</p>
    <p id='second'>not unique snippet of text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 21);
  const auto& selected_end = Position(first_paragraph, 39);
  ASSERT_EQ("not unique snippet",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end,
                            "with-,not%20unique%20snippet,-not");
}

// Exact selector with context test. Case when available suffix is the next
// block, which is a text node.
TEST_F(TextFragmentSelectorGeneratorTest,
       ExactTextSelector_NexttextNodeSuffix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph with not unique snippet</p>
    text
    <p id='second'>not unique snippet of text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 21);
  const auto& selected_end = Position(first_paragraph, 39);
  ASSERT_EQ("not unique snippet",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end,
                            "with-,not%20unique%20snippet,-text");
}

TEST_F(TextFragmentSelectorGeneratorTest, RangeSelector) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer than 20 chars</p>
    <p id='second'>Second paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(second_paragraph, 6);
  ASSERT_EQ("First paragraph text that is longer than 20 chars\n\nSecond",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end, "First,Second");
}

// It should be more than 300 characters selected from the same node so that
// ranges are used.
TEST_F(TextFragmentSelectorGeneratorTest, RangeSelector_SameNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text text text text text text text
    text text text text text text text text text text text text text
    text text text text text text text text text text text text text
    text text text text text text text text text text text text text
    text text text text text text text text text and last text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(first_paragraph, 320);
  ASSERT_EQ(
      "First paragraph text text text text text text text \
text text text text text text text text text text text text text \
text text text text text text text text text text text text text \
text text text text text text text text text text text text text \
text text text text text text text text text and last text",
      PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end,
                            "First%20paragraph,last%20text");
}

// When using all the selected text for the range is not enough for unique
// match, context should be added.
TEST_F(TextFragmentSelectorGeneratorTest, RangeSelector_RangeNotUnique) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph</p><p id='text1'>text</p>
    <p id='second'>Second paragraph</p><p id='text2'>text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  Node* first_text = GetDocument().getElementById("text1")->firstChild();
  const auto& selected_start = Position(first_paragraph, 6);
  const auto& selected_end = Position(first_text, 4);
  ASSERT_EQ("paragraph\n\ntext",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end,
                            "First-,paragraph,text,-Second");
}

// When using all the selected text for the range is not enough for unique
// match, context should be added, but only prefxi and no suffix is available.
TEST_F(TextFragmentSelectorGeneratorTest,
       RangeSelector_RangeNotUnique_NoSuffix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph</p><p id='text1'>text</p>
    <p id='second'>Second paragraph</p><p id='text2'>text</p>
  )HTML");
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  Node* second_text = GetDocument().getElementById("text2")->firstChild();
  const auto& selected_start = Position(second_paragraph, 7);
  const auto& selected_end = Position(second_text, 4);
  ASSERT_EQ("paragraph\n\ntext",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end,
                            "Second-,paragraph,text");
}

// When no range end is available it should return empty selector.
// There is no range end available because there is no word break in the second
// half of the selection.
TEST_F(TextFragmentSelectorGeneratorTest, RangeSelector_NoRangeEnd) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text text text text text text text
    text text text text text text text text text text text text text
    text text text text text text text text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_and_last_text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(first_paragraph, 312);
  ASSERT_EQ(
      "First paragraph text text text text text text text \
text text text text text text text text text text text text text \
text text text text text text text text_text_text_text_text_text_\
text_text_text_text_text_text_text_text_text_text_text_text_text_\
text_text_text_text_text_text_text_text_text_and_last_text",
      PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end, "");
}

// Selection should be autocompleted to contain full words.
TEST_F(TextFragmentSelectorGeneratorTest, WordLimit) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer than 20 chars</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 7);
  const auto& selected_end = Position(first_paragraph, 33);
  ASSERT_EQ("aragraph text that is long",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end,
                            "paragraph%20text%20that%20is%20longer");
}

// Selection should be autocompleted to contain full words. The autocompletion
// should work with extra spaces.
TEST_F(TextFragmentSelectorGeneratorTest, WordLimit_ExtraSpaces) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First
    paragraph text
    that is longer than 20 chars</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 11);
  const auto& selected_end = Position(first_paragraph, 41);
  ASSERT_EQ("aragraph text that is long",
            PlainText(EphemeralRange(selected_start, selected_end)));

  GenerateAndVerifySelector(selected_start, selected_end,
                            "paragraph%20text%20that%20is%20longer");
}

// Basic test case for |GetNextTextBlock|.
TEST_F(TextFragmentSelectorGeneratorTest, GetPreviousTextBlock) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 16);
  const auto& end = Position(first_paragraph, 20);
  ASSERT_EQ("text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("First paragraph", GetDocument()
                                   .GetFrame()
                                   ->GetTextFragmentSelectorGenerator()
                                   ->GetPreviousTextBlockForTesting(start));
}

// Check the case when available prefix contains collapsible space.
TEST_F(TextFragmentSelectorGeneratorTest, GetPreviousTextBlock_ExtraSpace) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First

         paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 26);
  const auto& end = Position(first_paragraph, 30);
  ASSERT_EQ("text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("First paragraph", GetDocument()
                                   .GetFrame()
                                   ->GetTextFragmentSelectorGenerator()
                                   ->GetPreviousTextBlockForTesting(start));
}

// Check the case when available prefix complete text content of the previous
// block.
TEST_F(TextFragmentSelectorGeneratorTest, GetPreviousTextBlock_PrevNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
    <p id='second'>Second paragraph text</p>
  )HTML");
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& start = Position(second_paragraph, 0);
  const auto& end = Position(second_paragraph, 6);
  ASSERT_EQ("Second", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("First paragraph text",
            GetDocument()
                .GetFrame()
                ->GetTextFragmentSelectorGenerator()
                ->GetPreviousTextBlockForTesting(start));
}

// Check the case when there is a commented block between selection and the
// available prefix.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetPreviousTextBlock_PrevNode_WithComment) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
    <!--
      multiline comment that should be ignored.
    //-->
    <p id='second'>Second paragraph text</p>
  )HTML");
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& start = Position(second_paragraph, 0);
  const auto& end = Position(second_paragraph, 6);
  ASSERT_EQ("Second", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("First paragraph text",
            GetDocument()
                .GetFrame()
                ->GetTextFragmentSelectorGenerator()
                ->GetPreviousTextBlockForTesting(start));
}

// Check the case when available prefix is a text node outside of selection
// block.
TEST_F(TextFragmentSelectorGeneratorTest, GetPreviousTextBlock_PrevTextNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    text
    <p id='first'>First paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("text", GetDocument()
                        .GetFrame()
                        ->GetTextFragmentSelectorGenerator()
                        ->GetPreviousTextBlockForTesting(start));
}

// Check the case when available prefix is a parent node text content outside of
// selection block.
TEST_F(TextFragmentSelectorGeneratorTest, GetPreviousTextBlock_ParentNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>nested
    <p id='first'>First paragraph text</p></div>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("nested", GetDocument()
                          .GetFrame()
                          ->GetTextFragmentSelectorGenerator()
                          ->GetPreviousTextBlockForTesting(start));
}

// Check the case when available prefix contains non-block tag(e.g. <b>).
TEST_F(TextFragmentSelectorGeneratorTest, GetPreviousTextBlock_NestedTextNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First <b>bold text</b> paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->lastChild();
  const auto& start = Position(first_paragraph, 11);
  const auto& end = Position(first_paragraph, 15);
  ASSERT_EQ("text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("First bold text paragraph",
            GetDocument()
                .GetFrame()
                ->GetTextFragmentSelectorGenerator()
                ->GetPreviousTextBlockForTesting(start));
}

// Check the case when available prefix is collected until nested block.
TEST_F(TextFragmentSelectorGeneratorTest, GetPreviousTextBlock_NestedBlock) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First <div id='div'>div</div> paragraph text</div>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("div")->nextSibling();
  const auto& start = Position(first_paragraph, 11);
  const auto& end = Position(first_paragraph, 15);
  ASSERT_EQ("text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("paragraph", GetDocument()
                             .GetFrame()
                             ->GetTextFragmentSelectorGenerator()
                             ->GetPreviousTextBlockForTesting(start));
}

// Check the case when available prefix includes non-block element but stops at
// nested block.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetPreviousTextBlock_NestedBlockInNestedText) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First <b><div id='div'>div</div>bold</b> paragraph text</div>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->lastChild();
  const auto& start = Position(first_paragraph, 11);
  const auto& end = Position(first_paragraph, 15);
  ASSERT_EQ("text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("bold paragraph", GetDocument()
                                  .GetFrame()
                                  ->GetTextFragmentSelectorGenerator()
                                  ->GetPreviousTextBlockForTesting(start));
}

// Check the case when available prefix includes invisible block.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetPreviousTextBlock_NestedInvisibleBlock) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First <div id='div' style='display:none'>invisible</div> paragraph text</div>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("div")->nextSibling();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 10);
  ASSERT_EQ("paragraph", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("First", GetDocument()
                         .GetFrame()
                         ->GetTextFragmentSelectorGenerator()
                         ->GetPreviousTextBlockForTesting(start));
}

// Check the case when previous node is used for available prefix when selection
// is not at index=0 but there is only space before it.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetPreviousTextBlock_SpacesBeforeSelection) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
    <p id='second'>
      Second paragraph text
    </p>
  )HTML");
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& start = Position(second_paragraph, 6);
  const auto& end = Position(second_paragraph, 13);
  ASSERT_EQ("Second", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("First paragraph text",
            GetDocument()
                .GetFrame()
                ->GetTextFragmentSelectorGenerator()
                ->GetPreviousTextBlockForTesting(start));
}

// Check the case when previous node is used for available prefix when selection
// is not at index=0 but there is only invisible block.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetPreviousTextBlock_InvisibleBeforeSelection) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
    <div id='second'>
      <p id='invisible' style='display:none'>
        invisible text
      </p>
      Second paragraph text
    </div>
  )HTML");
  Node* second_paragraph =
      GetDocument().getElementById("invisible")->nextSibling();
  const auto& start = Position(second_paragraph, 6);
  const auto& end = Position(second_paragraph, 13);
  ASSERT_EQ("Second", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("First paragraph text",
            GetDocument()
                .GetFrame()
                ->GetTextFragmentSelectorGenerator()
                ->GetPreviousTextBlockForTesting(start));
}

// Similar test for suffix.

// Basic test case for |GetNextTextBlock|.
TEST_F(TextFragmentSelectorGeneratorTest, GetNextTextBlock) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("paragraph text", GetDocument()
                                  .GetFrame()
                                  ->GetTextFragmentSelectorGenerator()
                                  ->GetNextTextBlockForTesting(end));
}

// Check the case when available suffix contains collapsible space.
TEST_F(TextFragmentSelectorGeneratorTest, GetNextTextBlock_ExtraSpace) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph


     text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("paragraph text", GetDocument()
                                  .GetFrame()
                                  ->GetTextFragmentSelectorGenerator()
                                  ->GetNextTextBlockForTesting(end));
}

// Check the case when available suffix is complete text content of the next
// block.
TEST_F(TextFragmentSelectorGeneratorTest, GetNextTextBlock_NextNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
    <p id='second'>Second paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 20);
  ASSERT_EQ("First paragraph text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("Second paragraph text", GetDocument()
                                         .GetFrame()
                                         ->GetTextFragmentSelectorGenerator()
                                         ->GetNextTextBlockForTesting(end));
}

// Check the case when there is a commented block between selection and the
// available suffix.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetNextTextBlock_NextNode_WithComment) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
    <!--
      multiline comment that should be ignored.
    //-->
    <p id='second'>Second paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 20);
  ASSERT_EQ("First paragraph text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("Second paragraph text", GetDocument()
                                         .GetFrame()
                                         ->GetTextFragmentSelectorGenerator()
                                         ->GetNextTextBlockForTesting(end));
}

// Check the case when available suffix is a text node outside of selection
// block.
TEST_F(TextFragmentSelectorGeneratorTest, GetNextTextBlock_NextTextNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
    text
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 20);
  ASSERT_EQ("First paragraph text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("text", GetDocument()
                        .GetFrame()
                        ->GetTextFragmentSelectorGenerator()
                        ->GetNextTextBlockForTesting(end));
}

// Check the case when available suffix is a parent node text content outside of
// selection block.
TEST_F(TextFragmentSelectorGeneratorTest, GetNextTextBlock_ParentNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div><p id='first'>First paragraph text</p> nested</div>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 20);
  ASSERT_EQ("First paragraph text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("nested", GetDocument()
                          .GetFrame()
                          ->GetTextFragmentSelectorGenerator()
                          ->GetNextTextBlockForTesting(end));
}

// Check the case when available suffix contains non-block tag(e.g. <b>).
TEST_F(TextFragmentSelectorGeneratorTest, GetNextTextBlock_NestedTextNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First <b>bold text</b> paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("bold text paragraph text", GetDocument()
                                            .GetFrame()
                                            ->GetTextFragmentSelectorGenerator()
                                            ->GetNextTextBlockForTesting(end));
}

// Check the case when available suffix is collected until nested block.
TEST_F(TextFragmentSelectorGeneratorTest, GetNextTextBlock_NestedBlock) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First paragraph <div id='div'>div</div> text</div>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("paragraph", GetDocument()
                             .GetFrame()
                             ->GetTextFragmentSelectorGenerator()
                             ->GetNextTextBlockForTesting(end));
}

// Check the case when available suffix includes non-block element but stops at
// nested block.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetNextTextBlock_NestedBlockInNestedText) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First <b>bold<div id='div'>div</div></b> paragraph text</div>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("bold", GetDocument()
                        .GetFrame()
                        ->GetTextFragmentSelectorGenerator()
                        ->GetNextTextBlockForTesting(end));
}

// Check the case when available suffix includes invisible block.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetNextTextBlock_NestedInvisibleBlock) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First <div id='div' style='display:none'>invisible</div> paragraph text</div>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("paragraph text", GetDocument()
                                  .GetFrame()
                                  ->GetTextFragmentSelectorGenerator()
                                  ->GetNextTextBlockForTesting(end));
}

// Check the case when next node is used for available suffix when selection is
// not at last index but there is only space after it.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetNextTextBlock_SpacesAfterSelection) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>
      First paragraph text
    </p>
    <p id='second'>
      Second paragraph text
    </p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 23);
  const auto& end = Position(first_paragraph, 27);
  ASSERT_EQ("text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("Second paragraph text", GetDocument()
                                         .GetFrame()
                                         ->GetTextFragmentSelectorGenerator()
                                         ->GetNextTextBlockForTesting(end));
}

// Check the case when next node is used for available suffix when selection is
// not at last index but there is only invisible block after it.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetNextTextBlock_InvisibleAfterSelection) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>
      First paragraph text
      <div id='invisible' style='display:none'>
        invisible text
      </div>
    </div>
    <p id='second'>
      Second paragraph text
    </p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 23);
  const auto& end = Position(first_paragraph, 27);
  ASSERT_EQ("text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("Second paragraph text", GetDocument()
                                         .GetFrame()
                                         ->GetTextFragmentSelectorGenerator()
                                         ->GetNextTextBlockForTesting(end));
}

// Check the case when previous node is used for available prefix when selection
// is not at last index but there is only invisible block. Invisible block
// contains another block which also should be invisible.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetNextTextBlock_InvisibleAfterSelection_WithNestedInvisible) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>
      First paragraph text
      <div id='invisible' style='display:none'>
        invisible text
        <div>
          nested invisible text
        </div
      </div>
    </div>
    <p id='second'>
      Second paragraph text
      <div id='invisible' style='display:none'>
        invisible text
        <div>
          nested invisible text
        </div
      </div>
    </p>
    test
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 23);
  const auto& end = Position(first_paragraph, 27);
  ASSERT_EQ("text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("Second paragraph text", GetDocument()
                                         .GetFrame()
                                         ->GetTextFragmentSelectorGenerator()
                                         ->GetNextTextBlockForTesting(end));
}

}  // namespace blink
