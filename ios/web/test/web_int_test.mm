// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/web_int_test.h"

#import "base/ios/block_types.h"
#import "base/memory/ptr_util.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_run_loop_timeout.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/common/web_view_creation_util.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state_observer.h"

#if DCHECK_IS_ON()
#import "ui/display/screen_base.h"
#endif

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForClearBrowsingDataTimeout;
using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

#pragma mark - IntTestWebStateObserver

// WebStateObserver class that is used to track when page loads finish.
class IntTestWebStateObserver : public WebStateObserver {
 public:
  // Instructs the observer to listen for page loads for `url`.
  explicit IntTestWebStateObserver(const GURL& url) : expected_url_(url) {}

  IntTestWebStateObserver(const IntTestWebStateObserver&) = delete;
  IntTestWebStateObserver& operator=(const IntTestWebStateObserver&) = delete;

  // Whether `expected_url_` has been loaded successfully.
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
};

#pragma mark - WebIntTest

WebIntTest::WebIntTest() {}
WebIntTest::~WebIntTest() {}

void WebIntTest::SetUp() {
  WebTest::SetUp();

  // Remove any previously existing WKWebView data.
  RemoveWKWebViewCreatedData([WKWebsiteDataStore defaultDataStore],
                             [WKWebsiteDataStore allWebsiteDataTypes]);

  // Create the WebState.
  web::WebState::CreateParams web_state_create_params(GetBrowserState());
  web_state_ = web::WebState::Create(web_state_create_params);

  // Resize the webview so that pages can be properly rendered.
  web_state()->GetView().frame = GetAnyKeyWindow().bounds;

  web_state()->SetDelegate(&web_state_delegate_);
  web_state()->SetKeepRenderProcessAlive(true);
}

void WebIntTest::TearDown() {
  RemoveWKWebViewCreatedData([WKWebsiteDataStore defaultDataStore],
                             [WKWebsiteDataStore allWebsiteDataTypes]);

  WebTest::TearDown();

#if DCHECK_IS_ON()
  // The same screen object is shared across multiple test runs on IOS build.
  // Make sure that all display observers are removed at the end of each
  // test.
  display::ScreenBase* screen =
      static_cast<display::ScreenBase*>(display::Screen::GetScreen());
  DCHECK(!screen->HasDisplayObservers());
#endif
}

bool WebIntTest::ExecuteBlockAndWaitForLoad(const GURL& url,
                                            ProceduralBlock block) {
  DCHECK(block);

  IntTestWebStateObserver observer(url);
  base::ScopedObservation<WebState, WebStateObserver> scoped_observer(
      &observer);
  scoped_observer.Observe(web_state());

  block();

  // Need to use a pointer to `observer` as the block wants to capture it by
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
  base::RunLoop run_loop;
  __block base::OnceClosure quit_closure = run_loop.QuitClosure();

  ProceduralBlock remove_data = ^{
    [data_store removeDataOfTypes:websiteDataTypes
                    modifiedSince:NSDate.distantPast
                completionHandler:^{
                  std::move(quit_closure).Run();
                }];
  };

  if ([websiteDataTypes containsObject:WKWebsiteDataTypeCookies]) {
    // TODO(crbug.com/554225): This approach of creating a WKWebView and
    // executing JS to clear cookies is a workaround for
    // https://bugs.webkit.org/show_bug.cgi?id=149078.
    // Remove this, when that bug is fixed. The `marker_web_view` will be
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

  // Wait until the data is removed. We increase the timeout to 90 seconds here
  // since this action has been timing out frequently on the bots.
  {
    base::test::ScopedRunLoopTimeout data_removal_timeout(FROM_HERE,
                                                          base::Seconds(90));
    run_loop.Run();
  }
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
