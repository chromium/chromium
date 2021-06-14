// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/session_state/web_session_state_tab_helper.h"

#import <WebKit/WebKit.h>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#import "base/logging.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/web/features.h"
#import "ios/chrome/browser/web/session_state/web_session_state_cache.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#include "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

class WebSessionStateTabHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();

    web::WebState::CreateParams createParams(chrome_browser_state_.get());
    web_state_ = web::WebState::Create(createParams);
    TabIdTabHelper::CreateForWebState(web_state_.get());
    WebSessionStateTabHelper::CreateForWebState(web_state_.get());

    session_cache_directory_ = chrome_browser_state_->GetStatePath().Append(
        kWebSessionCacheDirectoryName);
  }

  // Flushes all the runloops internally used by the cache.
  void FlushRunLoops() {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<web::WebState> web_state_;
  base::FilePath session_cache_directory_;
};

// Tests that APIs do nothing when the feature is disabled.
TEST_F(WebSessionStateTabHelperTest, DisableFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(web::kRestoreSessionFromCache);

  WebSessionStateTabHelper* helper =
      WebSessionStateTabHelper::FromWebState(web_state_.get());
  ASSERT_FALSE(helper->IsEnabled());

  // Nothing should be saved or restored when the feature is disabled.
  ASSERT_FALSE(helper->RestoreSessionFromCache());

  web_state_->GetView();
  web_state_->SetKeepRenderProcessAlive(true);
  GURL url(kChromeUIAboutNewTabURL);
  web::NavigationManager::WebLoadParams params(url);
  web_state_->GetNavigationManager()->LoadURLWithParams(params);
  DCHECK(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return !web_state_->IsLoading();
  }));
  helper->SaveSessionState();
  FlushRunLoops();

  // File should not be saved.
  NSString* sessionID =
      TabIdTabHelper::FromWebState(web_state_.get())->tab_id();
  base::FilePath filePath =
      session_cache_directory_.Append(base::SysNSStringToUTF8(sessionID));
  ASSERT_FALSE(base::PathExists(filePath));
}

// Tests session state serialize and deserialize APIs.
TEST_F(WebSessionStateTabHelperTest, SessionStateRestore) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(web::kRestoreSessionFromCache);

  // Make sure the internal WKWebView is live.
  web_state_->GetView();
  web_state_->SetKeepRenderProcessAlive(true);
  GURL url(kChromeUIAboutNewTabURL);
  web::NavigationManager::WebLoadParams params(url);
  web_state_->GetNavigationManager()->LoadURLWithParams(params);
  DCHECK(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return !web_state_->IsLoading();
  }));
  // As well as waiting for the page to finish loading, it seems an extra wait
  // is required for some older devices.  If SaveSessionState is called too
  // early, WebKit returns a non-nil data object that it won't restore properly.
  // This is OK, since it will fall back to  legacy session restore when
  // necessary.
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSeconds(2));

  // File should not be saved yet.
  NSString* sessionID =
      TabIdTabHelper::FromWebState(web_state_.get())->tab_id();
  base::FilePath filePath =
      session_cache_directory_.Append(base::SysNSStringToUTF8(sessionID));
  ASSERT_FALSE(base::PathExists(filePath));

  WebSessionStateTabHelper* helper =
      WebSessionStateTabHelper::FromWebState(web_state_.get());
  helper->SaveSessionStateIfStale();
  FlushRunLoops();
  if (@available(iOS 15, *)) {
    ASSERT_TRUE(
        WebSessionStateTabHelper::FromWebState(web_state_.get())->IsEnabled());
    ASSERT_TRUE(base::PathExists(filePath));
  } else {
    // On iOS 14, the feature is disabled.
    EXPECT_FALSE(base::PathExists(filePath));
    return;
  }

  // Create a new webState with a live WKWebView.
  web::WebState::CreateParams createParams(chrome_browser_state_.get());
  std::unique_ptr<web::WebState> web_state =
      web::WebState::Create(createParams);
  WebSessionStateTabHelper::CreateForWebState(web_state.get());
  TabIdTabHelper::CreateForWebState(web_state.get());
  web_state->GetView();
  web_state->SetKeepRenderProcessAlive(true);
  GURL urlBlank("about:blank");
  web::NavigationManager::WebLoadParams paramsBlank(urlBlank);
  web_state->GetNavigationManager()->LoadURLWithParams(paramsBlank);

  // copy the tabid file over to the new tabid...
  NSString* newSessionID =
      TabIdTabHelper::FromWebState(web_state.get())->tab_id();
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
