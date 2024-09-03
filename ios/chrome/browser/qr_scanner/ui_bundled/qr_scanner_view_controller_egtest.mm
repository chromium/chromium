// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>

#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/qr_scanner/ui_bundled/qr_scanner_app_interface.h"
#import "ios/chrome/browser/ui/scanner/camera_state.h"
#import "ios/chrome/browser/ui/settings/settings_app_interface.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/earl_grey_scoped_block_swizzler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using scanner::CameraState;

// Override a QRScannerViewController voice over check, simulating voice
// over being enabled. This doesn't reset the previous value, don't use
// nested.
class ScopedQRScannerVoiceSearchOverride {
 public:
  ScopedQRScannerVoiceSearchOverride(UIViewController* scanner_view_controller)
      : scanner_view_controller_(scanner_view_controller) {
    [QRScannerAppInterface
        overrideVoiceOverCheckForQRScannerViewController:
            scanner_view_controller_
                                                    isOn:YES];
  }

  ScopedQRScannerVoiceSearchOverride(
      const ScopedQRScannerVoiceSearchOverride&) = delete;
  ScopedQRScannerVoiceSearchOverride& operator=(
      const ScopedQRScannerVoiceSearchOverride&) = delete;

  ~ScopedQRScannerVoiceSearchOverride() {
    [QRScannerAppInterface overrideVoiceOverCheckForQRScannerViewController:
                               scanner_view_controller_
                                                                       isOn:NO];
  }

 private:
  UIViewController* scanner_view_controller_;
};

namespace {

char kTestURL[] = "/testurl";
char kTestURLResponse[] = "Test URL page";
char kTestURLEdited[] = "/testuredited";
char kTestURLEditedResponse[] = "Test URL edited page";

char kTestQuery[] = "testquery";
char kTestQueryURL[] = "/search";
char kTestQueryURLParams[] = "?q={searchTerms}";
char kTestQueryResponse[] = "Query: testquery";
char kTestQueryEditedResponse[] = "Query: testqueredited";
char kTestURLForbiddenCharacters[] = "test\u2028\u2029\u0085url";

char kTestDataURL[] = "data:dataURL";
char kTestSanitizedDataURL[] = "\"data:dataURL\"";
char kTestDataURLResponse[] = "Query: \"data:dataURL\"";

// The GREYCondition timeout used for calls to waitWithTimeout:pollInterval:.
CFTimeInterval kGREYConditionTimeout = 5;
// The GREYCondition poll interval used for calls to
// waitWithTimeout:pollInterval:.
CFTimeInterval kGREYConditionPollInterval = 0.1;

// Returns the GREYMatcher for an element which is visible, interactable, and
// enabled.
id<GREYMatcher> VisibleInteractableEnabled() {
  return grey_allOf(grey_enabled(), grey_sufficientlyVisible(), nil);
}

// Returns the GREYMatcher for the button that closes the QR Scanner.
id<GREYMatcher> QrScannerCloseButton() {
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabel(
                        QRScannerAppInterface.closeIconAccessibilityLabel),
                    grey_userInteractionEnabled(), nil);
}

// Returns the GREYMatcher for the button which indicates that torch is off and
// which turns on the torch.
id<GREYMatcher> QrScannerTorchOffButton() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_SCANNER_TORCH_BUTTON_ACCESSIBILITY_LABEL)),
                    grey_accessibilityValue(l10n_util::GetNSString(
                        IDS_IOS_SCANNER_TORCH_OFF_ACCESSIBILITY_VALUE)),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Returns the GREYMatcher for the button which indicates that torch is on and
// which turns off the torch.
id<GREYMatcher> QrScannerTorchOnButton() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_SCANNER_TORCH_BUTTON_ACCESSIBILITY_LABEL)),
                    grey_accessibilityValue(l10n_util::GetNSString(
                        IDS_IOS_SCANNER_TORCH_ON_ACCESSIBILITY_VALUE)),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Returns the GREYMatcher for the QR Scanner viewport caption.
