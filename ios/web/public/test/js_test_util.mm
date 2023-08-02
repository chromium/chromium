// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/js_test_util.h"

#import <WebKit/WebKit.h>

#import "base/apple/bundle_locations.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/js_messaging/page_script_util.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/test/js_test_util_internal.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/web_state_impl.h"
#import "testing/gtest/include/gtest/gtest.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {
namespace test {

id ExecuteJavaScript(WKWebView* web_view, NSString* script) {
  return ExecuteJavaScript(web_view, script, /*error=*/nil);
}

id ExecuteJavaScript(WKWebView* web_view,
                     NSString* script,
                     NSError* __autoreleasing* error) {
  __block id result;
  __block bool completed = false;
  __block NSError* block_error = nil;
  SCOPED_TRACE(base::SysNSStringToUTF8(script));
  [web_view evaluateJavaScript:script
             completionHandler:^(id script_result, NSError* script_error) {
               result = [script_result copy];
               block_error = [script_error copy];
               completed = true;
             }];
  BOOL success = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return completed;
  });
  // Log stack trace to provide some context.
  EXPECT_TRUE(success)
      << base::SysNSStringToUTF8(block_error.description)
      << "\nWKWebView failed to complete javascript execution.\n"
      << base::SysNSStringToUTF8(
             [[NSThread callStackSymbols] componentsJoinedByString:@"\n"]);
  if (error) {
    *error = block_error;
  }
  return result;
}

id ExecuteJavaScriptForFeature(web::WebState* web_state,
                               NSString* script,
                               JavaScriptFeature* feature) {
  JavaScriptFeatureManager* feature_manager =
      JavaScriptFeatureManager::FromBrowserState(web_state->GetBrowserState());
  JavaScriptContentWorld* world =
      feature_manager->GetContentWorldForFeature(feature);

  WKWebView* web_view =
      [web::WebStateImpl::FromWebState(web_state)->GetWebController()
          ensureWebViewCreated];
  return web::test::ExecuteJavaScript(web_view, world->GetWKContentWorld(),
                                      script);
}

bool LoadHtml(WKWebView* web_view, NSString* html, NSURL* base_url) {
  [web_view loadHTMLString:html baseURL:base_url];

  return WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return !web_view.loading;
  });
}

bool WaitForInjectedScripts(WKWebView* web_view) {
  return WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !![ExecuteJavaScript(web_view, @"!!__gCrWeb") isEqual:@YES];
  });
}

NSString* GetPageScript(NSString* script_file_name) {
  return web::GetPageScript(script_file_name);
}

NSString* GetSharedScripts() {
  // Scripts must be all injected at once because as soon as __gCrWeb exists,
  // injection is assumed to be done and __gCrWeb.message is used.
  return [NSString stringWithFormat:@"%@; %@; %@", GetPageScript(@"gcrweb"),
                                    GetPageScript(@"common"),
                                    GetPageScript(@"message")];
}

void OverrideJavaScriptFeatures(web::BrowserState* browser_state,
                                std::vector<JavaScriptFeature*> features) {
  WKWebViewConfigurationProvider& configuration_provider =
      WKWebViewConfigurationProvider::FromBrowserState(browser_state);
  WKWebViewConfiguration* configuration =
      configuration_provider.GetWebViewConfiguration();
  // User scripts must be removed because
  // `JavaScriptFeatureManager::ConfigureFeatures` will remove script message
  // handlers.
  [configuration.userContentController removeAllUserScripts];

  JavaScriptFeatureManager::FromBrowserState(browser_state)
      ->ConfigureFeatures(features);
}

}  // namespace test
}  // namespace web

