// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_text_fragment_selector_generator.h"

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#import "base/functional/bind.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class SendTabToSelfTextFragmentSelectorGeneratorTest : public PlatformTest {
 protected:
  SendTabToSelfTextFragmentSelectorGeneratorTest()
      : web_client_(std::make_unique<web::FakeWebClient>()) {}

  void SetUp() override {
    PlatformTest::SetUp();

    profile_ = TestProfileIOS::Builder().Build();

    GetWebClient()->SetJavaScriptFeatures(
        {SendTabToSelfTextFragmentSelectorGenerator::GetInstance()});

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView().frame = CGRectMake(0, 0, 400, 400);
    web_state_->SetKeepRenderProcessAlive(true);
  }

  web::FakeWebClient* GetWebClient() {
    return static_cast<web::FakeWebClient*>(web_client_.Get());
  }

  web::WebState* web_state() { return web_state_.get(); }

  std::optional<SendTabToSelfTextFragment> GenerateTextFragment() {
    base::test::TestFuture<std::optional<SendTabToSelfTextFragment>> future;
    SendTabToSelfTextFragmentSelectorGenerator::GetInstance()->GetTextFragment(
        web_state(), future.GetCallback());
    return future.Take();
  }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
};

// Tests the happy path: `GetTextFragment` successfully generates a fragment
// when the center of the viewport contains valid text.
//
// This test implicitly validates two critical pieces of the integration:
// 1. `document.caretRangeFromPoint` correctly identifies the text node at the
//    center of the viewport.
// 2. The mock `Selection` object we construct from that `Range` satisfies the
//    internal DOM-traversal requirements of the `text-fragments-polyfill`
//    library (e.g., proper implementation of `getRangeAt()`, `toString()`).
TEST_F(SendTabToSelfTextFragmentSelectorGeneratorTest, GetTextFragmentSuccess) {
  // Load a simple HTML page where a single, large `<p>` tag covers the entire
  // 400x400 viewport we configured in `SetUp()`. This guarantees the center
  // point (200, 200) falls exactly on the text node.
  NSString* html =
      @"<html><body style='margin: 0; padding: 0;'>"
       "<p style='width: 400px; height: 400px; font-size: 20px;'>"
       "  This is some sample text that covers the viewport center."
       "</p>"
       "</body></html>";
  web::test::LoadHtml(html, web_state());

  std::optional<SendTabToSelfTextFragment> result = GenerateTextFragment();

  ASSERT_TRUE(result.has_value());
  // Verify that the status code from the polyfill indicates success.
  EXPECT_EQ(TextFragmentGenerationStatus::kSuccess, result->status);

  // Ensure the generated payload contains a structurally valid fragment
  // containing at least a non-empty `text_start` property, proving it found
  // targetable text.
  EXPECT_FALSE(result->text_start.empty());
}

// Tests that `GetTextFragment` gracefully handles scenarios where the
// center of the viewport contains no selectable text (e.g., an empty page).
//
// In this case, `document.caretRangeFromPoint` might return a range pointing
// to the `<body>` element, but our JavaScript explicitly ignores expanding
// ranges to the `<body>` element to avoid performance issues, resulting in an
// empty mock selection that the polyfill rejects.
TEST_F(SendTabToSelfTextFragmentSelectorGeneratorTest,
       GetTextFragmentNoTextAtCenter) {
  // Load an empty page with no content to select.
  web::test::LoadHtml(@"<html><body></body></html>", web_state());

  std::optional<SendTabToSelfTextFragment> result = GenerateTextFragment();

  ASSERT_TRUE(result.has_value());
  // Verify that the polyfill explicitly signals an invalid/empty selection.
  EXPECT_EQ(TextFragmentGenerationStatus::kInvalidSelection, result->status);
}

// Tests the safety mechanism that prevents the generator from freezing the page
// when the user is scrolled over a massive container with no text.
//
// The polyfill's text-traversal can be extremely expensive. If the range falls
// on an empty `<div>`, our JS attempts to select the `<div>`'s contents. If
// that `<div>` has hundreds of child nodes, analyzing it could trigger a
// watchdog timeout. Our script deliberately limits this to < 50 children.
TEST_F(SendTabToSelfTextFragmentSelectorGeneratorTest,
       GetTextFragmentSkipsGiantContainer) {
  // Construct a page with a 1000x1000 container positioned at the top left.
  // The container is filled with 100 empty child `<div>`s, exceeding our
  // safety threshold of 50.
  NSMutableString* divChildren = [NSMutableString string];
  for (int i = 0; i < 100; ++i) {
    [divChildren appendString:@"<div></div>"];
  }
  NSString* html = [NSString
      stringWithFormat:
          @"<html><body style='margin: 0;'>"
           "<div id='giant' style='height: 1000px; width: 1000px;'>%@</div>"
           "</body></html>",
          divChildren];
  web::test::LoadHtml(html, web_state());

  std::optional<SendTabToSelfTextFragment> result = GenerateTextFragment();

  ASSERT_TRUE(result.has_value());
  // Verify the range expansion aborted, leading to an invalid selection.
  EXPECT_EQ(TextFragmentGenerationStatus::kInvalidSelection, result->status);
}

