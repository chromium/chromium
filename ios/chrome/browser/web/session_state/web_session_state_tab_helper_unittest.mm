// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/session_state/web_session_state_tab_helper.h"

#import <WebKit/WebKit.h>

#import "base/base_paths.h"
#import "base/files/file_util.h"
#import "base/logging.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool/thread_pool_instance.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/chrome/browser/web/features.h"
#import "ios/chrome/browser/web/session_state/web_session_state_cache.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

class WebSessionStateTabHelperTest : public PlatformTest {
 public:
  WebSessionStateTabHelperTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {}

  void SetUp() override {
    PlatformTest::SetUp();
    browser_state_ = TestChromeBrowserState::Builder().Build();

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);

    WebSessionStateTabHelper::CreateForWebState(web_state());

    session_cache_directory_ = browser_state_.get()->GetStatePath().Append(
        kWebSessionCacheDirectoryName);
  }

  // Flushes all the runloops internally used by the cache.
  void FlushRunLoops() {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  web::WebState* web_state() { return web_state_.get(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
  base::FilePath session_cache_directory_;
};

// Tests that APIs do nothing when the feature is disabled.
TEST_F(WebSessionStateTabHelperTest, DisableFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(web::kRestoreSessionFromCache);

  WebSessionStateTabHelper* helper =
      WebSessionStateTabHelper::FromWebState(web_state());
  ASSERT_FALSE(helper->IsEnabled());

  // Nothing should be saved or restored when the feature is disabled.
  ASSERT_FALSE(helper->RestoreSessionFromCache());

  web_state()->GetView();
  web_state()->SetKeepRenderProcessAlive(true);
  GURL url(kChromeUIAboutNewTabURL);
  web::NavigationManager::WebLoadParams params(url);
  web_state()->GetNavigationManager()->LoadURLWithParams(params);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return !web_state()->IsLoading();
  }));
  helper->SaveSessionState();
  FlushRunLoops();

  // File should not be saved.
  NSString* sessionID = web_state()->GetStableIdentifier();
  base::FilePath filePath =
      session_cache_directory_.Append(base::SysNSStringToUTF8(sessionID));
  ASSERT_FALSE(base::PathExists(filePath));
}

// Tests session state serialize and deserialize APIs.
TEST_F(WebSessionStateTabHelperTest, SessionStateRestore) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(web::kRestoreSessionFromCache);

  // Make sure the internal WKWebView is live.
  web_state()->GetView();
  web_state()->SetKeepRenderProcessAlive(true);
  GURL url(kChromeUIAboutNewTabURL);
  web::NavigationManager::WebLoadParams params(url);
  web_state()->GetNavigationManager()->LoadURLWithParams(params);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return !web_state()->IsLoading();
  }));
  // As well as waiting for the page to finish loading, it seems an extra wait
  // is required for some older devices.  If SaveSessionState is called too
  // early, WebKit returns a non-nil data object that it won't restore properly.
  // This is OK, since it will fall back to  legacy session restore when
  // necessary.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(2));

  // File should not be saved yet.
  NSString* sessionID = web_state()->GetStableIdentifier();
  base::FilePath filePath =
      session_cache_directory_.Append(base::SysNSStringToUTF8(sessionID));
  ASSERT_FALSE(base::PathExists(filePath));

  WebSessionStateTabHelper* helper =
      WebSessionStateTabHelper::FromWebState(web_state());
  helper->SaveSessionStateIfStale();
  FlushRunLoops();
  if (@available(iOS 15, *)) {
    ASSERT_TRUE(
        WebSessionStateTabHelper::FromWebState(web_state())->IsEnabled());
    ASSERT_TRUE(base::PathExists(filePath));
  } else {
    // On iOS 14, the feature is disabled.
    EXPECT_FALSE(base::PathExists(filePath));
    return;
  }

  // Create a new webState with a live WKWebView.
  web::WebState::CreateParams createParams(browser_state_.get());
  std::unique_ptr<web::WebState> web_state =
      web::WebState::Create(createParams);
  WebSessionStateTabHelper::CreateForWebState(web_state.get());
  web_state->GetView();
  web_state->SetKeepRenderProcessAlive(true);
  GURL urlBlank("about:blank");
  web::NavigationManager::WebLoadParams paramsBlank(urlBlank);
  web_state->GetNavigationManager()->LoadURLWithParams(paramsBlank);

  // copy the tabid file over to the new tabid...
  NSString* newSessionID = web_state.get()->GetStableIdentifier();
  base::FilePath newFilePath =
      session_cache_directory_.Append(base::SysNSStringToUTF8(newSessionID));
  EXPECT_TRUE(base::CopyFile(filePath, newFilePath));

  // Only restore for session restore URLs.
  ASSERT_TRUE(WebSessionStateTabHelper::FromWebState(web_state.get())
                  ->RestoreSessionFromCache());

  // kChromeUIAboutNewTabURL should get rewritten to kChromeUINewTabURL.
  ASSERT_EQ(web_state->GetLastCommittedURL(), GURL(kChromeUINewTabURL));
}

}  // namespace
