// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_state_delegate_browser_agent.h"

#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_alert_dialog_overlay.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/web/model/blocked_popup_tab_helper.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/ui/java_script_dialog_presenter.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

const char kURL1[] = "https://www.some.url.com";
const char kURL2[] = "https://www.some.url2.com";

// Test fixture for WebStateDelegateTabHelper.
class WebStateDelegateBrowserAgentTest : public PlatformTest {
 public:
  WebStateDelegateBrowserAgentTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    TabInsertionBrowserAgent::CreateForBrowser(browser_.get());
    WebStateDelegateBrowserAgent::CreateForBrowser(
        browser_.get(), TabInsertionBrowserAgent::FromBrowser(browser_.get()));
  }
  ~WebStateDelegateBrowserAgentTest() override = default;

  web::WebStateDelegate* delegate() {
    return WebStateDelegateBrowserAgent::FromBrowser(browser_.get());
  }

  web::WebState* InsertNewWebState(const GURL& url) {
    web::NavigationManager::WebLoadParams load_params(url);
    load_params.transition_type = ui::PAGE_TRANSITION_TYPED;

    web::WebState::CreateParams create_params(browser_->GetProfile());
    create_params.created_with_opener = false;

    std::unique_ptr<web::WebState> web_state =
        web::WebState::Create(create_params);
    OverlayRequestQueue::CreateForWebState(web_state.get());
    BlockedPopupTabHelper::GetOrCreateForWebState(web_state.get());
    SnapshotTabHelper::CreateForWebState(web_state.get());
    web_state->GetNavigationManager()->LoadURLWithParams(load_params);

    WebStateList* web_state_list = browser_->GetWebStateList();
    web_state_list->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    return web_state_list->GetActiveWebState();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

// Test that CreateNewWebState() creates a new web state in the correct place.
TEST_F(WebStateDelegateBrowserAgentTest, CreateNewWebState) {
  web::WebState* web_state = InsertNewWebState(GURL(kURL1));
  web::WebState* web_state2 =
      delegate()->CreateNewWebState(web_state, GURL(kURL2), GURL(kURL1), true);
  EXPECT_NE(web_state2, nullptr);

  // Check that it was inserted correctly
  EXPECT_EQ(browser_->GetWebStateList()->GetIndexOfWebState(web_state2),
            browser_->GetWebStateList()->GetIndexOfWebState(web_state) + 1);
}

// Test that CreateNewWebState() doesn't create a popup if popups aren't
// enabled.
TEST_F(WebStateDelegateBrowserAgentTest, CreateNewWebStateAndPopup) {
  web::WebState* web_state = InsertNewWebState(GURL(kURL1));

  // Verify that this webstate's popups are blocked
  BlockedPopupTabHelper* popup_helper =
      BlockedPopupTabHelper::GetOrCreateForWebState(web_state);
  EXPECT_TRUE(popup_helper->ShouldBlockPopup(GURL(kURL1)));
  // Create a new webstate without user initiation.
  web::WebState* web_state2 =
      delegate()->CreateNewWebState(web_state, GURL(kURL2), GURL(kURL1), false);
  // Expect that the webstate isn't created, returned, or inserted into the\
  // web state list.
  EXPECT_EQ(web_state2, nullptr);
  EXPECT_EQ(browser_->GetWebStateList()->count(), 1);
}

// Test that CloseWebState() removed the web state from the web state list.
TEST_F(WebStateDelegateBrowserAgentTest, CloseWebState) {
  web::WebState* web_state = InsertNewWebState(GURL(kURL1));
  delegate()->CloseWebState(web_state);
  EXPECT_EQ(browser_->GetWebStateList()->count(), 0);
}

// Test that the new tab options for OpenURLFromWebState() creates new web
// states and activates them appropriately.
TEST_F(WebStateDelegateBrowserAgentTest, OpenURLNewTabs) {
  web::WebState* web_state = InsertNewWebState(GURL(kURL1));
  web::WebState::OpenURLParams fg_open_params(
      GURL(kURL2), web::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false);
  web::WebState* web_state2 =
      delegate()->OpenURLFromWebState(web_state, fg_open_params);
  EXPECT_EQ(browser_->GetWebStateList()->count(), 2);
  // NEW_FOREGROUND_TAB should activate the newly added webstate.
  EXPECT_EQ(browser_->GetWebStateList()->GetActiveWebState(), web_state2);

  web::WebState::OpenURLParams bg_open_params(
      GURL(kURL2), web::Referrer(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false);
  delegate()->OpenURLFromWebState(web_state, bg_open_params);
  EXPECT_EQ(browser_->GetWebStateList()->count(), 3);
  // NEW_BACKGROUND_TAB should *not* activate the newly added webstate, so
  // web_state2 should still be active.
  EXPECT_EQ(browser_->GetWebStateList()->GetActiveWebState(), web_state2);
}

// Tests that OpenURLFromWebState() doesn't create a new tab with the
// CURRENT_TAB option.
TEST_F(WebStateDelegateBrowserAgentTest, OpenURLCurrentTab) {
  InsertNewWebState(GURL(kURL1));
  web::WebState::OpenURLParams open_params(GURL(kURL2), web::Referrer(),
                                           WindowOpenDisposition::CURRENT_TAB,
                                           ui::PAGE_TRANSITION_LINK, false);
  EXPECT_EQ(browser_->GetWebStateList()->count(), 1);
}

// Tests that OnAuthRequired() adds an HTTP authentication overlay request to
// the WebState's OverlayRequestQueue at OverlayModality::kWebContentArea.
TEST_F(WebStateDelegateBrowserAgentTest, OnAuthRequired) {
  NSURLProtectionSpace* protection_space =
      [[NSURLProtectionSpace alloc] initWithProxyHost:@"http://chromium.test"
                                                 port:0
                                                 type:nil
                                                realm:nil
                                 authenticationMethod:nil];
  NSURLCredential* credential =
      [[NSURLCredential alloc] initWithUser:@""
                                   password:@""
                                persistence:NSURLCredentialPersistenceNone];
  web::WebStateDelegate::AuthCallback callback =
      base::BindOnce(^(NSString* user, NSString* password){
      });
  web::WebState* web_state = InsertNewWebState(GURL(kURL1));
  delegate()->OnAuthRequired(web_state, protection_space, credential,
                             std::move(callback));

  // Verify that an HTTP auth overlay request has been created for the WebState.
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state, OverlayModality::kWebContentArea);
  ASSERT_TRUE(queue);
  OverlayRequest* request = queue->front_request();
  EXPECT_TRUE(request);
  EXPECT_TRUE(request->GetConfig<HTTPAuthOverlayRequestConfig>());
}

// Tests that GetJavaScriptDialogPresenter() returns an overlay-based JavaScript
// dialog presenter.
TEST_F(WebStateDelegateBrowserAgentTest, GetJavaScriptDialogPresenter) {
  // Verify that the delegate returns a non-null presenter.
  web::WebState* web_state = InsertNewWebState(GURL(kURL1));
  web::JavaScriptDialogPresenter* presenter =
      delegate()->GetJavaScriptDialogPresenter(web_state);
  EXPECT_TRUE(presenter);

  // Present a JavaScript alert.
  GURL kOriginUrl("http://chromium.test");
  presenter->RunJavaScriptAlertDialog(web_state, kOriginUrl, @"",
                                      base::DoNothing());

  // Verify that JavaScript alert OverlayRequest has been added to the
  // WebState's queue.
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state, OverlayModality::kWebContentArea);
  ASSERT_TRUE(queue);
  OverlayRequest* request = queue->front_request();
  EXPECT_TRUE(request);
  EXPECT_TRUE(request->GetConfig<JavaScriptAlertDialogRequest>());
}
