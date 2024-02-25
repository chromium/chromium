// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#import <optional>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "base/values.h"
#import "ios/web/find_in_page/find_in_page_constants.h"
#import "ios/web/find_in_page/find_in_page_java_script_feature.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/js_messaging/web_frame_impl.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "testing/gtest_mac.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Find strings.
const char kFindStringFoo[] = "foo";
const char kFindString12345[] = "12345";

// Pump search timeout.
constexpr base::TimeDelta kPumpSearchTimeout = base::Milliseconds(100);

}  // namespace

namespace web {

// Calls FindInPage Javascript handlers and checks that return values are
// correct.
class FindInPageJsTest : public WebTestWithWebState {
 protected:

  void SetUp() override {
    WebTestWithWebState::SetUp();

    WKWebViewConfigurationProvider& configuration_provider =
        WKWebViewConfigurationProvider::FromBrowserState(GetBrowserState());
    // Force the creation of the content worlds.
    configuration_provider.GetWebViewConfiguration();

    content_world_ =
        JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
            ->GetContentWorldForFeature(
                FindInPageJavaScriptFeature::GetInstance());
  }

  bool WaitForWebFramesCount(unsigned long web_frames_count) {
    return WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
      return all_web_frames().size() == web_frames_count;
    });
  }

  // Returns all web frames for `web_state()`.
  std::set<WebFrameImpl*> all_web_frames() {
    std::set<WebFrameImpl*> frames;
    for (WebFrame* frame :
         web_state()->GetPageWorldWebFramesManager()->GetAllWebFrames()) {
      frames.insert(static_cast<WebFrameImpl*>(frame));
    }
    return frames;
  }
  // Returns main frame for `web_state_`.
  WebFrameInternal* main_web_frame() {
    WebFrame* main_frame =
        web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame();
    return main_frame->GetWebFrameInternal();
  }

  raw_ptr<JavaScriptContentWorld> content_world_;
};

// Tests that FindInPage searches in main frame containing a match and responds
// with 1 match.
TEST_F(FindInPageJsTest, FindText) {
  ASSERT_TRUE(LoadHtml("<span>foo</span>"));
  ASSERT_TRUE(WaitForWebFramesCount(1));

  __block bool message_received = false;
  auto params = base::Value::List()
                    .Append(kFindStringFoo)
                    .Append(kPumpSearchTimeout.InMillisecondsF());
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSearch, params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        ASSERT_TRUE(result);
        ASSERT_TRUE(result->is_double());
        double count = result->GetDouble();
        ASSERT_EQ(1.0, count);
        message_received = true;
      }),
      kWaitForJSCompletionTimeout);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received;
  }));
}

// Tests that FindInPage searches in main frame for text that exists but is
// hidden and responds with 0 matches.
TEST_F(FindInPageJsTest, FindTextNoResults) {
  ASSERT_TRUE(LoadHtml("<span style='display:none'>foo</span>"));
  ASSERT_TRUE(WaitForWebFramesCount(1));

  __block bool message_received = false;
  auto params = base::Value::List()
                    .Append(kFindStringFoo)
                    .Append(kPumpSearchTimeout.InMillisecondsF());
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSearch, params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        ASSERT_TRUE(result);
        ASSERT_TRUE(result->is_double());
        double count = result->GetDouble();
        ASSERT_EQ(0.0, count);
        message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received;
  }));
}

// Tests that FindInPage doesn't search in noscript elements.
TEST_F(FindInPageJsTest, FindTextIgnoresNoscript) {
  ASSERT_TRUE(LoadHtml("<body><noscript>foo</noscript></body>"));
  ASSERT_TRUE(WaitForWebFramesCount(1));

  __block bool message_received = false;
  auto params = base::Value::List()
                    .Append(kFindStringFoo)
                    .Append(kPumpSearchTimeout.InMillisecondsF());
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSearch, params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        ASSERT_TRUE(result);
        ASSERT_TRUE(result->is_double());
        double count = result->GetDouble();
        ASSERT_EQ(0.0, count);
        message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received;
  }));
}

