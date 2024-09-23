// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

const char kPageContent[] = "hello world";
NSString* const kTargetDeviceName = @"My other device";

std::unique_ptr<net::test_server::HttpResponse> RespondWithConstantPage(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("text/html");
  http_response->set_content(kPageContent);
  return http_response;
}

}  // namespace

@interface SendTabToSelfCoordinatorTestCase : ChromeTestCase
@end

@implementation SendTabToSelfCoordinatorTestCase

- (void)setUp {
  [super setUp];

  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&RespondWithConstantPage));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that the entry point button is shown to a signed out user, even if
// there are no device-level accounts.
- (void)testShowButtonIfSignedOutAndNoDeviceAccount {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:kPageContent];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabShareButton()]
      performAction:grey_tap()];

  NSString* sendTabToSelf =
      l10n_util::GetNSString(IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
  [ChromeEarlGrey verifyTextVisibleInActivitySheetWithID:sendTabToSelf];
}

- (void)testShowPromoIfSignedOutAndHasDeviceAccount {
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];
  [SigninEarlGrey addFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:kPageContent];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabShareButton()]
      performAction:grey_tap()];

  NSString* sendTabToSelf =
      l10n_util::GetNSString(IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
  [ChromeEarlGrey tapButtonInActivitySheetWithID:sendTabToSelf];

  [SigninEarlGreyUI verifyWebSigninIsVisible:YES];

  // Confirm the promo.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kWebSigninPrimaryButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // The device list should be shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(kTargetDeviceName)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testShowMessageIfSignedInAndNoTargetDevice {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:kPageContent];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabShareButton()]
      performAction:grey_tap()];
  NSString* sendTabToSelf =
      l10n_util::GetNSString(IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
  [ChromeEarlGrey tapButtonInActivitySheetWithID:sendTabToSelf];

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_SEND_TAB_TO_SELF_NO_TARGET_DEVICE_LABEL))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testShowDevicePickerIfSignedInAndHasTargetDevice {
  // Setting a recent timestamp here is necessary, otherwise the device will be
  // considered expired and won't be displayed.
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:kPageContent];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabShareButton()]
      performAction:grey_tap()];
  NSString* sendTabToSelf =
      l10n_util::GetNSString(IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
  [ChromeEarlGrey tapButtonInActivitySheetWithID:sendTabToSelf];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(kTargetDeviceName)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
