// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/web_test_with_web_state.h"

#include "base/ios/ios_util.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/js_messaging/crw_js_injector.h"
#import "ios/web/navigation/crw_wk_navigation_states.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/navigation/wk_navigation_util.h"
#include "ios/web/public/deprecated/url_verification_constants.h"
#import "ios/web/public/web_client.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForActionTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForPageLoadTimeout;

namespace {
// Returns CRWWebController for the given |web_state|.
CRWWebController* GetWebController(web::WebState* web_state) {
  web::WebStateImpl* web_state_impl =
      static_cast<web::WebStateImpl*>(web_state);
  return web_state_impl->GetWebController();
}
}  // namespace

namespace web {

WebTestWithWebState::WebTestWithWebState(WebTaskEnvironment::Options options)
    : WebTest(options) {}

WebTestWithWebState::WebTestWithWebState(
    std::unique_ptr<web::WebClient> web_client,
    WebTaskEnvironment::Options options)
    : WebTest(std::move(web_client), options) {}

WebTestWithWebState::~WebTestWithWebState() {}

void WebTestWithWebState::SetUp() {
  WebTest::SetUp();
  web::WebState::CreateParams params(GetBrowserState());
  web_state_ = web::WebState::Create(params);

  // Force generation of child views; necessary for some tests.
  web_state_->GetView();

  web_state()->SetKeepRenderProcessAlive(true);
}

void WebTestWithWebState::TearDown() {
  DestroyWebState();
  WebTest::TearDown();
}

void WebTestWithWebState::AddPendingItem(const GURL& url,
                                         ui::PageTransition transition) {
  GetWebController(web_state())
      .webStateImpl->GetNavigationManagerImpl()
      .AddPendingItem(url, Referrer(), transition,
                      web::NavigationInitiationType::BROWSER_INITIATED,
                      web::NavigationManager::UserAgentOverrideOption::INHERIT);
}

void WebTestWithWebState::AddTransientItem(const GURL& url) {
  GetWebController(web_state())
      .webStateImpl->GetNavigationManagerImpl()
      .AddTransientItem(url);
}

bool WebTestWithWebState::LoadHtmlWithoutSubresources(const std::string& html) {
  NSString* block_all = @"[{"
                         "  \"trigger\": {"
                         "    \"url-filter\": \".*\""
                         "  },"
                         "  \"action\": {"
                         "    \"type\": \"block\""
                         "  }"
                         "}]";
  __block WKContentRuleList* content_rule_list = nil;
  __block NSError* error = nil;
  __block BOOL rule_compilation_completed = NO;
  [WKContentRuleListStore.defaultStore
      compileContentRuleListForIdentifier:@"block_everything"
                   encodedContentRuleList:block_all
                        completionHandler:^(WKContentRuleList* rule_list,
                                            NSError* err) {
                          error = err;
                          content_rule_list = rule_list;
                          rule_compilation_completed = YES;
                        }];

  bool success = WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    return rule_compilation_completed;
  });
  if (!success) {
    DLOG(WARNING) << "ContentRuleList compilation timed out.";
    return false;
  }
  if (error) {
    DLOG(WARNING) << "ContentRuleList compilation failed with error: "
                  << base::SysNSStringToUTF8(error.description);
    return false;
  }
  DCHECK(content_rule_list);
  WKWebViewConfigurationProvider& configuration_provider =
      WKWebViewConfigurationProvider::FromBrowserState(GetBrowserState());
  WKWebViewConfiguration* configuration =
      configuration_provider.GetWebViewConfiguration();
  [configuration.userContentController addContentRuleList:content_rule_list];
  bool result = LoadHtml(html);
  [configuration.userContentController removeContentRuleList:content_rule_list];
  return result;
}