id<GREYMatcher> QrScannerViewportCaption() {
  return chrome_test_util::StaticTextWithAccessibilityLabelId(
      IDS_IOS_QR_SCANNER_VIEWPORT_CAPTION);
}

// Returns the GREYMatcher for the Cancel button to dismiss a UIAlertController.
id<GREYMatcher> DialogCancelButton() {
  return grey_allOf(
      grey_text(l10n_util::GetNSString(IDS_IOS_QR_SCANNER_ALERT_CANCEL)),
      grey_accessibilityTrait(UIAccessibilityTraitStaticText),
      grey_sufficientlyVisible(), nil);
}

// Opens the QR Scanner view.
void ShowQRScanner() {
  // Tap the omnibox to get the keyboard accessory view to show up.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NewTabPageOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];

  // Tap the QR Code scanner button in the keyboard accessory view.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"QR code search")]
      performAction:grey_tap()];
}

// Taps the `button`.
void TapButton(id<GREYMatcher> button) {
  [[EarlGrey selectElementWithMatcher:button] performAction:grey_tap()];
}

// Appends the given `editText` to the `text` already in the omnibox and presses
// the keyboard return key.
void EditOmniboxTextAndTapKeyboardReturn(std::string text, NSString* editText) {
  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:editText flags:0];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];
}

// Presses the keyboard return key.
void TapKeyboardReturnKeyInOmniboxWithText(std::string text) {
  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];
}

// Provides responses for the test page URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  std::string body_content;
  if (base::StartsWith(request.relative_url, kTestURL,
                       base::CompareCase::SENSITIVE)) {
    body_content = kTestURLResponse;
  } else if (base::StartsWith(request.relative_url, kTestURLEdited,
                              base::CompareCase::SENSITIVE)) {
    body_content = kTestURLEditedResponse;
  } else if (base::StartsWith(request.relative_url, kTestQueryURL,
                              base::CompareCase::SENSITIVE)) {
    GURL url = request.GetURL();
    std::string query;
    bool found = net::GetValueForKeyInQuery(url, "q", &query);
    if (found) {
      body_content = "Query: " + query;
    } else {
      body_content = "No query";
    }
  } else {
    return nullptr;
  }

  http_response->set_content(
      base::StringPrintf("<html><body>%s</body></html>", body_content.c_str()));

  return std::move(http_response);
}

}  // namespace

#pragma mark - Test Case

@interface QRScannerViewControllerTestCase : ChromeTestCase {
  GURL _testURL;
  GURL _testURLEdited;
  GURL _testQuery;
}

@end

@implementation QRScannerViewControllerTestCase {
  // A swizzler for the CameraController method cameraControllerWithDelegate:.
  std::unique_ptr<EarlGreyScopedBlockSwizzler> _camera_controller_swizzler;
}

- (void)setUp {
  [super setUp];

  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  _testURL = self.testServer->GetURL(kTestURL);
  _testURLEdited = self.testServer->GetURL(kTestURLEdited);
  _testQuery = self.testServer->GetURL(kTestQueryURL);

  NSString* templateURL =
      base::SysUTF8ToNSString(_testQuery.spec() + kTestQueryURLParams);
  [SettingsAppInterface overrideSearchEngineWithURL:templateURL];
}

- (void)tearDown {
  [super tearDown];
  [SettingsAppInterface resetSearchEngine];
  _camera_controller_swizzler.reset();
}

// Checks that the close button is visible, interactable, and enabled.
- (void)assertCloseButtonIsVisible {
  [[EarlGrey selectElementWithMatcher:QrScannerCloseButton()]
      assertWithMatcher:VisibleInteractableEnabled()];
}

// Checks that the torch off button is visible, interactable, and enabled, and
// that the torch on button is not.
- (void)assertTorchOffButtonIsVisible {
  [[EarlGrey selectElementWithMatcher:QrScannerTorchOffButton()]
      assertWithMatcher:VisibleInteractableEnabled()];
  [[EarlGrey selectElementWithMatcher:QrScannerTorchOnButton()]
      assertWithMatcher:grey_notVisible()];
}

