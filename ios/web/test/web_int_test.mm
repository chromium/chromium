// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/web_int_test.h"

#include "base/base_paths.h"
#import "base/ios/block_types.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/scoped_observer.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/common/web_view_creation_util.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/js_test_util.h"
#include "ios/web/public/web_state_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

#pragma mark - IntTestWebStateObserver

// WebStateObserver class that is used to track when page loads finish.
class IntTestWebStateObserver : public WebStateObserver {
 public:
  // Instructs the observer to listen for page loads for |url|.
  explicit IntTestWebStateObserver(const GURL& url) : expected_url_(url) {}

  // Whether |expected_url_| has been loaded successfully.
  bool IsExpectedPageLoaded() { return page_loaded_; }

  // WebStateObserver methods:
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override {
    page_loaded_ = true;
  }

  void WebStateDestroyed(web::WebState* web_state) override { NOTREACHED(); }

 private:
  GURL expected_url_;
  bool page_loaded_ = false;

  DISALLOW_COPY_AND_ASSIGN(IntTestWebStateObserver);
};

#pragma mark - WebIntTest

WebIntTest::WebIntTest() {}
WebIntTest::~WebIntTest() {}

void WebIntTest::SetUp() {
  WebTest::SetUp();

  // Start the http server.
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  ASSERT_FALSE(server.IsRunning());

  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
  server.StartOrDie(test_data_dir.Append("."));

  // Remove any previously existing WKWebView data.
  RemoveWKWebViewCreatedData([WKWebsiteDataStore defaultDataStore],
                             [WKWebsiteDataStore allWebsiteDataTypes]);

  // Create the WebState.
  web::WebState::CreateParams web_state_create_params(GetBrowserState());
  web_state_ = web::WebState::Create(web_state_create_params);

  // Resize the webview so that pages can be properly rendered.
  web_state()->GetView().frame =
      [UIApplication sharedApplication].keyWindow.bounds;

  web_state()->SetDelegate(&web_state_delegate_);
  web_state()->SetKeepRenderProcessAlive(true);
}

void WebIntTest::TearDown() {
  RemoveWKWebViewCreatedData([WKWebsiteDataStore defaultDataStore],
                             [WKWebsiteDataStore allWebsiteDataTypes]);

  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  server.Stop();
  EXPECT_FALSE(server.IsRunning());

  WebTest::TearDown();
}

id WebIntTest::ExecuteJavaScript(NSString* script) {
  return web::test::ExecuteJavaScript(web_state()->GetJSInjectionReceiver(),
                                      script);
}

bool WebIntTest::ExecuteBlockAndWaitForLoad(const GURL& url,
                                            ProceduralBlock block) {
  DCHECK(block);

  IntTestWebStateObserver observer(url);
  ScopedObserver<WebState, WebStateObserver> scoped_observer(&observer);
  scoped_observer.Add(web_state());

  block();

  // Need to use a pointer to |observer| as the block wants to capture it by
  // value (even if marked with __block) which would not work.
  IntTestWebStateObserver* observer_ptr = &observer;
  return WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return observer_ptr->IsExpectedPageLoaded();
  });
}

bool WebIntTest::LoadUrl(const GURL& url) {
  web::NavigationManager::WebLoadParams params(url);
  params.transition_type = ui::PageTransition::PAGE_TRANSITION_TYPED;
  return LoadWithParams(params);
}

bool WebIntTest::LoadWithParams(
    const NavigationManager::WebLoadParams& params) {
  NavigationManager::WebLoadParams block_params(params);
  return ExecuteBlockAndWaitForLoad(params.url, ^{
    navigation_manager()->LoadURLWithParams(block_params);
  });
}

void WebIntTest::RemoveWKWebViewCreatedData(WKWebsiteDataStore* data_store,
                                            NSSet* websiteDataTypes) {
  __block bool data_removed = false;

  ProceduralBlock remove_data = ^{
    [data_store removeDataOfTypes:websiteDataTypes
                    modifiedSince:[NSDate distantPast]
                completionHandler:^{
                  data_removed = true;
                }];
  };

  if ([websiteDataTypes containsObject:WKWebsiteDataTypeCookies]) {
    // TODO(crbug.com/554225): This approach of creating a WKWebView and
    // executing JS to clear cookies is a workaround for
    // https://bugs.webkit.org/show_bug.cgi?id=149078.
    // Remove this, when that bug is fixed. The |marker_web_view| will be
    // released when cookies have been cleared.
    WKWebView* marker_web_view =
        web::BuildWKWebView(CGRectZero, GetBrowserState());
    [marker_web_view evaluateJavaScript:@""
                      completionHandler:^(id, NSError*) {
                        [marker_web_view self];
                        remove_data();
                      }];
  } else {
    remove_data();
  }

  base::test::ios::WaitUntilCondition(^bool {
    return data_removed;
  });
}

NSInteger WebIntTest::GetIndexOfNavigationItem(
    const web::NavigationItem* item) {
  for (NSInteger i = 0; i < navigation_manager()->GetItemCount(); ++i) {
    if (navigation_manager()->GetItemAtIndex(i) == item)
      return i;
  }
  return NSNotFound;
}

}  // namespace web
