// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_mediator.h"

#include "components/omnibox/browser/test_location_bar_model.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_alert_overlay.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_presentation_context.h"
#import "ios/chrome/browser/ui/location_bar/test/fake_location_bar_consumer.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for LocationBarMediator.
class LocationBarMediatorTest : public PlatformTest {
 protected:
  LocationBarMediatorTest()
      : web_state_list_(&web_state_list_delegate_),
        mediator_(
            [[LocationBarMediator alloc] initWithLocationBarModel:&model_]),
        consumer_([[FakeLocationBarConsumer alloc] init]) {
    // Set up the TestBrowser.
    TestChromeBrowserState::Builder browser_state_builder;
    browser_state_ = browser_state_builder.Build();
    browser_ =
        std::make_unique<TestBrowser>(browser_state_.get(), &web_state_list_);
    // Set up the OverlayPresenter.
    OverlayPresenter* overlay_presenter = OverlayPresenter::FromBrowser(
        browser_.get(), OverlayModality::kWebContentArea);
    overlay_presenter->SetPresentationContext(&presentation_context_);
    // Set up the mediator.
    mediator_.webStateList = &web_state_list_;
    mediator_.webContentAreaOverlayPresenter = overlay_presenter;
    mediator_.consumer = consumer_;
  }
  ~LocationBarMediatorTest() override { [mediator_ disconnect]; }

  FakeOverlayPresentationContext presentation_context_;
  web::WebTaskEnvironment task_environment_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  TestLocationBarModel model_;
  LocationBarMediator* mediator_;
  FakeLocationBarConsumer* consumer_;
};

// Tests that the share button is disabled while overlays are presented
// over the web content area.
TEST_F(LocationBarMediatorTest, DisableShareForOverlays) {
  const GURL kUrl("https://chromium.test");
  std::unique_ptr<web::TestWebState> passed_web_state =
      std::make_unique<web::TestWebState>();
  web::TestWebState* web_state = passed_web_state.get();
  web_state->SetCurrentURL(kUrl);
  web_state_list_.InsertWebState(0, std::move(passed_web_state),
                                 WebStateList::INSERT_ACTIVATE,
                                 WebStateOpener(nullptr));
  ASSERT_TRUE(consumer_.locationShareable);

  // Present a JavaScript alert over the WebState and verify that the page is no
  // longer shareable.
  JavaScriptDialogSource source(web_state, kUrl, /* is_main_frame= */ true);
  const std::string kMessage("message");
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state, OverlayModality::kWebContentArea);
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<JavaScriptAlertOverlayRequestConfig>(
          source, kMessage));
  EXPECT_FALSE(consumer_.locationShareable);

  // Cancel the request and verify that the location is shareable again.
  queue->CancelAllRequests();
  EXPECT_TRUE(consumer_.locationShareable);
}

// Tests that the location text and page icon are updated when an HTTP auth
// dialog is displayed.
TEST_F(LocationBarMediatorTest, HTTPAuthDialog) {
  const GURL kUrl("https://chromium.test");
  std::unique_ptr<web::TestWebState> passed_web_state =
      std::make_unique<web::TestWebState>();
  web::TestWebState* web_state = passed_web_state.get();
  web_state->SetCurrentURL(kUrl);
  web_state_list_.InsertWebState(0, std::move(passed_web_state),
                                 WebStateList::INSERT_ACTIVATE,
                                 WebStateOpener(nullptr));

  // Present an HTTP authentication dialog over the WebState and verify the
  // location text and page icon.
  const std::string kMessage("message");
  const std::string kDefaultUsername("username");
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state, OverlayModality::kWebContentArea);
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<HTTPAuthOverlayRequestConfig>(
          kMessage, kDefaultUsername));
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_LOCATION_BAR_SIGN_IN),
              consumer_.locationText);
  EXPECT_FALSE(consumer_.icon);
  EXPECT_FALSE(consumer_.statusText);
}

// TODO(crbug.com/992578): Add more tests to this suite.