// Checks that the torch on button is visible, interactable, and enabled, and
// that the torch off button is not.
- (void)assertTorchOnButtonIsVisible {
  [[EarlGrey selectElementWithMatcher:QrScannerTorchOnButton()]
      assertWithMatcher:VisibleInteractableEnabled()];
  [[EarlGrey selectElementWithMatcher:QrScannerTorchOffButton()]
      assertWithMatcher:grey_notVisible()];
}

// Checks that the torch off button is visible and disabled.
- (void)assertTorchButtonIsDisabled {
  [[EarlGrey selectElementWithMatcher:QrScannerTorchOffButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];
}

// Checks that the camera viewport caption is visible.
- (void)assertCameraViewportCaptionIsVisible {
  [[EarlGrey selectElementWithMatcher:QrScannerViewportCaption()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks that the close button, the camera preview, and the camera viewport
// caption are visible. If `torch` is YES, checks that the torch off button is
// visible, otherwise checks that the torch button is disabled. If `preview` is
// YES, checks that the preview is visible and of the same size as the QR
// Scanner view, otherwise checks that the preview is in the view hierarchy but
// is hidden.
- (void)assertQRScannerUIIsVisibleWithTorch:(BOOL)torch {
  [self assertCloseButtonIsVisible];
  [self assertCameraViewportCaptionIsVisible];
  if (torch) {
    [self assertTorchOffButtonIsVisible];
  } else {
    [self assertTorchButtonIsDisabled];
  }
}

// Presents the QR Scanner with a command, waits for it to be displayed, and
// checks if all its views and buttons are visible. Checks that no alerts are
// presented.
- (void)showQRScannerAndCheckLayoutWithCameraMock:(id)mock {
  UIViewController* bvc = QRScannerAppInterface.currentBrowserViewController;
  NSError* error =
      [QRScannerAppInterface assertModalOfClass:@"QRScannerViewController"
                               isNotPresentedBy:bvc];
  GREYAssertNil(error, error.localizedDescription);
  error = [QRScannerAppInterface assertModalOfClass:@"UIAlertController"
                                   isNotPresentedBy:bvc];
  GREYAssertNil(error, error.localizedDescription);

  [QRScannerAppInterface addCameraControllerInitializationExpectations:mock];
  ShowQRScanner();
  [self waitForModalOfClass:@"QRScannerViewController" toAppearAbove:bvc];
  [self assertQRScannerUIIsVisibleWithTorch:NO];
  error =
      [QRScannerAppInterface assertModalOfClass:@"UIAlertController"
                               isNotPresentedBy:[bvc presentedViewController]];
  GREYAssertNil(error, error.localizedDescription);
  error = [QRScannerAppInterface assertModalOfClass:@"UIAlertController"
                                   isNotPresentedBy:bvc];
  GREYAssertNil(error, error.localizedDescription);
}

// Closes the QR scanner by tapping the close button and waits for it to
// disappear.
- (void)closeQRScannerWithCameraMock:(id)mock {
  [QRScannerAppInterface addCameraControllerDismissalExpectations:mock];
  TapButton(QrScannerCloseButton());
  [self waitForModalOfClass:@"QRScannerViewController"
       toDisappearFromAbove:QRScannerAppInterface.currentBrowserViewController];
}

// Checks that the omnibox is visible and contains `text`.
- (void)assertOmniboxIsVisibleWithText:(std::string)text {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(text)]
      assertWithMatcher:grey_notNil()];
}

#pragma mark - Helpers for Dialogs

// Checks that the QRScannerViewController is presenting a UIAlertController and
// that the title of this alert corresponds to `state`.
- (void)assertQRScannerIsPresentingADialogForState:(CameraState)state {
  NSError* error = [QRScannerAppInterface
      assertModalOfClass:@"UIAlertController"
           isPresentedBy:[QRScannerAppInterface.currentBrowserViewController
                                 presentedViewController]];
  GREYAssertNil(error, error.localizedDescription);
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_text([QRScannerAppInterface
                                              dialogTitleForState:state])];
}

// Checks that there is no visible alert with title corresponding to `state`.
- (void)assertQRScannerIsNotPresentingADialogForState:(CameraState)state {
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:grey_text([QRScannerAppInterface
                                                 dialogTitleForState:state])];
}

