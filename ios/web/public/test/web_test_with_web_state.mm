// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/web_test_with_web_state.h"

#include "base/ios/ios_util.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/current_thread.h"
#import "base/test/ios/wait_util.h"
#include "ios/web/common/features.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/navigation/wk_navigation_util.h"
#include "ios/web/public/deprecated/url_verification_constants.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_client.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/test/js_test_util_internal.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/web_state_impl.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForActionTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForPageLoadTimeout;

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
  web::test::GetWebController(web_state())
      .webStateImpl->GetNavigationManagerImpl()
      .AddPendingItem(url, Referrer(), transition,
                      web::NavigationInitiationType::BROWSER_INITIATED,
                      /*is_post_navigation=*/false,
                      /*is_using_https_as_default_scheme=*/false);
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
  LoadHtmlInWebState(html, url, web_state());
}

void WebTestWithWebState::LoadHtml(NSString* html) {
  LoadHtmlInWebState(html, web_state());
}

bool WebTestWithWebState::LoadHtml(const std::string& html) {
  return LoadHtmlInWebState(html, web_state());
}

void WebTestWithWebState::LoadHtmlInWebState(NSString* html,
                                             const GURL& url,
                                             WebState* web_state) {
  web::test::LoadHtml(html, url, web_state);
}

void WebTestWithWebState::LoadHtmlInWebState(NSString* html,
                                             WebState* web_state) {
  web::test::LoadHtml(html, web_state);
}

bool WebTestWithWebState::LoadHtmlInWebState(const std::string& html,
                                             WebState* web_state) {
  LoadHtmlInWebState(base::SysUTF8ToNSString(html), web_state);
  // TODO(crbug.com/780062): LoadHtmlInWebState(NSString*) should return bool.
  return true;
}

void WebTestWithWebState::WaitForBackgroundTasks() {
  // Because tasks can add new tasks to either queue, the loop continues until
  // the first pass where no activity is seen from either queue.
  bool activitySeen = false;
  base::CurrentThread messageLoop = base::CurrentThread::Get();
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
  base::test::ios::WaitUntilCondition(condition, true, base::Seconds(1000));
}

bool WebTestWithWebState::WaitUntilLoaded() {
  return WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    WaitForBackgroundTasks();
    return !web_state()->IsLoading();
  });
}

std::unique_ptr<base::Value> WebTestWithWebState::CallJavaScriptFunction(
    const std::string& function,
    const std::vector<base::Value>& parameters) {
  return web::test::CallJavaScriptFunction(web_state(), function, parameters);
}

std::unique_ptr<base::Value>
WebTestWithWebState::CallJavaScriptFunctionForFeature(
    const std::string& function,
    const std::vector<base::Value>& parameters,
    JavaScriptFeature* feature) {
  return web::test::CallJavaScriptFunctionForFeature(web_state(), function,
                                                     parameters, feature);
}

id WebTestWithWebState::ExecuteJavaScriptForFeature(
    NSString* script,
    JavaScriptFeature* feature) {
  JavaScriptFeatureManager* feature_manager =
      JavaScriptFeatureManager::FromBrowserState(GetBrowserState());
  JavaScriptContentWorld* world =
      feature_manager->GetContentWorldForFeature(feature);
  // JS execution in particular content worlds is only available on iOS 14+.
  DCHECK(base::ios::IsRunningOnIOS14OrLater());

  if (@available(ios 14, *)) {
    WKWebView* web_view =
        [web::test::GetWebController(web_state()) ensureWebViewCreated];
    return web::test::ExecuteJavaScript(web_view, world->GetWKContentWorld(),
                                        script);
  }

  return nil;
}

id WebTestWithWebState::ExecuteJavaScript(NSString* script) {
  SCOPED_TRACE(base::SysNSStringToUTF8(script));
  return web::test::ExecuteJavaScript(script, web_state());
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

void WebTestWithWebState::WillProcessTask(const base::PendingTask&, bool) {
  // Nothing to do.
}

void WebTestWithWebState::DidProcessTask(const base::PendingTask&) {
  processed_a_task_ = true;
}

}  // namespace web
