// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "base/test/ios/wait_util.h"
#import "components/dom_distiller/core/mojom/distilled_page_prefs.mojom.h"
#import "components/dom_distiller/core/viewer.h"
#import "ios/chrome/test/ios_chrome_test_with_web_state.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "url/gurl.h"

namespace {

constexpr char kTestArticleUrl[] = "https://example.com/article";
constexpr char kTestCspNonce[] = "secret_nonce_12345678";

}  // namespace

// Test fixture verifying Content Security Policy enforcement for Reader Mode
// documents committed via WebState::LoadData().
using ReaderModeDistillerViewerTest = IOSChromeTestWithWebState;

// Tests that when Reader Mode HTML is generated with a CSP nonce and loaded via
// WebState::LoadData(), inline event handlers (e.g. onerror) do NOT execute,
// while nonced scripts execute successfully.
TEST_F(ReaderModeDistillerViewerTest,
       TestCspBlocksInlineEventHandlersInLoadData) {
  const GURL article_url(kTestArticleUrl);
  const std::string csp_nonce(kTestCspNonce);

  // Load an initial page so the WebState commits a navigation item and realizes
  // its web view, matching the state when Reader Mode loads data over an
  // article.
  web::test::LoadHtml(@"<html><body>Initial Page</body></html>", article_url,
                      web_state());

  // Generate article template HTML with the CSP nonce in non-offline mode.
  std::string html = dom_distiller::viewer::GetArticleTemplateHtml(
      dom_distiller::mojom::Theme::kLight,
      dom_distiller::mojom::FontFamily::kSansSerif, csp_nonce,
      /*use_offline_data=*/false);

  // Embed an inline event handler on an image that fails to load (surviving
  // distillation), an un-nonced script, and a valid nonced script representing
  // the viewer bootstrap script.
  html += "<div id='content'>";
  html += "<img src='data:image/png,malformed' "
          "onerror='window.inlineEventHandlerRan = true'>";
  html += "</div>";
  html += "<script>window.unnoncedScriptRan = true;</script>";
  html += "<script nonce='" + csp_nonce + "'>";
  html += "window.noncedScriptRan = true;";
  html += "</script>";

  NSData* data = [NSData dataWithBytes:html.data() length:html.length()];
  web_state()->LoadData(data, @"text/html", article_url);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^bool {
        return !web_state()->IsLoading();
      }));

  // Wait for the nonced script to execute.
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        id result = web::test::ExecuteJavaScript(
            @"window.noncedScriptRan === true", web_state());
        return [result isEqual:@YES];
      }));

  id nonced_ran = web::test::ExecuteJavaScript(
      @"window.noncedScriptRan === true", web_state());
  EXPECT_NSEQ(@YES, nonced_ran);

  // Verify that the inline event handler was blocked by CSP and did not
  // execute.
  id inline_event_handler_ran = web::test::ExecuteJavaScript(
      @"window.inlineEventHandlerRan === true", web_state());
  EXPECT_NSEQ(@NO, inline_event_handler_ran);

  // Verify that the un-nonced script was blocked by CSP and did not execute.
  id unnonced_ran = web::test::ExecuteJavaScript(
      @"window.unnoncedScriptRan === true", web_state());
  EXPECT_NSEQ(@NO, unnonced_ran);
}

// Tests that without a CSP nonce (the pre-fix condition), inline event
// handlers and un-nonced scripts DO execute, confirming that the CSP is
// responsible for blocking them.
TEST_F(ReaderModeDistillerViewerTest,
       TestInlineEventHandlerExecutesWithoutCsp) {
  const GURL article_url(kTestArticleUrl);

  // Load an initial page so the WebState commits a navigation item and realizes
  // its web view, matching the state when Reader Mode loads data over an
  // article.
  web::test::LoadHtml(@"<html><body>Initial Page</body></html>", article_url,
                      web_state());

  // Generate article template HTML without a CSP nonce.
  std::string html = dom_distiller::viewer::GetArticleTemplateHtml(
      dom_distiller::mojom::Theme::kLight,
      dom_distiller::mojom::FontFamily::kSansSerif,
      /*csp_nonce=*/"",
      /*use_offline_data=*/false);

  html += "<div id='content'>";
  html += "<img src='data:image/png,malformed' "
          "onerror='window.inlineEventHandlerRan = true'>";
  html += "</div>";
  html += "<script>window.unnoncedScriptRan = true;</script>";

  NSData* data = [NSData dataWithBytes:html.data() length:html.length()];
  web_state()->LoadData(data, @"text/html", article_url);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^bool {
        return !web_state()->IsLoading();
      }));

  // Wait for the onerror handler to execute.
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        id result = web::test::ExecuteJavaScript(
            @"window.inlineEventHandlerRan === true", web_state());
        return [result isEqual:@YES];
      }));

  id inline_event_handler_ran = web::test::ExecuteJavaScript(
      @"window.inlineEventHandlerRan === true", web_state());
  EXPECT_NSEQ(@YES, inline_event_handler_ran);

  // Wait for the un-nonced script to execute.
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        id result = web::test::ExecuteJavaScript(
            @"window.unnoncedScriptRan === true", web_state());
        return [result isEqual:@YES];
      }));

  id unnonced_ran = web::test::ExecuteJavaScript(
      @"window.unnoncedScriptRan === true", web_state());
  EXPECT_NSEQ(@YES, unnonced_ran);
}