// Tests that `GetTextFragment` safely fails when the viewport center lands
// on a non-text visual element, such as an image.
//
// Text fragments can only target text nodes. A point landing on an `<img>`
// cannot yield a viable text fragment.
TEST_F(SendTabToSelfTextFragmentSelectorGeneratorTest,
       GetTextFragmentHitsImage) {
  // Load a page with a single, massive 1000x1000 base64-encoded image that
  // completely envelopes the viewport.
  NSString* html = @"<html><body style='margin: 0;'>"
                    "<img "
                    "src='data:image/"
                    "png;base64,"
                    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42m"
                    "P8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJccc==' "
                    "style='width: 1000px; height: 1000px;'>"
                    "</body></html>";
  web::test::LoadHtml(html, web_state());

  std::optional<SendTabToSelfTextFragment> result = GenerateTextFragment();

  ASSERT_TRUE(result.has_value());
  // Verify that the polyfill failed to generate a fragment, returning a
  // non-success status code (typically `kInvalidSelection`).
  EXPECT_NE(TextFragmentGenerationStatus::kSuccess, result->status);
}

// Tests that the generator can successfully traverse deeply nested DOM
// structures to find text at the viewport center.
//
// Modern web pages often bury text inside `<span>`, `<article>`, `<b>`, etc.
// This test ensures that `document.caretRangeFromPoint` and our range expansion
// correctly bubble out of the nested tags to yield a valid selection for
// the polyfill.
TEST_F(SendTabToSelfTextFragmentSelectorGeneratorTest,
       GetTextFragmentDeeplyNested) {
  // Load a page with multiple layers of inline and block elements enclosing
  // the target text.
  NSString* html = @"<html><body style='margin: 0;'>"
                    "<div style='width: 400px; height: 400px;'>"
                    "  <section><span><article><b>"
                    "    Target text in deep nesting."
                    "  </b></article></span></section>"
                    "</div>"
                    "</body></html>";
  web::test::LoadHtml(html, web_state());

  std::optional<SendTabToSelfTextFragment> result = GenerateTextFragment();

  ASSERT_TRUE(result.has_value());
  // Verify success even with complex nesting.
  EXPECT_EQ(TextFragmentGenerationStatus::kSuccess, result->status);
}

// Tests that the generator works correctly in a long document by scrolling
// a specific element into the center before attempting generation.
//
// Since we trigger fragment generation dynamically based on the current
// scroll position, it is vital to prove that coordinates map correctly
// off-screen (by relying on the layout viewport and visual viewport offsets).
TEST_F(SendTabToSelfTextFragmentSelectorGeneratorTest,
       GetTextFragmentLargeArticle) {
  // Create a large page with many paragraphs to force scrolling.
  NSMutableString* paragraphs = [NSMutableString string];
  for (int i = 0; i < 50; ++i) {
    [paragraphs appendFormat:@"<p>Paragraph %d of the large article.</p>", i];
  }
  // Insert the target text in the middle.
  [paragraphs appendString:@"<p id='target'>Target text in the middle.</p>"];
  for (int i = 51; i < 100; ++i) {
    [paragraphs appendFormat:@"<p>Paragraph %d of the large article.</p>", i];
  }

  NSString* html = [NSString
      stringWithFormat:@"<html><body style='margin: 0; padding: 0;'>%@</body>"
                        "</html>",
                       paragraphs];
  web::test::LoadHtml(html, web_state());

  // Programmatically scroll the target element to the center of the viewport.
  web::test::ExecuteJavaScript(
      @"document.getElementById('target').scrollIntoView({block: 'center'});",
      web_state());

  std::optional<SendTabToSelfTextFragment> result = GenerateTextFragment();

  ASSERT_TRUE(result.has_value());
  // Verify that the generator correctly identifies the text now at the center.
  EXPECT_EQ(TextFragmentGenerationStatus::kSuccess, result->status);
}

// Tests that the generator can handle multiple adjacent text spans and
// successfully generate a fragment from them.
//
// Fragments often need to be generated for sentences broken apart by
// formatting (like `<span>` or `<a>`). This proves the polyfill can walk
// adjacent siblings accurately.
TEST_F(SendTabToSelfTextFragmentSelectorGeneratorTest,
       GetTextFragmentMultipleSpans) {
  // Load a page where the text is split across multiple `<span>` elements.
  // The center point might fall directly between spans or inside one.
  NSString* html =
      @"<html><body style='margin: 0; padding: 0; font-size: 20px;'>"
       "<div style='width: 400px; height: 400px;'>"
       "  <span>First part.</span>"
       "  <span>Second part.</span>"
       "  <span>Third part.</span>"
       "</div>"
       "</body></html>";
  web::test::LoadHtml(html, web_state());

  std::optional<SendTabToSelfTextFragment> result = GenerateTextFragment();

  ASSERT_TRUE(result.has_value());
  // Verify success when dealing with fragmented text elements.
  EXPECT_EQ(TextFragmentGenerationStatus::kSuccess, result->status);
}
