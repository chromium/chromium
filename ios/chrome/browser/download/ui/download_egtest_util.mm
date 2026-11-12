// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui/download_egtest_util.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_confirmation/account_picker_confirmation_screen_constants.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_screen/account_picker_screen_constants.h"
#import "ios/chrome/browser/download/ui/download_manager_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/http_response.h"

namespace download {

namespace {

// Matcher for the account picker.
id<GREYMatcher> AccountPicker() {
  return grey_allOf(
      grey_accessibilityID(
          kAccountPickerScreenNavigationControllerAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

// Matcher for the "Save" button in the account picker.
id<GREYMatcher> AccountPickerPrimaryButton() {
  return grey_allOf(
      grey_accessibilityID(kAccountPickerPrimaryButtonAccessibilityIdentifier),
      grey_interactable(), nil);
}

}  // namespace

id<GREYMatcher> DownloadButton() {
  NSString* accessibility_id =
      [ChromeEarlGrey isIncognitoMode]
          ? kDownloadManagerDownloadAccessibilityIdentifier
          : kDownloadManagerSaveEllipsisAccessibilityIdentifier;
  return grey_allOf(grey_accessibilityID(accessibility_id), grey_enabled(),
                    nil);
}

std::unique_ptr<net::test_server::HttpResponse> GetResponse(
    const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);
  result->set_content(
      "<a id='download' href='/download-example?50000'>Download</a>");
  return result;
}

std::unique_ptr<net::test_server::HttpResponse>
GetLinkToContentDispositionResponse(
    const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);
  result->set_content(
      "<a id='pdf' download href='/content-disposition'>PDF</a><br/><a "
      "id='pdf_new_window' target='_blank' href='/content-disposition'>PDF in "
      "new tab</a>");
  return result;
}

std::unique_ptr<net::test_server::HttpResponse>
GetContentDispositionPDFResponse(const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);
  result->set_content("fakePDFData");
  result->AddCustomHeader("Content-Type", "application/pdf");
  result->AddCustomHeader("Content-Disposition",
                          "attachment; filename=filename.pdf");
  return result;
}

[[nodiscard]] bool WaitForOpenInButton() {
  // These downloads usually take longer and need a longer timeout.
  constexpr base::TimeDelta kLongDownloadTimeout = base::Minutes(1);
  return base::test::ios::WaitUntilConditionOrTimeout(kLongDownloadTimeout, ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
        assertWithMatcher:grey_interactable()
                    error:&error];
    return (error == nil);
  });
}

[[nodiscard]] bool WaitForOpenPDFButton() {
  // These downloads usually take longer and need a longer timeout.
  constexpr base::TimeDelta kLongDownloadTimeout = base::Minutes(1);
  return base::test::ios::WaitUntilConditionOrTimeout(kLongDownloadTimeout, ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenPDFButton()]
        assertWithMatcher:grey_interactable()
                    error:&error];
    return (error == nil);
  });
}

[[nodiscard]] bool WaitForDownloadButton(bool loading) {
  return base::test::ios::WaitUntilConditionOrTimeout(
      loading ? base::test::ios::kWaitForPageLoadTimeout
              : base::test::ios::kWaitForUIElementTimeout,
      ^{
        NSError* error = nil;
        [[EarlGrey selectElementWithMatcher:DownloadButton()]
            assertWithMatcher:grey_interactable()
                        error:&error];
        return (error == nil);
      });
}

void StartLocalDownload() {
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];
  if (![ChromeEarlGrey isIncognitoMode]) {
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:AccountPicker()];
    [[EarlGrey selectElementWithMatcher:AccountPickerPrimaryButton()]
        performAction:grey_tap()];
    [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:AccountPicker()];
  }
}

}  // namespace download
