// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/plus_addresses/coordinator/plus_address_bottom_sheet_mediator.h"

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "base/types/expected.h"
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

namespace {
constexpr char kFacet[] = "facet.bar";
constexpr char kFakePlusAddress[] = "plus+remote@plus.plus";
}  // namespace

namespace plus_addresses {
namespace {

// Used to control the behavior of the coordinator's plus address service.
// This avoids the identity portion of the implementation, and mocks out the
// network requests normally handled by the `PlusAddressClient`.
class FakePlusAddressService : public PlusAddressService {
 public:
  FakePlusAddressService() = default;

  void ReservePlusAddress(const url::Origin& origin,
                          PlusAddressRequestCallback on_completed) override {
    if (force_error_) {
      std::move(on_completed)
          .Run(base::unexpected(PlusAddressRequestError(
              PlusAddressRequestErrorType::kOAuthError)));
      return;
    }
    std::move(on_completed)
        .Run(PlusProfile({.facet = kFacet,
                          .plus_address = kFakePlusAddress,
                          .is_confirmed = false}));
  }

  void ConfirmPlusAddress(const url::Origin& origin,
                          const std::string& plus_address,
                          PlusAddressRequestCallback on_completed) override {
    if (force_error_) {
      std::move(on_completed)
          .Run(base::unexpected(PlusAddressRequestError(
              PlusAddressRequestErrorType::kOAuthError)));
      return;
    }
    std::move(on_completed)
        .Run(PlusProfile({.facet = kFacet,
                          .plus_address = kFakePlusAddress,
                          .is_confirmed = false}));
  }

  absl::optional<std::string> GetPrimaryEmail() override {
    // Ensure the value is present without requiring identity setup.
    return "plus+primary@plus.plus";
  }

  void set_force_error_for_testing(bool force_error) {
    force_error_ = force_error;
  }

 private:
  bool force_error_;
};

}  // namespace
}  // namespace plus_addresses

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
    incognito_ = browser_state_.get()->IsOffTheRecord();
  }
  base::test::TaskEnvironment task_environment_;
  id consumer_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
  BOOL incognito_;
};

