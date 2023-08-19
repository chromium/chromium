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
                         FindBufferRunnerType runner_type)
      : TextFragmentFinder(client, selector, document, runner_type) {}

 private:
  void GoToStep(SelectorMatchStep step) override { step_ = step; }
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
      TextFragmentFinder::FindBufferRunnerType::kSynchronous);
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

}  // namespace blink