// Tests that FindInPage searches in child iframe and asserts that a result was
// found.
TEST_F(FindInPageJsTest, FindIFrameText) {
  ASSERT_TRUE(WebTestWithWebState::LoadHtml(
      "<iframe "
      "srcdoc='<html><body><span>foo</span></body></html>'></iframe>"));
  ASSERT_TRUE(WaitForWebFramesCount(2));

  std::set<WebFrameImpl*> all_frames = all_web_frames();
  __block bool message_received = false;
  WebFrameInternal* child_frame = nullptr;
  for (auto* frame : all_frames) {
    if (frame->IsMainFrame()) {
      continue;
    }
    child_frame = frame->GetWebFrameInternal();
  }
  ASSERT_TRUE(child_frame);
  auto params = base::Value::List()
                    .Append(kFindStringFoo)
                    .Append(kPumpSearchTimeout.InMillisecondsF());
  child_frame->CallJavaScriptFunctionInContentWorld(
      kFindInPageSearch, params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        ASSERT_TRUE(result);
        ASSERT_TRUE(result->is_double());
        double count = result->GetDouble();
        ASSERT_EQ(1.0, count);
        message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received;
  }));
}

// Tests that FindInPage works when searching for white space.
TEST_F(FindInPageJsTest, FindWhiteSpace) {
  ASSERT_TRUE(LoadHtml("<span> </span>"));
  ASSERT_TRUE(WaitForWebFramesCount(1));

  __block bool message_received = false;
  auto params = base::Value::List().Append(" ").Append(
      kPumpSearchTimeout.InMillisecondsF());
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSearch, params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        ASSERT_TRUE(result);
        ASSERT_TRUE(result->is_double());
        double count = result->GetDouble();
        ASSERT_EQ(1.0, count);
        message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received;
  }));
}

// Tests that FindInPage works when match results cover multiple HTML Nodes.
TEST_F(FindInPageJsTest, FindAcrossMultipleNodes) {
  ASSERT_TRUE(
      LoadHtml("<p>xx1<span>2</span>3<a>4512345xxx12</a>34<a>5xxx12345xx</p>"));
  ASSERT_TRUE(WaitForWebFramesCount(1));

  __block bool message_received = false;
  auto params = base::Value::List()
                    .Append(kFindString12345)
                    .Append(kPumpSearchTimeout.InMillisecondsF());
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSearch, params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        ASSERT_TRUE(result);
        ASSERT_TRUE(result->is_double());
        double count = result->GetDouble();
        ASSERT_EQ(4.0, count);
        message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received;
  }));
}

// Tests that a FindInPage match can be highlighted and the correct
// accessibility string is returned.
TEST_F(FindInPageJsTest, FindHighlightMatch) {
  ASSERT_TRUE(LoadHtml("<span>some foo match</span>"));
  ASSERT_TRUE(WaitForWebFramesCount(1));

  __block bool message_received = false;
  auto params = base::Value::List()
                    .Append(kFindStringFoo)
                    .Append(kPumpSearchTimeout.InMillisecondsF());
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSearch, params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        ASSERT_TRUE(result);
        ASSERT_TRUE(result->is_double());
        double count = result->GetDouble();
        ASSERT_EQ(1.0, count);
        message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received;
  }));

  __block bool highlight_done = false;
  __block std::string context_string;
  auto highlight_params = base::Value::List().Append(0);
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSelectAndScrollToMatch, highlight_params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        highlight_done = true;
        context_string =
            *result->GetDict().FindString(kSelectAndScrollResultContextString);
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return highlight_done;
  }));

  EXPECT_NSEQ(@1,
              ExecuteJavaScript(
                  @"document.getElementsByClassName('find_selected').length"));
  EXPECT_EQ("some foo match", context_string);
}