#pragma mark - Helpers for mocks

// Swizzles the QRScannerViewController property cameraController: to return
// `cameraControllerMock` instead of a new instance of CameraController.
- (void)swizzleCameraController:(id)cameraControllerMock {
  id swizzleCameraControllerBlock = [QRScannerAppInterface
      cameraControllerSwizzleBlockWithMock:cameraControllerMock];

  _camera_controller_swizzler = std::make_unique<EarlGreyScopedBlockSwizzler>(
      @"QRScannerViewController", @"cameraController",
      swizzleCameraControllerBlock);
}

// Checks that the modal presented by `viewController` is of class `klass` and
// waits for the modal's view to load.
- (void)waitForModalOfClass:(NSString*)klassString
              toAppearAbove:(UIViewController*)viewController {
  NSError* error = [QRScannerAppInterface assertModalOfClass:klassString
                                               isPresentedBy:viewController];
  GREYAssertNil(error, error.localizedDescription);
  UIViewController* modal = [viewController presentedViewController];
  GREYCondition* modalViewLoadedCondition =
      [GREYCondition conditionWithName:@"modalViewLoadedCondition"
                                 block:^BOOL {
                                   return [modal isViewLoaded];
                                 }];
  BOOL modalViewLoaded =
      [modalViewLoadedCondition waitWithTimeout:kGREYConditionTimeout
                                   pollInterval:kGREYConditionPollInterval];
  NSString* errorString = [NSString
      stringWithFormat:@"The view of a modal of class %@ should be loaded.",
                       klassString];
  GREYAssertTrue(modalViewLoaded, errorString);
}

// Checks that the `viewController` is not presenting a modal, or that the modal
// presented by `viewController` is not of class `klass`. If a modal was
// previously presented, waits until it is dismissed.
- (void)waitForModalOfClass:(NSString*)klassString
       toDisappearFromAbove:(UIViewController*)viewController {
  BOOL (^waitingBlock)() =
      [QRScannerAppInterface blockForWaitingForModalOfClass:klassString
                                       toDisappearFromAbove:viewController];
  Class klass = NSClassFromString(klassString);
  GREYCondition* modalViewDismissedCondition =
      [GREYCondition conditionWithName:@"modalViewDismissedCondition"
                                 block:waitingBlock];

  BOOL modalViewDismissed =
      [modalViewDismissedCondition waitWithTimeout:kGREYConditionTimeout
                                      pollInterval:kGREYConditionPollInterval];
  NSString* errorString = [NSString
      stringWithFormat:@"The modal of class %@ should be loaded.", klass];
  GREYAssertTrue(modalViewDismissed, errorString);
}

#pragma mark - Appearance Tests

// Tests that the close button, camera preview, viewport caption, and the torch
// button are visible if the camera is available. The preview is delayed.
- (void)testQRScannerUIIsShown {
  id cameraControllerMock =
      [QRScannerAppInterface cameraControllerMockWithAuthorizationStatus:
                                 AVAuthorizationStatusAuthorized];
  [self swizzleCameraController:cameraControllerMock];

  // Open the QR scanner.
  [self showQRScannerAndCheckLayoutWithCameraMock:cameraControllerMock];

  // Preview is loaded and camera is ready to be displayed.
  [self assertQRScannerUIIsVisibleWithTorch:NO];

  // Close the QR scanner.
  [self closeQRScannerWithCameraMock:cameraControllerMock];
  [cameraControllerMock verify];
}