// Ensure that the consumer is notified when a plus address is successfully
// reserved.
// TODO(crbug.com/1506002): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_ReservePlusAddress ReservePlusAddress
#else
#define MAYBE_ReservePlusAddress DISABLED_ReservePlusAddress
#endif
TEST_F(PlusAddressBottomSheetMediatorTest, MAYBE_ReservePlusAddress) {
  plus_addresses::FakePlusAddressService service;
  PlusAddressBottomSheetMediator* mediator =
      [[PlusAddressBottomSheetMediator alloc]
          initWithPlusAddressService:&service
                           activeUrl:GURL(kFacet)
                    autofillCallback:base::DoNothing()
                           urlLoader:url_loader_
                           incognito:incognito_];
  mediator.consumer = consumer_;
  OCMExpect([consumer_
      didReservePlusAddress:base::SysUTF8ToNSString(kFakePlusAddress)]);
  [mediator reservePlusAddress];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Ensure that the consumer is notified when errors are observed by the
// mediator.
TEST_F(PlusAddressBottomSheetMediatorTest, ReservePlusAddressError) {
  plus_addresses::FakePlusAddressService service;
  service.set_force_error_for_testing(/*force_error_for_testing=*/true);
  PlusAddressBottomSheetMediator* mediator =
      [[PlusAddressBottomSheetMediator alloc]
          initWithPlusAddressService:&service
                           activeUrl:GURL(kFacet)
                    autofillCallback:base::DoNothing()
                           urlLoader:url_loader_
                           incognito:incognito_];
  mediator.consumer = consumer_;
  OCMExpect([consumer_ notifyError:plus_addresses::PlusAddressMetrics::
                                       PlusAddressModalCompletionStatus::
                                           kReservePlusAddressError]);
  [mediator reservePlusAddress];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Ensure the consumer is notified when plus addresses are confirmed.
// TODO(crbug.com/1506002): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_ConfirmPlusAddress ConfirmPlusAddress
#else
#define MAYBE_ConfirmPlusAddress DISABLED_ConfirmPlusAddress
#endif
TEST_F(PlusAddressBottomSheetMediatorTest, MAYBE_ConfirmPlusAddress) {
  plus_addresses::FakePlusAddressService service;
  PlusAddressBottomSheetMediator* mediator =
      [[PlusAddressBottomSheetMediator alloc]
          initWithPlusAddressService:&service
                           activeUrl:GURL(kFacet)
                    autofillCallback:base::DoNothing()
                           urlLoader:url_loader_
                           incognito:incognito_];
  mediator.consumer = consumer_;
  OCMExpect([consumer_
      didReservePlusAddress:base::SysUTF8ToNSString(kFakePlusAddress)]);
  [mediator reservePlusAddress];
  EXPECT_OCMOCK_VERIFY(consumer_);
  OCMExpect([consumer_ didConfirmPlusAddress]);
  [mediator confirmPlusAddress];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Ensure the consumer is notified when plus addresses confirmation fails.
// TODO(crbug.com/1506002): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_ConfirmPlusAddressError ConfirmPlusAddressError
#else
#define MAYBE_ConfirmPlusAddressError DISABLED_ConfirmPlusAddressError
#endif
TEST_F(PlusAddressBottomSheetMediatorTest, MAYBE_ConfirmPlusAddressError) {
  plus_addresses::FakePlusAddressService service;
  PlusAddressBottomSheetMediator* mediator =
      [[PlusAddressBottomSheetMediator alloc]
          initWithPlusAddressService:&service
                           activeUrl:GURL(kFacet)
                    autofillCallback:base::DoNothing()
                           urlLoader:url_loader_
                           incognito:incognito_];
  mediator.consumer = consumer_;
  OCMExpect([consumer_
      didReservePlusAddress:base::SysUTF8ToNSString(kFakePlusAddress)]);
  [mediator reservePlusAddress];
  EXPECT_OCMOCK_VERIFY(consumer_);
  OCMExpect([consumer_ notifyError:plus_addresses::PlusAddressMetrics::
                                       PlusAddressModalCompletionStatus::
                                           kConfirmPlusAddressError]);
  service.set_force_error_for_testing(/*force_error_for_testing=*/true);
  [mediator confirmPlusAddress];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

TEST_F(PlusAddressBottomSheetMediatorTest, openManagementUrlOnNewTab) {
  plus_addresses::FakePlusAddressService service;
  PlusAddressBottomSheetMediator* mediator =
      [[PlusAddressBottomSheetMediator alloc]
          initWithPlusAddressService:&service
                           activeUrl:GURL(kFacet)
                    autofillCallback:base::DoNothing()
                           urlLoader:url_loader_
                           incognito:incognito_];
  [mediator openNewTab:PlusAddressURLType::kManagement];

  EXPECT_EQ(GURL(plus_addresses::features::kPlusAddressManagementUrl.Get()),
            url_loader_->last_params.web_params.url);
  // Ensure one new tab is opened.
  EXPECT_EQ(1, url_loader_->load_new_tab_call_count);
}

// Ensure that `openNewTab` opens errorReportUrl.
TEST_F(PlusAddressBottomSheetMediatorTest, openErrorReportUrlOnNewTab) {
  plus_addresses::FakePlusAddressService service;
  PlusAddressBottomSheetMediator* mediator =
      [[PlusAddressBottomSheetMediator alloc]
          initWithPlusAddressService:&service
                           activeUrl:GURL(kFacet)
                    autofillCallback:base::DoNothing()
                           urlLoader:url_loader_
                           incognito:incognito_];
  [mediator openNewTab:PlusAddressURLType::kErrorReport];

  EXPECT_EQ(GURL(plus_addresses::features::kPlusAddressErrorReportUrl.Get()),
            url_loader_->last_params.web_params.url);
  // Ensure one new tab is opened.
  EXPECT_EQ(1, url_loader_->load_new_tab_call_count);
}