// Tests that a FindInPage match can be highlighted and that a previous
// highlight is removed when another match is highlighted.
TEST_F(FindInPageJsTest, FindHighlightSeparateMatches) {
  ASSERT_TRUE(LoadHtml("<span>foo foo match</span>"));
  ASSERT_TRUE(WaitForWebFramesCount(1));

  __block bool message_received = false;
  auto params = base::Value::List()
                    .Append(kFindStringFoo)
                    .Append(kPumpSearchTimeout.InMillisecondsF());
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSearch, params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        ASSERT_TRUE(result);
        ASSERT_TRUE(result->is_double());
        double count = result->GetDouble();
        ASSERT_EQ(2.0, count);
        message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received;
  }));

  __block bool highlight_done = false;
  __block std::string context_string;
  auto highlight_params = base::Value::List().Append(0);
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSelectAndScrollToMatch, highlight_params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        highlight_done = true;
        context_string =
            *result->GetDict().FindString(kSelectAndScrollResultContextString);
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return highlight_done;
  }));

  EXPECT_EQ("foo ", context_string);
  EXPECT_NSEQ(@1,
              ExecuteJavaScript(
                  @"document.getElementsByClassName('find_selected').length"));

  highlight_done = false;
  auto highlight_second_params = base::Value::List().Append(1);
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSelectAndScrollToMatch, highlight_second_params,
      content_world_, base::BindOnce(^(const base::Value* result) {
        highlight_done = true;
        context_string =
            *result->GetDict().FindString(kSelectAndScrollResultContextString);
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return highlight_done;
  }));

  EXPECT_EQ(" foo match", context_string);
  id inner_html = ExecuteJavaScript(@"document.body.innerHTML");
  ASSERT_TRUE([inner_html isKindOfClass:[NSString class]]);
  EXPECT_TRUE([inner_html
      containsString:@"<chrome_find class=\"find_in_page\">foo</chrome_find> "
                     @"<chrome_find class=\"find_in_page "
                     @"find_selected\">foo</chrome_find>"]);
  EXPECT_TRUE(
      [inner_html containsString:@"find_selected{background-color:#ff9632"]);
}

// Tests that FindInPage does not highlight any matches given an invalid index.
TEST_F(FindInPageJsTest, FindHighlightMatchAtInvalidIndex) {
  ASSERT_TRUE(LoadHtml("<span>invalid </span>"));
  ASSERT_TRUE(WaitForWebFramesCount(1));

  __block bool message_received = false;
  auto params = base::Value::List()
                    .Append(kFindStringFoo)
                    .Append(kPumpSearchTimeout.InMillisecondsF());
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSearch, params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        ASSERT_TRUE(result);
        ASSERT_TRUE(result->is_double());
        double count = result->GetDouble();
        ASSERT_TRUE(count == 0.0);
        message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received;
  }));

  __block bool highlight_done = false;
  auto highlight_params = base::Value::List().Append(0);
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSelectAndScrollToMatch, highlight_params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        highlight_done = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return highlight_done;
  }));

  EXPECT_NSEQ(@0,
              ExecuteJavaScript(
                  @"document.getElementsByClassName('find_selected').length"));
}

// Tests that FindInPage works when searching for strings with non-ascii
// characters.
TEST_F(FindInPageJsTest, SearchForNonAscii) {
  ASSERT_TRUE(LoadHtml("<span>école francais</span>"));
  ASSERT_TRUE(WaitForWebFramesCount(1));

  __block bool message_received = false;
  auto params = base::Value::List().Append("école").Append(
      kPumpSearchTimeout.InMillisecondsF());
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSearch, params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        ASSERT_TRUE(result);
        ASSERT_TRUE(result->is_double());
        double count = result->GetDouble();
        ASSERT_EQ(1.0, count);
        message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received;
  }));
}