// Tests that the torch is switched on and off when pressing the torch button,
// and that the button icon changes accordingly.
- (void)testTurningTorchOnAndOff {
  id cameraControllerMock =
      [QRScannerAppInterface cameraControllerMockWithAuthorizationStatus:
                                 AVAuthorizationStatusAuthorized];
  [self swizzleCameraController:cameraControllerMock];

  // Open the QR scanner.
  [self showQRScannerAndCheckLayoutWithCameraMock:cameraControllerMock];

  // Torch becomes available.
  [QRScannerAppInterface callTorchAvailabilityChanged:YES];
  [self assertQRScannerUIIsVisibleWithTorch:YES];

  // Turn torch on.
  [QRScannerAppInterface
      addCameraControllerTorchOnExpectations:cameraControllerMock];
  [self assertTorchOffButtonIsVisible];
  TapButton(QrScannerTorchOffButton());
  [self assertTorchOffButtonIsVisible];

  // Torch becomes active.
  [QRScannerAppInterface callTorchStateChanged:YES];
  [self assertTorchOnButtonIsVisible];

  // Turn torch off.
  [QRScannerAppInterface
      addCameraControllerTorchOffExpectations:cameraControllerMock];
  TapButton(QrScannerTorchOnButton());
  [self assertTorchOnButtonIsVisible];

  // Torch becomes inactive.
  [QRScannerAppInterface callTorchStateChanged:NO];
  [self assertTorchOffButtonIsVisible];

  // Close the QR scanner.
  [self closeQRScannerWithCameraMock:cameraControllerMock];
  [cameraControllerMock verify];
}

// Tests that if the QR scanner is closed while the torch is on, the torch is
// switched off and the correct button indicating that the torch is off is shown
// when the scanner is opened again.
- (void)testTorchButtonIsResetWhenQRScannerIsReopened {
  id cameraControllerMock =
      [QRScannerAppInterface cameraControllerMockWithAuthorizationStatus:
                                 AVAuthorizationStatusAuthorized];
  [self swizzleCameraController:cameraControllerMock];

  // Open the QR scanner.
  [self showQRScannerAndCheckLayoutWithCameraMock:cameraControllerMock];
  [self assertQRScannerUIIsVisibleWithTorch:NO];
  [QRScannerAppInterface callTorchAvailabilityChanged:YES];
  [self assertQRScannerUIIsVisibleWithTorch:YES];

  // Turn torch on.
  [QRScannerAppInterface
      addCameraControllerTorchOnExpectations:cameraControllerMock];
  TapButton(QrScannerTorchOffButton());
  [QRScannerAppInterface callTorchStateChanged:YES];
  [self assertTorchOnButtonIsVisible];

  // Close the QR scanner.
  [self closeQRScannerWithCameraMock:cameraControllerMock];

  // Reopen the QR scanner.
  [self showQRScannerAndCheckLayoutWithCameraMock:cameraControllerMock];
  [QRScannerAppInterface callTorchAvailabilityChanged:YES];
  [self assertTorchOffButtonIsVisible];

  // Close the QR scanner again.
  [self closeQRScannerWithCameraMock:cameraControllerMock];
  [cameraControllerMock verify];
}

// Tests that the torch button is disabled when the camera reports that torch
// became unavailable.
- (void)testTorchButtonIsDisabledWhenTorchBecomesUnavailable {
  id cameraControllerMock =
      [QRScannerAppInterface cameraControllerMockWithAuthorizationStatus:
                                 AVAuthorizationStatusAuthorized];
  [self swizzleCameraController:cameraControllerMock];

  // Open the QR scanner.
  [self showQRScannerAndCheckLayoutWithCameraMock:cameraControllerMock];

  // Torch becomes available.
  [QRScannerAppInterface callTorchAvailabilityChanged:YES];
  [self assertQRScannerUIIsVisibleWithTorch:YES];

  // Torch becomes unavailable.
  [QRScannerAppInterface callTorchAvailabilityChanged:NO];
  [self assertQRScannerUIIsVisibleWithTorch:NO];

  // Close the QR scanner.
  [self closeQRScannerWithCameraMock:cameraControllerMock];
  [cameraControllerMock verify];
}

#pragma mark - Dialogs Tests

