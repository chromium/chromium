// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/plus_addresses/coordinator/plus_address_bottom_sheet_mediator.h"

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "base/types/expected.h"
#import "components/plus_addresses/fake_plus_address_service.h"
#import "components/plus_addresses/features.h"
#import "components/plus_addresses/plus_address_metrics.h"
#import "components/plus_addresses/plus_address_service.h"
#import "components/plus_addresses/plus_address_types.h"
#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_constants.h"
#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"
#import "url/origin.h"

using plus_addresses::FakePlusAddressService;

class PlusAddressBottomSheetMediatorTest : public PlatformTest {
 protected:
  PlusAddressBottomSheetMediatorTest() {}

  void SetUp() override {
    consumer_ = OCMProtocolMock(@protocol(PlusAddressBottomSheetConsumer));
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));
    BOOL incognito = browser_state_.get()->IsOffTheRecord();
    mediator_ = [[PlusAddressBottomSheetMediator alloc]
        initWithPlusAddressService:&service()
                         activeUrl:GURL(FakePlusAddressService::kFacet)
                  autofillCallback:base::DoNothing()
                         urlLoader:url_loader_
                         incognito:incognito];
    mediator_.consumer = consumer_;
  }

  FakePlusAddressService& service() { return service_; }
  PlusAddressBottomSheetMediator* mediator() { return mediator_; }
  FakeUrlLoadingBrowserAgent* url_loader() { return url_loader_.get(); }

  id consumer_;

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
  FakePlusAddressService service_;
  PlusAddressBottomSheetMediator* mediator_ = nil;
};

// Ensure that the consumer is notified when a plus address is successfully
// reserved.
TEST_F(PlusAddressBottomSheetMediatorTest, ReservePlusAddress) {
  OCMExpect([consumer_
      didReservePlusAddress:base::SysUTF8ToNSString(
                                FakePlusAddressService::kFakePlusAddress)]);
  [mediator() reservePlusAddress];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Ensure that the consumer is notified when errors are observed by the
// mediator.
TEST_F(PlusAddressBottomSheetMediatorTest, ReservePlusAddressError) {
  service().set_should_fail_to_reserve(true);
  OCMExpect([consumer_ notifyError:plus_addresses::PlusAddressMetrics::
                                       PlusAddressModalCompletionStatus::
                                           kReservePlusAddressError]);
  [mediator() reservePlusAddress];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Ensure the consumer is notified when plus addresses are confirmed.
TEST_F(PlusAddressBottomSheetMediatorTest, ConfirmPlusAddress) {
  OCMExpect([consumer_
      didReservePlusAddress:base::SysUTF8ToNSString(
                                FakePlusAddressService::kFakePlusAddress)]);
  [mediator() reservePlusAddress];
  EXPECT_OCMOCK_VERIFY(consumer_);
  OCMExpect([consumer_ didConfirmPlusAddress]);
  [mediator() confirmPlusAddress];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Ensure the consumer is notified when plus addresses confirmation fails.
TEST_F(PlusAddressBottomSheetMediatorTest, ConfirmPlusAddressError) {
  OCMExpect([consumer_
      didReservePlusAddress:base::SysUTF8ToNSString(
                                FakePlusAddressService::kFakePlusAddress)]);
  [mediator() reservePlusAddress];
  EXPECT_OCMOCK_VERIFY(consumer_);
  OCMExpect([consumer_ notifyError:plus_addresses::PlusAddressMetrics::
                                       PlusAddressModalCompletionStatus::
                                           kConfirmPlusAddressError]);
  service().set_should_fail_to_confirm(true);
  [mediator() confirmPlusAddress];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

TEST_F(PlusAddressBottomSheetMediatorTest, openManagementUrlOnNewTab) {
  [mediator() openNewTab:PlusAddressURLType::kManagement];

  EXPECT_EQ(GURL(plus_addresses::features::kPlusAddressManagementUrl.Get()),
            url_loader()->last_params.web_params.url);
  // Ensure one new tab is opened.
  EXPECT_EQ(1, url_loader()->load_new_tab_call_count);
}

// Ensure that `openNewTab` opens errorReportUrl.
TEST_F(PlusAddressBottomSheetMediatorTest, openErrorReportUrlOnNewTab) {
  [mediator() openNewTab:PlusAddressURLType::kErrorReport];

  EXPECT_EQ(GURL(plus_addresses::features::kPlusAddressErrorReportUrl.Get()),
            url_loader()->last_params.web_params.url);
  // Ensure one new tab is opened.
  EXPECT_EQ(1, url_loader()->load_new_tab_call_count);
}
