// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/text_fragment_finder.h"

#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

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

class TextFragmentFinderTest : public SimTest,
                               public TextFragmentFinder::Client {
 public:
  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  }

  void NoMatchFound() override { no_match_called_ = true; }

  void DidFindMatch(const RangeInFlatTree& match,
                    const TextFragmentAnchorMetrics::Match match_metrics,
                    bool is_unique) override {}
  bool IsNoMatchFoundCalled() { return no_match_called_; }

 private:
  bool no_match_called_ = false;
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

  MockTextFragmentFinder* finder = MakeGarbageCollected<MockTextFragmentFinder>(
      *this, selector, &GetDocument(),
      TextFragmentFinder::FindBufferRunnerType::kSynchronous);
  finder->FindMatch();

  finder->FindPrefix();
  EXPECT_EQ(false, IsNoMatchFoundCalled());

  finder->FindTextStart();
  EXPECT_EQ(false, IsNoMatchFoundCalled());

  Node* input = GetDocument().getElementById("input");
  input->remove();
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  finder->FindSuffix();
  EXPECT_EQ(true, IsNoMatchFoundCalled());
}

}  // namespace blink
