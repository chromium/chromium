// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/url_checker_delegate_impl.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/ref_counted.h"
#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "components/safe_browsing/core/browser/db/database_manager.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_client.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_query_manager.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/http/http_request_headers.h"
#import "testing/platform_test.h"

using base::test::ios::kWaitForFileOperationTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using security_interstitials::UnsafeResource;

namespace {
// Struct used to test the execution of UnsafeResource callbacks.
struct UnsafeResourceCallbackState {
  bool executed = false;
  bool proceed = false;
  bool show_interstitial = false;
};
// Function used as the callback for UnsafeResources.
void PopulateCallbackState(UnsafeResourceCallbackState* state,
                           UnsafeResource::UrlCheckResult result) {
  state->executed = true;
  state->proceed = result.proceed;
  state->show_interstitial = result.showed_interstitial;
}
}  // namespace

// Test fixture for UrlCheckerDelegateImpl.
class UrlCheckerDelegateImplTest : public PlatformTest {
 public:
  UrlCheckerDelegateImplTest()
      : browser_state_(std::make_unique<web::FakeBrowserState>()),
        delegate_(
            base::MakeRefCounted<UrlCheckerDelegateImpl>(nullptr,
                                                         client_.AsWeakPtr())),
        item_(web::NavigationItem::Create()),
        web_state_(std::make_unique<web::FakeWebState>()) {
    // Set up the WebState.
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->SetLastCommittedItem(item_.get());
    web_state_->SetNavigationManager(std::move(navigation_manager));
    web_state_->SetBrowserState(browser_state_.get());
    // Construct the allow list and unsafe resource container.
    SafeBrowsingUnsafeResourceContainer::CreateForWebState(web_state_.get());
    SafeBrowsingUrlAllowList::CreateForWebState(web_state_.get());
    SafeBrowsingQueryManager::CreateForWebState(web_state_.get(), &client_);
  }
  ~UrlCheckerDelegateImplTest() override = default;

  // Creates an UnsafeResource whose callback populates `callback_state`.
  UnsafeResource CreateUnsafeResource(
      UnsafeResourceCallbackState* callback_state) {
    UnsafeResource resource;
    resource.url = GURL("http://www.chromium.test");
    resource.callback_sequence = task_environment_.GetMainThreadTaskRunner();
    resource.callback =
        base::BindRepeating(&PopulateCallbackState, callback_state);
    resource.weak_web_state = web_state_->GetWeakPtr();
    return resource;
  }

  // Waits for `state.executed` to be reset to true.  Returns whether the state
  // populated before a timeout.
  bool WaitForUnsafeResourceCallbackExecution(
      UnsafeResourceCallbackState* state) {
    task_environment_.RunUntilIdle();
    return WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^{
      return state->executed;
    });
  }

  // Getters for the allow list and unsafe resource container.
  SafeBrowsingUrlAllowList* allow_list() {
    return web_state_ ? SafeBrowsingUrlAllowList::FromWebState(web_state_.get())
                      : nullptr;
  }
  SafeBrowsingUnsafeResourceContainer* container() {
    return web_state_ ? SafeBrowsingUnsafeResourceContainer::FromWebState(
                            web_state_.get())
                      : nullptr;
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<web::FakeBrowserState> browser_state_;
  FakeSafeBrowsingClient client_;
  scoped_refptr<safe_browsing::UrlCheckerDelegate> delegate_;
  std::unique_ptr<web::NavigationItem> item_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Tests that the delegate does not allow unsafe resources to proceed and does
// not show an interstitial for UnsafeResources for destroyed WebStates.
TEST_F(UrlCheckerDelegateImplTest, DontProceedForDestroyedWebState) {
  // Construct the UnsafeResource.
  UnsafeResourceCallbackState callback_state;
  UnsafeResource resource = CreateUnsafeResource(&callback_state);

  // Destroy the WebState.
  web_state_ = nullptr;

  // Instruct the delegate to display the blocking page.
  delegate_->StartDisplayingBlockingPageHelper(resource, /*method=*/"",
                                               net::HttpRequestHeaders(),
                                               /*has_user_gesture=*/true);
  EXPECT_TRUE(WaitForUnsafeResourceCallbackExecution(&callback_state));

  // Verify the callback state.
  EXPECT_FALSE(callback_state.proceed);
  EXPECT_FALSE(callback_state.show_interstitial);
}

// Tests that the delegate does not allow unsafe resources to proceed and does
// not show an interstitial for UnsafeResources if it is blocked by the client.
TEST_F(UrlCheckerDelegateImplTest, DontProceedIfBlockedByClient) {
  // Construct the UnsafeResource.
  UnsafeResourceCallbackState callback_state;
  UnsafeResource resource = CreateUnsafeResource(&callback_state);

  // Make client block unsafe resource.
  client_.set_should_block_unsafe_resource(true);

  // Instruct the delegate to display the blocking page.
  delegate_->StartDisplayingBlockingPageHelper(resource, /*method=*/"",
                                               net::HttpRequestHeaders(),
                                               /*has_user_gesture=*/true);
  EXPECT_TRUE(WaitForUnsafeResourceCallbackExecution(&callback_state));

  // Verify the callback state.
  EXPECT_FALSE(callback_state.proceed);
  EXPECT_FALSE(callback_state.show_interstitial);
}

// Tests that the delegate allows unsafe resources to proceed without showing an
// interstitial for allows unsafe navigations.
TEST_F(UrlCheckerDelegateImplTest, ProceedForAllowedUnsafeNavigation) {
  // Construct the UnsafeResource.
  safe_browsing::SBThreatType threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING;
  UnsafeResourceCallbackState callback_state;
  UnsafeResource resource = CreateUnsafeResource(&callback_state);
  resource.threat_type = threat_type;

  // Add the resource to the allow list.
  allow_list()->AllowUnsafeNavigations(resource.url, threat_type);

  // Instruct the delegate to display the blocking page.
  delegate_->StartDisplayingBlockingPageHelper(resource, /*method=*/"",
                                               net::HttpRequestHeaders(),
                                               /*has_user_gesture=*/true);
  EXPECT_TRUE(WaitForUnsafeResourceCallbackExecution(&callback_state));

  // Verify the callback state.
  EXPECT_TRUE(callback_state.proceed);
  EXPECT_FALSE(callback_state.show_interstitial);
}
