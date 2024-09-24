// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/web_session_state_tab_helper.h"

#import <WebKit/WebKit.h>

#import "base/base_paths.h"
#import "base/files/file_util.h"
#import "base/logging.h"
#import "base/path_service.h"
#import "base/strings/stringprintf.h"
#import "base/task/thread_pool/thread_pool_instance.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/sessions/model/session_constants.h"
#import "ios/chrome/browser/sessions/model/web_session_state_cache.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

class WebSessionStateTabHelperTest : public PlatformTest {
 public:
  WebSessionStateTabHelperTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {}

  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);

    WebSessionStateTabHelper::CreateForWebState(web_state());

    session_cache_directory_ =
        profile_.get()->GetStatePath().Append(kLegacyWebSessionsDirname);
  }

  base::FilePath SessionCachePathForWebState(const web::WebState* web_state) {
    const web::WebStateID session_id = web_state->GetUniqueIdentifier();
    return session_cache_directory_.AppendASCII(base::StringPrintf(
        "%08u", static_cast<uint32_t>(session_id.identifier())));
  }

  // Flushes all the runloops internally used by the cache.
  void FlushRunLoops() {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  web::WebState* web_state() { return web_state_.get(); }

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
  base::FilePath session_cache_directory_;
};

// Tests session state serialize and deserialize APIs.
TEST_F(WebSessionStateTabHelperTest, SessionStateRestore) {
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
  const base::FilePath file_path = SessionCachePathForWebState(web_state());
  ASSERT_FALSE(base::PathExists(file_path));

  WebSessionStateTabHelper* helper =
      WebSessionStateTabHelper::FromWebState(web_state());
  helper->SaveSessionStateIfStale();
  FlushRunLoops();
  ASSERT_TRUE(base::PathExists(file_path));

  // Create a new webState with a live WKWebView.
  const web::WebState::CreateParams create_params(profile_.get());
  std::unique_ptr<web::WebState> new_web_state =
      web::WebState::Create(create_params);
  WebSessionStateTabHelper::CreateForWebState(new_web_state.get());
  new_web_state->GetView();
  new_web_state->SetKeepRenderProcessAlive(true);
  const GURL url_blank("about:blank");
  const web::NavigationManager::WebLoadParams params_blank(url_blank);
  new_web_state->GetNavigationManager()->LoadURLWithParams(params_blank);

  // copy the tabid file over to the new tabid...
  base::FilePath new_file_path =
      SessionCachePathForWebState(new_web_state.get());
  EXPECT_TRUE(base::CopyFile(file_path, new_file_path));

  // Only restore for session restore URLs.
  NSData* data = WebSessionStateTabHelper::FromWebState(new_web_state.get())
                     ->FetchSessionFromCache();
  ASSERT_TRUE(data);
  EXPECT_TRUE(new_web_state->SetSessionStateData(data));

  // kChromeUIAboutNewTabURL should get rewritten to kChromeUINewTabURL.
  ASSERT_EQ(new_web_state->GetLastCommittedURL(), GURL(kChromeUINewTabURL));
}

}  // namespace