// Tests that a UIAlertController is presented instead of the
// QRScannerViewController if the camera is unavailable.
- (void)testCameraUnavailableDialog {
  UIViewController* bvc = QRScannerAppInterface.currentBrowserViewController;
  NSError* error =
      [QRScannerAppInterface assertModalOfClass:@"QRScannerViewController"
                               isNotPresentedBy:bvc];
  GREYAssertNil(error, error.localizedDescription);
  error = [QRScannerAppInterface assertModalOfClass:@"UIAlertController"
                                   isNotPresentedBy:bvc];
  GREYAssertNil(error, error.localizedDescription);

  id cameraControllerMock = [QRScannerAppInterface
      cameraControllerMockWithAuthorizationStatus:AVAuthorizationStatusDenied];
  [self swizzleCameraController:cameraControllerMock];

  ShowQRScanner();
  error = [QRScannerAppInterface assertModalOfClass:@"QRScannerViewController"
                                   isNotPresentedBy:bvc];
  GREYAssertNil(error, error.localizedDescription);

  [self waitForModalOfClass:@"UIAlertController" toAppearAbove:bvc];

  TapButton(DialogCancelButton());
  [self waitForModalOfClass:@"UIAlertController" toDisappearFromAbove:bvc];
}

// Tests that a UIAlertController is presented by the QRScannerViewController if
// the camera state changes after the QRScannerViewController is presented.
- (void)testDialogIsDisplayedIfCameraStateChanges {
  id cameraControllerMock =
      [QRScannerAppInterface cameraControllerMockWithAuthorizationStatus:
                                 AVAuthorizationStatusAuthorized];
  [self swizzleCameraController:cameraControllerMock];

  std::vector<CameraState> tests{scanner::MULTIPLE_FOREGROUND_APPS,
                                 scanner::CAMERA_UNAVAILABLE,
                                 scanner::CAMERA_PERMISSION_DENIED,
                                 scanner::CAMERA_IN_USE_BY_ANOTHER_APPLICATION};

  for (const CameraState& state : tests) {
    [self showQRScannerAndCheckLayoutWithCameraMock:cameraControllerMock];
    [QRScannerAppInterface callCameraStateChanged:state];
    [self assertQRScannerIsPresentingADialogForState:state];

    // Close the dialog.
    [QRScannerAppInterface
        addCameraControllerDismissalExpectations:cameraControllerMock];
    TapButton(DialogCancelButton());
    UIViewController* bvc = QRScannerAppInterface.currentBrowserViewController;
    [self waitForModalOfClass:@"QRScannerViewController"
         toDisappearFromAbove:bvc];
    NSError* error =
        [QRScannerAppInterface assertModalOfClass:@"UIAlertController"
                                 isNotPresentedBy:bvc];
    GREYAssertNil(error, error.localizedDescription);
  }

  [cameraControllerMock verify];
}

// Tests that a new dialog replaces an old dialog if the camera state changes.
// TODO(crbug.com/40105250): Re-enable test on iOS12.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testDialogIsReplacedIfCameraStateChanges \
  testDialogIsReplacedIfCameraStateChanges
#else
#define MAYBE_testDialogIsReplacedIfCameraStateChanges \
  DISABLED_testDialogIsReplacedIfCameraStateChanges
#endif
- (void)MAYBE_testDialogIsReplacedIfCameraStateChanges {
  id cameraControllerMock =
      [QRScannerAppInterface cameraControllerMockWithAuthorizationStatus:
                                 AVAuthorizationStatusAuthorized];
  [self swizzleCameraController:cameraControllerMock];

  // Change state to CAMERA_UNAVAILABLE.
  CameraState currentState = scanner::CAMERA_UNAVAILABLE;
  [self showQRScannerAndCheckLayoutWithCameraMock:cameraControllerMock];
  [QRScannerAppInterface callCameraStateChanged:currentState];
  [self assertQRScannerIsPresentingADialogForState:currentState];

  std::vector<CameraState> tests{scanner::CAMERA_PERMISSION_DENIED,
                                 scanner::MULTIPLE_FOREGROUND_APPS,
                                 scanner::CAMERA_IN_USE_BY_ANOTHER_APPLICATION,
                                 scanner::CAMERA_UNAVAILABLE};

  for (const CameraState& state : tests) {
    [QRScannerAppInterface callCameraStateChanged:state];
    [self assertQRScannerIsPresentingADialogForState:state];
    [self assertQRScannerIsNotPresentingADialogForState:currentState];
    currentState = state;
  }

  // Cancel the dialog.
  [QRScannerAppInterface
      addCameraControllerDismissalExpectations:cameraControllerMock];
  TapButton(DialogCancelButton());
  [self waitForModalOfClass:@"QRScannerViewController"
       toDisappearFromAbove:QRScannerAppInterface.currentBrowserViewController];
  NSError* error = [QRScannerAppInterface
      assertModalOfClass:@"UIAlertController"
        isNotPresentedBy:QRScannerAppInterface.currentBrowserViewController];
  GREYAssertNil(error, error.localizedDescription);

  [cameraControllerMock verify];
}