// Tests that FindInPage scrolls page to bring selected match into view.
TEST_F(FindInPageJsTest, CheckFindInPageScrollsToMatch) {
  // Set frame so that offset can be predictable across devices.
  web_state()->GetView().frame = CGRectMake(0, 0, 300, 200);

  // Create HTML with div of height 4000px followed by a span below with
  // searchable text in order to ensure that the text begins outside of screen
  // on all devices.
  ASSERT_TRUE(
      LoadHtml("<div style=\"height: 4000px;\"></div><span>foo</span>"));
  ASSERT_TRUE(WaitForWebFramesCount(1));

  __block bool message_received = false;
  auto params = base::Value::List()
                    .Append(kFindStringFoo)
                    .Append(kPumpSearchTimeout.InMillisecondsF());
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSearch, params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        ASSERT_TRUE(result);
        ASSERT_TRUE(result->is_double());
        ASSERT_EQ(1.0, result->GetDouble());
        message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received;
  }));

  __block bool highlight_done = false;
  auto highlight_params = base::Value::List().Append(0);
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSelectAndScrollToMatch, highlight_params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        highlight_done = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return highlight_done;
  }));

  // Check that page has scrolled to the match.
  __block CGFloat top_scroll_after_select = 0.0;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    top_scroll_after_select =
        web_state()->GetWebViewProxy().scrollViewProxy.contentOffset.y;
    return top_scroll_after_select > 0;
  }));
  // Scroll offset should either be 1035.333 for most iPhone and 1035.5 for iPad
  // and 5S.
  EXPECT_NEAR(top_scroll_after_select, 1035, 1.0);
}

// Tests that FindInPage is able to clear CSS and match highlighting.
TEST_F(FindInPageJsTest, StopFindInPage) {
  ASSERT_TRUE(LoadHtml("<span>foo foo</span>"));
  ASSERT_TRUE(WaitForWebFramesCount(1));

  // Do a search to ensure match highlighting is cleared properly.
  __block bool message_received = false;
  auto params = base::Value::List()
                    .Append(kFindStringFoo)
                    .Append(kPumpSearchTimeout.InMillisecondsF());
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSearch, params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return message_received;
  }));

  message_received = false;
  auto highlight_params = base::Value::List().Append(0);
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSelectAndScrollToMatch, highlight_params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return message_received;
  }));

  message_received = false;
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageStop, {}, content_world_,
      base::BindOnce(^(const base::Value* result) {
        message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return message_received;
  }));

  id inner_html = ExecuteJavaScript(@"document.body.innerHTML");
  ASSERT_TRUE([inner_html isKindOfClass:[NSString class]]);
  EXPECT_FALSE([inner_html containsString:@"find_selected"]);
  EXPECT_FALSE([inner_html containsString:@"find_in_page"]);
  EXPECT_FALSE([inner_html containsString:@"chrome_find"]);
}

// Tests that FindInPage only selects the visible match when there is also a
// hidden match.
TEST_F(FindInPageJsTest, HiddenMatch) {
  ASSERT_TRUE(
      LoadHtml("<span style='display:none'>foo</span><span>foo bar</span>"));
  ASSERT_TRUE(WaitForWebFramesCount(1));

  __block bool message_received = false;
  auto params = base::Value::List()
                    .Append(kFindStringFoo)
                    .Append(kPumpSearchTimeout.InMillisecondsF());
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSearch, params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        ASSERT_TRUE(result);
        ASSERT_TRUE(result->is_double());
        double count = result->GetDouble();
        ASSERT_EQ(1.0, count);
        message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received;
  }));

  message_received = false;
  auto highlight_params = base::Value::List().Append(0);
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSelectAndScrollToMatch, highlight_params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return message_received;
  }));

  id inner_html = ExecuteJavaScript(@"document.body.innerHTML");
  ASSERT_TRUE([inner_html isKindOfClass:[NSString class]]);
  NSRange visible_match =
      [inner_html rangeOfString:@"find_in_page find_selected"];
  NSRange hidden_match = [inner_html rangeOfString:@"find_in_page"];
  // Assert that the selected match comes after the first match in the DOM since
  // it is expected the hidden match is skipped.
  EXPECT_GT(visible_match.location, hidden_match.location);
}