void WebTestWithWebState::LoadHtml(NSString* html, const GURL& url) {
  // Initiate asynchronous HTML load.
  CRWWebController* web_controller = GetWebController(web_state());
  ASSERT_EQ(web::WKNavigationState::FINISHED, web_controller.navigationState);

  // If the underlying WKWebView is empty, first load a placeholder to create a
  // WKBackForwardListItem to store the NavigationItem associated with the
  // |-loadHTML|.
  // TODO(crbug.com/777884): consider changing |-loadHTML| to match WKWebView's
  // |-loadHTMLString:baseURL| that doesn't create a navigation entry.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
      !web_state()->GetNavigationManager()->GetItemCount()) {
    GURL placeholder_url = wk_navigation_util::CreatePlaceholderUrlForUrl(url);
    NavigationManager::WebLoadParams params(placeholder_url);
    web_state()->GetNavigationManager()->LoadURLWithParams(params);

    // Set NoNavigationError so the placeHolder doesn't trigger a
    // kNavigatingToFailedNavigationItem.
    web::WebStateImpl* web_state_impl =
        static_cast<web::WebStateImpl*>(web_state());
    web_state_impl->GetNavigationManagerImpl()
        .GetCurrentItemImpl()
        ->error_retry_state_machine()
        .SetIgnorePlaceholderNavigation();

    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
      return web_controller.navigationState == web::WKNavigationState::FINISHED;
    }));
  }

  [web_controller loadHTML:html forURL:url];
  ASSERT_EQ(web::WKNavigationState::REQUESTED, web_controller.navigationState);

  // Wait until the page is loaded.
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return web_controller.navigationState == web::WKNavigationState::FINISHED;
  }));

  // Wait until the script execution is possible. Script execution will fail if
  // WKUserScript was not jet injected by WKWebView.
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return [ExecuteJavaScript(@"0;") isEqual:@0];
  }));
}

void WebTestWithWebState::LoadHtml(NSString* html) {
  GURL url("https://chromium.test/");
  LoadHtml(html, url);
}

bool WebTestWithWebState::LoadHtml(const std::string& html) {
  LoadHtml(base::SysUTF8ToNSString(html));
  // TODO(crbug.com/780062): LoadHtml(NSString*) should return bool.
  return true;
}

void WebTestWithWebState::WaitForBackgroundTasks() {
  // Because tasks can add new tasks to either queue, the loop continues until
  // the first pass where no activity is seen from either queue.
  bool activitySeen = false;
  base::MessageLoopCurrent messageLoop = base::MessageLoopCurrent::Get();
  messageLoop->AddTaskObserver(this);
  do {
    activitySeen = false;

    // Yield to the iOS message queue, e.g. [NSObject performSelector:] events.
    if (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true) ==
        kCFRunLoopRunHandledSource)
      activitySeen = true;

    // Yield to the Chromium message queue, e.g. WebThread::PostTask()
    // events.
    processed_a_task_ = false;
    base::RunLoop().RunUntilIdle();
    if (processed_a_task_)  // Set in TaskObserver method.
      activitySeen = true;

  } while (activitySeen);
  messageLoop->RemoveTaskObserver(this);
}

void WebTestWithWebState::WaitForCondition(ConditionBlock condition) {
  base::test::ios::WaitUntilCondition(condition, true,
                                      base::TimeDelta::FromSeconds(1000));
}

id WebTestWithWebState::ExecuteJavaScript(NSString* script) {
  __block id execution_result = nil;
  __block bool execution_completed = false;
  SCOPED_TRACE(base::SysNSStringToUTF8(script));
  [GetWebController(web_state()).jsInjector
      executeJavaScript:script
      completionHandler:^(id result, NSError* error) {
        // Most of executed JS does not return the result, and there is no need
        // to log WKErrorJavaScriptResultTypeIsUnsupported error code.
        if (error && error.code != WKErrorJavaScriptResultTypeIsUnsupported) {
          DLOG(WARNING) << base::SysNSStringToUTF8(error.localizedDescription);
        }
        execution_result = [result copy];
        execution_completed = true;
      }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return execution_completed;
  }));

  return execution_result;
}

void WebTestWithWebState::DestroyWebState() {
  web_state_.reset();
}

std::string WebTestWithWebState::BaseUrl() const {
  web::URLVerificationTrustLevel unused_level;
  return web_state()->GetCurrentURL(&unused_level).spec();
}

web::WebState* WebTestWithWebState::web_state() {
  return web_state_.get();
}

const web::WebState* WebTestWithWebState::web_state() const {
  return web_state_.get();
}

void WebTestWithWebState::WillProcessTask(const base::PendingTask&) {
  // Nothing to do.
}

void WebTestWithWebState::DidProcessTask(const base::PendingTask&) {
  processed_a_task_ = true;
}

}  // namespace web