// Tests that an error dialog is dismissed if the camera becomes available.
- (void)testDialogDismissedIfCameraBecomesAvailable {
  id cameraControllerMock =
      [QRScannerAppInterface cameraControllerMockWithAuthorizationStatus:
                                 AVAuthorizationStatusAuthorized];
  [self swizzleCameraController:cameraControllerMock];

  std::vector<CameraState> tests{scanner::CAMERA_IN_USE_BY_ANOTHER_APPLICATION,
                                 scanner::CAMERA_UNAVAILABLE,
                                 scanner::MULTIPLE_FOREGROUND_APPS,
                                 scanner::CAMERA_PERMISSION_DENIED};

  for (const CameraState& state : tests) {
    [self showQRScannerAndCheckLayoutWithCameraMock:cameraControllerMock];
    [QRScannerAppInterface callCameraStateChanged:state];
    [self assertQRScannerIsPresentingADialogForState:state];

    // Change state to CAMERA_AVAILABLE.
    [QRScannerAppInterface callCameraStateChanged:scanner::CAMERA_AVAILABLE];
    [self assertQRScannerIsNotPresentingADialogForState:state];
    [self closeQRScannerWithCameraMock:cameraControllerMock];
  }

  [cameraControllerMock verify];
}

#pragma mark - Scanned Result Tests

// A helper function for testing that the view controller correctly passes the
// received results to its delegate and that pages can be loaded. The result
// received from the camera controller is in `result`, `response` is the
// expected response on the loaded page, and `editString` is a nullable string
// which can be appended to the response in the omnibox before the page is
// loaded.
- (void)doTestReceivingResult:(std::string)result
              sanitizedResult:(std::string)sanitizedResult
                     response:(std::string)response
                         edit:(NSString*)editString {
  id cameraControllerMock =
      [QRScannerAppInterface cameraControllerMockWithAuthorizationStatus:
                                 AVAuthorizationStatusAuthorized];
  [self swizzleCameraController:cameraControllerMock];

  // Open the QR scanner.
  [self showQRScannerAndCheckLayoutWithCameraMock:cameraControllerMock];
  [QRScannerAppInterface callTorchAvailabilityChanged:YES];
  [self assertQRScannerUIIsVisibleWithTorch:YES];

  // Receive a scanned result from the camera.
  [QRScannerAppInterface
      addCameraControllerDismissalExpectations:cameraControllerMock];
  [QRScannerAppInterface
      callReceiveQRScannerResult:base::SysUTF8ToNSString(result)];

  [self waitForModalOfClass:@"QRScannerViewController"
       toDisappearFromAbove:QRScannerAppInterface.currentBrowserViewController];
  [cameraControllerMock verify];

  // Optionally edit the text in the omnibox before pressing return.
  [self assertOmniboxIsVisibleWithText:sanitizedResult];
  if (editString != nil) {
    EditOmniboxTextAndTapKeyboardReturn(sanitizedResult, editString);
  } else {
    TapKeyboardReturnKeyInOmniboxWithText(sanitizedResult);
  }
  [ChromeEarlGrey waitForWebStateContainingText:response];

  // Press the back button to get back to the NTP.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  NSError* error = [QRScannerAppInterface
      assertModalOfClass:@"QRScannerViewController"
        isNotPresentedBy:QRScannerAppInterface.currentBrowserViewController];
  GREYAssertNil(error, error.localizedDescription);
}

