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
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
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

  GenerateAndVerifySelector(selected_start, selected_end,
                            "First%20paragraph%20text%20that%20is");
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

  GenerateAndVerifySelector(selected_start, selected_end,
                            "First%20paragraph%20text%20that%20is%20longer");
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

  GenerateAndVerifySelector(selected_start, selected_end,
                            "Second%20paragraph%20text");
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

  GenerateAndVerifySelector(selected_start, selected_end, "");
}

}  // namespace blink
