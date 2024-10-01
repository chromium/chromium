// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/browser_container_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_presentation_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_consumer.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForUIElementTimeout;

// BrowserContainerConsumer for use in tests.
@interface FakeBrowserContainerConsumer : NSObject <BrowserContainerConsumer>
@property(nonatomic, strong) UIView* contentView;
@property(nonatomic, strong) UIViewController* contentViewController;
@property(nonatomic, strong)
    UIViewController* webContentsOverlayContainerViewController;
@property(nonatomic, strong) UIViewController* screenTimeViewController;
@property(nonatomic, assign, getter=isContentBlocked) BOOL contentBlocked;
@end

@implementation FakeBrowserContainerConsumer
@end

// Test fixture for BrowserContainerMediator.
class BrowserContainerMediatorTest : public PlatformTest {
 public:
  BrowserContainerMediatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    overlay_presenter_ = OverlayPresenter::FromBrowser(
        browser_.get(), OverlayModality::kWebContentArea);
    mediator_ = [[BrowserContainerMediator alloc]
                  initWithWebStateList:browser_->GetWebStateList()
        webContentAreaOverlayPresenter:overlay_presenter_];
    consumer_ = [[FakeBrowserContainerConsumer alloc] init];
    mediator_.consumer = consumer_;
    overlay_presenter_->SetPresentationContext(&presentation_context_);
  }
  ~BrowserContainerMediatorTest() override {
    overlay_presenter_->SetPresentationContext(nullptr);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  FakeOverlayPresentationContext presentation_context_;
  raw_ptr<OverlayPresenter> overlay_presenter_ = nullptr;
  BrowserContainerMediator* mediator_ = nil;
  FakeBrowserContainerConsumer* consumer_ = nil;
};

// Tests that the content area is blocked when an HTTP authentication dialog is
// shown for a page whose host does not match the last committed URL.
TEST_F(BrowserContainerMediatorTest, BlockContentForHTTPAuthDialog) {
  ASSERT_FALSE(consumer_.contentBlocked);

  // Add and activate a WebState with kWebStateUrl.
  const GURL kWebStateUrl("http://www.committed.test");
  auto passed_web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state = passed_web_state.get();
  web_state->SetCurrentURL(kWebStateUrl);
  browser_->GetWebStateList()->InsertWebState(
      std::move(passed_web_state),
      WebStateList::InsertionParams::Automatic().Activate());

  // Show an HTTP authentication dialog from a different URL.
  const GURL kHttpAuthUrl("http://www.http_auth.test");
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state, OverlayModality::kWebContentArea);
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<HTTPAuthOverlayRequestConfig>(
          kHttpAuthUrl, /*message=*/"", /*default_username=*/"");
  queue->AddRequest(std::move(request));

  // Verify that the content is blocked since kHttpAuthUrl is a different host
  // than kWebStateUrl.
  EXPECT_TRUE(consumer_.contentBlocked);

  // Cancel the request and verify that the content is no longer blocked.
  queue->CancelAllRequests();
  EXPECT_FALSE(consumer_.contentBlocked);
}