// Tests that FindInPage responds with an updated match count when a once
// hidden match becomes visible after a search finishes.
TEST_F(FindInPageJsTest, HiddenMatchBecomesVisible) {
  ASSERT_TRUE(LoadHtml("<span>foo</span><span id=\"hidden_match\" "
                       "style='display:none'>foo</span>"));
  ASSERT_TRUE(WaitForWebFramesCount(1));

  __block bool message_received = false;
  auto params = base::Value::List()
                    .Append(kFindStringFoo)
                    .Append(kPumpSearchTimeout.InMillisecondsF());
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSearch, params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        ASSERT_TRUE(result);
        ASSERT_TRUE(result->is_double());
        double count = result->GetDouble();
        ASSERT_EQ(1.0, count);
        message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received;
  }));

  ExecuteJavaScript(
      @"document.getElementById('hidden_match').removeAttribute('style')");
  message_received = false;
  auto highlight_params = base::Value::List().Append(0);
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSelectAndScrollToMatch, highlight_params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        ASSERT_TRUE(result);
        ASSERT_TRUE(result->is_dict());
        const base::Value::Dict& result_dict = result->GetDict();
        const std::optional<double> count =
            result_dict.FindDouble(kSelectAndScrollResultMatches);
        ASSERT_TRUE(count);
        ASSERT_EQ(2.0, count.value());
        const std::optional<double> index =
            result_dict.FindDouble(kSelectAndScrollResultIndex);
        ASSERT_TRUE(index);
        ASSERT_EQ(0.0, index.value());
        message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return message_received;
  }));
}

// Tests that FindInPage highlights the next visible match when attempting to
// select a match that was once visible but is no longer.
TEST_F(FindInPageJsTest, MatchBecomesInvisible) {
  ASSERT_TRUE(LoadHtml(
      "<span>foo foo </span> <span id=\"matches_to_hide\">foo foo</span>"));
  ASSERT_TRUE(WaitForWebFramesCount(1));

  __block bool message_received = false;
  auto params = base::Value::List()
                    .Append(kFindStringFoo)
                    .Append(kPumpSearchTimeout.InMillisecondsF());
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSearch, params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        ASSERT_TRUE(result);
        ASSERT_TRUE(result->is_double());
        double count = result->GetDouble();
        EXPECT_EQ(4.0, count);
        message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received;
  }));

  __block bool select_last_match_message_received = false;
  auto select_params = base::Value::List().Append(3);
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSelectAndScrollToMatch, select_params, content_world_,
      base::BindOnce(^(const base::Value* result) {
        ASSERT_TRUE(result);
        ASSERT_TRUE(result->is_dict());
        const base::Value::Dict& result_dict = result->GetDict();
        const std::optional<double> index =
            result_dict.FindDouble(kSelectAndScrollResultIndex);
        ASSERT_TRUE(index);
        EXPECT_EQ(3.0, index.value());
        select_last_match_message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return select_last_match_message_received;
  }));

  ExecuteJavaScript(
      @"document.getElementById('matches_to_hide').style.display = \"none\";");

  __block bool select_third_match_message_received = false;
  auto select_third_match_params = base::Value::List().Append(2);
  main_web_frame()->CallJavaScriptFunctionInContentWorld(
      kFindInPageSelectAndScrollToMatch, select_third_match_params,
      content_world_, base::BindOnce(^(const base::Value* result) {
        ASSERT_TRUE(result);
        ASSERT_TRUE(result->is_dict());
        const base::Value::Dict& result_dict = result->GetDict();
        const std::optional<double> index =
            result_dict.FindDouble(kSelectAndScrollResultIndex);
        ASSERT_TRUE(index);
        // Since there are only two visible matches now and this
        // kFindInPageSelectAndScrollToMatch call is asking Find in Page to
        // traverse to a previous match, Find in Page should look for the next
        // previous visible match. This happens to be the 2nd match.
        EXPECT_EQ(1.0, index.value());
        select_third_match_message_received = true;
      }),
      kWaitForJSCompletionTimeout);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return select_third_match_message_received;
  }));
}

}  // namespace web
