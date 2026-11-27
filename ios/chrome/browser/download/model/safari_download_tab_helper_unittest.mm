// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/safari_download_tab_helper.h"

#import <memory>

#import "base/test/task_environment.h"
#import "ios/chrome/browser/download/model/safari_download_tab_helper_delegate.h"
#import "ios/chrome/browser/shared/model/utils/mime_type_util.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "net/base/apple/url_conversions.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

namespace {
constexpr char kMobileConfigUrl[] = "https://test.test/profile.mobileconfig";
constexpr char kCalendarUrl[] = "https://test.test/calendar.ics";
constexpr char kWalletOrderUrl[] = "https://test.test/order.order";
}  // namespace

class SafariDownloadTabHelperTest : public PlatformTest {
 protected:
  SafariDownloadTabHelperTest() {
    SafariDownloadTabHelper::CreateForWebState(&web_state_);
    web_state_.WasShown();
    delegate_mock_ =
        OCMStrictProtocolMock(@protocol(SafariDownloadTabHelperDelegate));
    tab_helper()->set_delegate(delegate_mock_);
  }

  SafariDownloadTabHelper* tab_helper() {
    return SafariDownloadTabHelper::FromWebState(&web_state_);
  }

  base::test::TaskEnvironment task_environment_;
  web::FakeWebState web_state_;
  OCMockObject<SafariDownloadTabHelperDelegate>* delegate_mock_;
};

// Tests that Safari downloads triggered while the WebState is visible
// immediately present their corresponding warning alerts.
TEST_F(SafariDownloadTabHelperTest, PresentsAlertsWhenVisible) {
  const GURL mobile_config_url(kMobileConfigUrl);
  OCMExpect([delegate_mock_
      presentMobileConfigAlertFromURL:net::NSURLWithGURL(mobile_config_url)]);
  tab_helper()->DownloadMobileConfig(std::make_unique<web::FakeDownloadTask>(
      mobile_config_url, kMobileConfigurationType));

  const GURL calendar_url(kCalendarUrl);
  OCMExpect([delegate_mock_
      presentCalendarAlertFromURL:net::NSURLWithGURL(calendar_url)]);
  tab_helper()->DownloadCalendar(
      std::make_unique<web::FakeDownloadTask>(calendar_url, kCalendarMimeType));

  const GURL wallet_order_url(kWalletOrderUrl);
  OCMExpect([delegate_mock_
      presentAppleWalletOrderAlertFromURL:net::NSURLWithGURL(
                                              wallet_order_url)]);
  tab_helper()->DownloadAppleWalletOrder(
      std::make_unique<web::FakeDownloadTask>(wallet_order_url,
                                              kAppleWalletOrderMimeType));

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that a mobileconfig download triggered while the WebState is hidden is
// dropped and not replayed when the WebState becomes visible.
TEST_F(SafariDownloadTabHelperTest, DropsMobileConfigDownloadWhenHidden) {
  web_state_.WasHidden();

  tab_helper()->DownloadMobileConfig(std::make_unique<web::FakeDownloadTask>(
      GURL(kMobileConfigUrl), kMobileConfigurationType));
  web_state_.WasShown();

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that a calendar download triggered while the WebState is hidden is
// dropped and not replayed when the WebState becomes visible.
TEST_F(SafariDownloadTabHelperTest, DropsCalendarDownloadWhenHidden) {
  web_state_.WasHidden();

  tab_helper()->DownloadCalendar(std::make_unique<web::FakeDownloadTask>(
      GURL(kCalendarUrl), kCalendarMimeType));
  web_state_.WasShown();

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that an Apple Wallet order download triggered while the WebState is
// hidden is dropped and not replayed when the WebState becomes visible.
TEST_F(SafariDownloadTabHelperTest, DropsAppleWalletOrderDownloadWhenHidden) {
  web_state_.WasHidden();

  tab_helper()->DownloadAppleWalletOrder(
      std::make_unique<web::FakeDownloadTask>(GURL(kWalletOrderUrl),
                                              kAppleWalletOrderMimeType));
  web_state_.WasShown();

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}