- (void)doTestReceivingResult:(std::string)result
                     response:(std::string)response
                         edit:(NSString*)editString {
  [self doTestReceivingResult:result
              sanitizedResult:result
                     response:response
                         edit:editString];
}

// Test that the correct page is loaded if the scanner result is a URL which is
// then manually edited when VoiceOver is enabled.
- (void)testReceivingQRScannerURLResultWithVoiceOver {
  id cameraControllerMock =
      [QRScannerAppInterface cameraControllerMockWithAuthorizationStatus:
                                 AVAuthorizationStatusAuthorized];
  [self swizzleCameraController:cameraControllerMock];

  // Open the QR scanner.
  [self showQRScannerAndCheckLayoutWithCameraMock:cameraControllerMock];
  [QRScannerAppInterface callTorchAvailabilityChanged:YES];
  [self assertQRScannerUIIsVisibleWithTorch:YES];

  // Add override for the VoiceOver check.
  ScopedQRScannerVoiceSearchOverride scopedOverride(
      [QRScannerAppInterface
              .currentBrowserViewController presentedViewController]);

  // Receive a scanned result from the camera.
  [QRScannerAppInterface
      addCameraControllerDismissalExpectations:cameraControllerMock];
  [QRScannerAppInterface callReceiveQRScannerResult:base::SysUTF8ToNSString(
                                                        _testURL.GetContent())];

  // Fake the end of the VoiceOver announcement.
  [QRScannerAppInterface postScanEndVoiceoverAnnouncement];

  [self waitForModalOfClass:@"QRScannerViewController"
       toDisappearFromAbove:QRScannerAppInterface.currentBrowserViewController];
  [cameraControllerMock verify];

  // Optionally edit the text in the omnibox before pressing return.
  [self assertOmniboxIsVisibleWithText:_testURL.GetContent()];
  TapKeyboardReturnKeyInOmniboxWithText(_testURL.GetContent());
  [ChromeEarlGrey waitForWebStateContainingText:kTestURLResponse];
}

// Test that the correct page is loaded if the scanner result is a URL.
- (void)testReceivingQRScannerURLResult {
  [self doTestReceivingResult:_testURL.GetContent()
                     response:kTestURLResponse
                         edit:nil];
}

// Test that the URL is sanitized and the correct page is loaded if the scanner
// result is a URL with forbidden characters.
- (void)testForbiddenCharactersRemoved {
  [self doTestReceivingResult:self.testServer->base_url().GetContent() +
                              kTestURLForbiddenCharacters
              sanitizedResult:_testURL.GetContent()
                     response:kTestURLResponse
                         edit:nil];
}

// Test that the correct page is loaded if the scanner result is a URL which is
// then manually edited.
- (void)testReceivingQRScannerURLResultAndEditingTheURL {
  [self doTestReceivingResult:_testURL.GetContent()
                     response:kTestURLEditedResponse
                         edit:@"\bedited/"];
}

// Test that the correct page is loaded if the scanner result is a search query.
- (void)testReceivingQRScannerSearchQueryResult {
  [self doTestReceivingResult:kTestQuery response:kTestQueryResponse edit:nil];
}

// Test that the correct page is loaded if the scanner result is a search query
// which is then manually edited.
- (void)testReceivingQRScannerSearchQueryResultAndEditingTheQuery {
  [self doTestReceivingResult:kTestQuery
                     response:kTestQueryEditedResponse
                         edit:@"\bedited"];
}

// Test that the correct page is loaded if the scanner result is a not supported
// URL.
- (void)testReceivingQRScannerLoadDataResult {
  [self doTestReceivingResult:kTestDataURL
              sanitizedResult:kTestSanitizedDataURL
                     response:kTestDataURLResponse
                         edit:nil];
}

@end
