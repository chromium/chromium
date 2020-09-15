// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector_generator.h"

#include <gtest/gtest.h>

#include "base/test/bind_test_util.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class TextFragmentSelectorGeneratorTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  }
};

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
  bool callback_called = false;
  base::OnceCallback<void(const TextFragmentSelector&)> callback =
      base::BindLambdaForTesting([&](const TextFragmentSelector& selector) {
        EXPECT_EQ(selector.Type(), TextFragmentSelector::SelectorType::kExact);
        EXPECT_EQ(selector.Start(), "First paragraph text that is");
        callback_called = true;
      });

  TextFragmentSelectorGenerator generator;
  generator.UpdateSelection(
      GetDocument().GetFrame(),
      ToEphemeralRangeInFlatTree(EphemeralRange(selected_start, selected_end)));
  generator.SetCallbackForTesting(std::move(callback));
  generator.GenerateSelector();

  EXPECT_TRUE(callback_called);
}

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
  bool callback_called = false;
  base::OnceCallback<void(const TextFragmentSelector&)> callback =
      base::BindLambdaForTesting([&](const TextFragmentSelector& selector) {
        EXPECT_EQ(selector.Type(), TextFragmentSelector::SelectorType::kExact);
        EXPECT_EQ(selector.Start(), "First paragraph text that is longer");
        callback_called = true;
      });

  TextFragmentSelectorGenerator generator;
  generator.UpdateSelection(
      GetDocument().GetFrame(),
      ToEphemeralRangeInFlatTree(EphemeralRange(selected_start, selected_end)));
  generator.SetCallbackForTesting(std::move(callback));
  generator.GenerateSelector();

  EXPECT_TRUE(callback_called);
}

TEST_F(TextFragmentSelectorGeneratorTest, ExactTextWithExtraSpace) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer than 20 chars</p>
    <p id='second'>Second paragraph   text</p>
  )HTML");
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& selected_start = Position(second_paragraph, 0);
  const auto& selected_end = Position(second_paragraph, 23);
  bool callback_called = false;
  base::OnceCallback<void(const TextFragmentSelector&)> callback =
      base::BindLambdaForTesting([&](const TextFragmentSelector& selector) {
        EXPECT_EQ(selector.Type(), TextFragmentSelector::SelectorType::kExact);
        EXPECT_EQ(selector.Start(), "Second paragraph text");
        callback_called = true;
      });

  TextFragmentSelectorGenerator generator;
  generator.UpdateSelection(
      GetDocument().GetFrame(),
      ToEphemeralRangeInFlatTree(EphemeralRange(selected_start, selected_end)));
  generator.SetCallbackForTesting(std::move(callback));
  generator.GenerateSelector();

  EXPECT_TRUE(callback_called);
}

// Multi-block selection is currently not implemented.
TEST_F(TextFragmentSelectorGeneratorTest, MultiblockSelection) {
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
  bool callback_called = false;
  base::OnceCallback<void(const TextFragmentSelector&)> callback =
      base::BindLambdaForTesting([&](const TextFragmentSelector& selector) {
        EXPECT_EQ(selector.Type(),
                  TextFragmentSelector::SelectorType::kInvalid);
        callback_called = true;
      });

  TextFragmentSelectorGenerator generator;
  generator.UpdateSelection(
      GetDocument().GetFrame(),
      ToEphemeralRangeInFlatTree(EphemeralRange(selected_start, selected_end)));
  generator.SetCallbackForTesting(std::move(callback));
  generator.GenerateSelector();

  EXPECT_TRUE(callback_called);
}

}  // namespace blink
