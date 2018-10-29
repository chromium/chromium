// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AVFoundation/AVFoundation.h>
#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>

#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#include "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_url_loader.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_delegate.h"
#include "ios/chrome/browser/ui/qr_scanner/camera_controller.h"
#include "ios/chrome/browser/ui/qr_scanner/qr_scanner_view.h"
#include "ios/chrome/browser/ui/qr_scanner/qr_scanner_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/public/features.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/base/scoped_block_swizzler.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using namespace chrome_test_util;
using namespace qr_scanner;

namespace {

char kTestURL[] = "http://testurl";
char kTestURLResponse[] = "Test URL page";
char kTestQuery[] = "testquery";
char kTestQueryURL[] = "http://searchurl/testquery";
char kTestQueryResponse[] = "Test query page";

char kTestURLEdited[] = "http://testuredited";
char kTestURLEditedResponse[] = "Test URL edited page";
char kTestQueryEditedURL[] = "http://searchurl/testqueredited";
char kTestQueryEditedResponse[] = "Test query edited page";

// The GREYCondition timeout used for calls to waitWithTimeout:pollInterval:.
CFTimeInterval kGREYConditionTimeout = 5;
// The GREYCondition poll interval used for calls to
// waitWithTimeout:pollInterval:.
CFTimeInterval kGREYConditionPollInterval = 0.1;

// Returns the GREYMatcher for an element which is visible, interactable, and
// enabled.
id<GREYMatcher> VisibleInteractableEnabled() {
  return grey_allOf(grey_sufficientlyVisible(), grey_interactable(),
                    grey_enabled(), nil);
}

// Returns the GREYMatcher for the button that closes the QR Scanner.
id<GREYMatcher> QrScannerCloseButton() {
  return ButtonWithAccessibilityLabel(
      [[ChromeIcon closeIcon] accessibilityLabel]);
}

// Returns the GREYMatcher for the button which indicates that torch is off and
// which turns on the torch.
id<GREYMatcher> QrScannerTorchOffButton() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_QR_SCANNER_TORCH_BUTTON_ACCESSIBILITY_LABEL)),
                    grey_accessibilityValue(l10n_util::GetNSString(
                        IDS_IOS_QR_SCANNER_TORCH_OFF_ACCESSIBILITY_VALUE)),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Returns the GREYMatcher for the button which indicates that torch is on and
// which turns off the torch.
id<GREYMatcher> QrScannerTorchOnButton() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_QR_SCANNER_TORCH_BUTTON_ACCESSIBILITY_LABEL)),
                    grey_accessibilityValue(l10n_util::GetNSString(
                        IDS_IOS_QR_SCANNER_TORCH_ON_ACCESSIBILITY_VALUE)),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Returns the GREYMatcher for the QR Scanner viewport caption.
id<GREYMatcher> QrScannerViewportCaption() {
  return StaticTextWithAccessibilityLabelId(
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
      waitForElementWithMatcherSufficientlyVisible:chrome_test_util::Omnibox()];

  // Tap the QR Code scanner button in the keyboard accessory view.
  id<GREYMatcher> matcher =
      grey_allOf(grey_accessibilityLabel(@"QR code Search"),
                 grey_kindOfClass([UIButton class]), nil);

  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
}

// Taps the |button|.
void TapButton(id<GREYMatcher> button) {
  [[EarlGrey selectElementWithMatcher:button] performAction:grey_tap()];
}

// Appends the given |editText| to the |text| already in the omnibox and presses
// the keyboard return key.
void EditOmniboxTextAndTapKeyboardReturn(std::string text, NSString* editText) {
  [[EarlGrey selectElementWithMatcher:OmniboxText(text)]
      performAction:grey_typeText([editText stringByAppendingString:@"\n"])];
}

// Presses the keyboard return key.
void TapKeyboardReturnKeyInOmniboxWithText(std::string text) {
  [[EarlGrey selectElementWithMatcher:OmniboxText(text)]
      performAction:grey_typeText(@"\n")];
}

}  // namespace

@interface QRScannerViewControllerTestCase : ChromeTestCase {
  GURL _testURL;
  GURL _testURLEdited;
  GURL _testQuery;
  GURL _testQueryEdited;
}

@end

@implementation QRScannerViewControllerTestCase {
  // A swizzler for the CameraController method cameraControllerWithDelegate:.
  std::unique_ptr<ScopedBlockSwizzler> camera_controller_swizzler_;
  // A swizzler for the LocationBarCoordinator method
  // loadGURLFromLocationBar:transition:.
  std::unique_ptr<ScopedBlockSwizzler> load_GURL_from_location_bar_swizzler_;
}

+ (void)setUp {
  [super setUp];
  std::map<GURL, std::string> responses;
  responses[web::test::HttpServer::MakeUrl(kTestURL)] = kTestURLResponse;
  responses[web::test::HttpServer::MakeUrl(kTestQueryURL)] = kTestQueryResponse;
  responses[web::test::HttpServer::MakeUrl(kTestURLEdited)] =
      kTestURLEditedResponse;
  responses[web::test::HttpServer::MakeUrl(kTestQueryEditedURL)] =
      kTestQueryEditedResponse;
  web::test::SetUpSimpleHttpServer(responses);
}

- (void)setUp {
  [super setUp];
  _testURL = web::test::HttpServer::MakeUrl(kTestURL);
  _testURLEdited = web::test::HttpServer::MakeUrl(kTestURLEdited);
  _testQuery = web::test::HttpServer::MakeUrl(kTestQueryURL);
  _testQueryEdited = web::test::HttpServer::MakeUrl(kTestQueryEditedURL);
}

- (void)tearDown {
  [super tearDown];
  load_GURL_from_location_bar_swizzler_.reset();
  camera_controller_swizzler_.reset();
}

// Checks that the close button is visible, interactable, and enabled.
- (void)assertCloseButtonIsVisible {
  [[EarlGrey selectElementWithMatcher:QrScannerCloseButton()]
      assertWithMatcher:VisibleInteractableEnabled()];
}

// Checks that the close button is not visible.
- (void)assertCloseButtonIsNotVisible {
  [[EarlGrey selectElementWithMatcher:QrScannerCloseButton()]
      assertWithMatcher:grey_notVisible()];
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
      assertWithMatcher:grey_allOf(grey_sufficientlyVisible(),
                                   grey_not(grey_enabled()), nil)];
}

// Checks that the camera viewport caption is visible.
- (void)assertCameraViewportCaptionIsVisible {
  [[EarlGrey selectElementWithMatcher:QrScannerViewportCaption()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks that the close button, the camera preview, and the camera viewport
// caption are visible. If |torch| is YES, checks that the torch off button is
// visible, otherwise checks that the torch button is disabled. If |preview| is
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
  UIViewController* bvc = [self currentBVC];
  [self assertModalOfClass:[QRScannerViewController class]
          isNotPresentedBy:bvc];
  [self assertModalOfClass:[UIAlertController class] isNotPresentedBy:bvc];

  [self addCameraControllerInitializationExpectations:mock];
  ShowQRScanner();
  [self waitForModalOfClass:[QRScannerViewController class] toAppearAbove:bvc];
  [self assertQRScannerUIIsVisibleWithTorch:NO];
  [self assertModalOfClass:[UIAlertController class]
          isNotPresentedBy:[bvc presentedViewController]];
  [self assertModalOfClass:[UIAlertController class] isNotPresentedBy:bvc];
}

// Closes the QR scanner by tapping the close button and waits for it to
// disappear.
- (void)closeQRScannerWithCameraMock:(id)mock {
  [self addCameraControllerDismissalExpectations:mock];
  TapButton(QrScannerCloseButton());
  [self waitForModalOfClass:[QRScannerViewController class]
       toDisappearFromAbove:[self currentBVC]];
}

// Returns the current BrowserViewController.
- (UIViewController*)currentBVC {
  // TODO(crbug.com/629516): Evaluate moving this into a common utility.
  MainController* mainController = chrome_test_util::GetMainController();
  return [[mainController browserViewInformation] currentBVC];
}

// Checks that the omnibox is visible and contains |text|.
- (void)assertOmniboxIsVisibleWithText:(std::string)text {
  [[EarlGrey selectElementWithMatcher:OmniboxText(text)]
      assertWithMatcher:grey_notNil()];
}

#pragma mark helpers for dialogs

// Checks that the modal presented by |viewController| is of class |klass|.
- (void)assertModalOfClass:(Class)klass
             isPresentedBy:(UIViewController*)viewController {
  UIViewController* modal = [viewController presentedViewController];
  NSString* errorString = [NSString
      stringWithFormat:@"A modal of class %@ should be presented by %@.", klass,
                       [viewController class]];
  GREYAssertTrue(modal && [modal isKindOfClass:klass], errorString);
}

// Checks that the |viewController| is not presenting a modal, or that the modal
// presented by |viewController| is not of class |klass|.
- (void)assertModalOfClass:(Class)klass
          isNotPresentedBy:(UIViewController*)viewController {
  UIViewController* modal = [viewController presentedViewController];
  NSString* errorString = [NSString
      stringWithFormat:@"A modal of class %@ should not be presented by %@.",
                       klass, [viewController class]];
  GREYAssertTrue(!modal || ![modal isKindOfClass:klass], errorString);
}

// Checks that the modal presented by |viewController| is of class |klass| and
// waits for the modal's view to load.
- (void)waitForModalOfClass:(Class)klass
              toAppearAbove:(UIViewController*)viewController {
  [self assertModalOfClass:klass isPresentedBy:viewController];
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
                       klass];
  GREYAssertTrue(modalViewLoaded, errorString);
}

// Checks that the |viewController| is not presenting a modal, or that the modal
// presented by |viewController| is not of class |klass|. If a modal was
// previously presented, waits until it is dismissed.
- (void)waitForModalOfClass:(Class)klass
       toDisappearFromAbove:(UIViewController*)viewController {
  GREYCondition* modalViewDismissedCondition = [GREYCondition
      conditionWithName:@"modalViewDismissedCondition"
                  block:^BOOL {
                    UIViewController* modal =
                        [viewController presentedViewController];
                    return !modal || ![modal isKindOfClass:klass];
                  }];

  BOOL modalViewDismissed =
      [modalViewDismissedCondition waitWithTimeout:kGREYConditionTimeout
                                      pollInterval:kGREYConditionPollInterval];
  NSString* errorString = [NSString
      stringWithFormat:@"The modal of class %@ should be loaded.", klass];
  GREYAssertTrue(modalViewDismissed, errorString);
}

// Checks that the QRScannerViewController is presenting a UIAlertController and
// that the title of this alert corresponds to |state|.
- (void)assertQRScannerIsPresentingADialogForState:(CameraState)state {
  [self assertModalOfClass:[UIAlertController class]
             isPresentedBy:[[self currentBVC] presentedViewController]];
  [[EarlGrey
      selectElementWithMatcher:grey_text([self dialogTitleForState:state])]
      assertWithMatcher:grey_notNil()];
}

// Checks that there is no visible alert with title corresponding to |state|.
- (void)assertQRScannerIsNotPresentingADialogForState:(CameraState)state {
  [[EarlGrey
      selectElementWithMatcher:grey_text([self dialogTitleForState:state])]
      assertWithMatcher:grey_nil()];
}

// Returns the expected title for the dialog which is presented for |state|.
- (NSString*)dialogTitleForState:(CameraState)state {
  base::string16 appName = base::UTF8ToUTF16(version_info::GetProductName());
  switch (state) {
    case CAMERA_AVAILABLE:
    case CAMERA_NOT_LOADED:
      return nil;
    case CAMERA_IN_USE_BY_ANOTHER_APPLICATION:
      return l10n_util::GetNSString(
          IDS_IOS_QR_SCANNER_CAMERA_IN_USE_ALERT_TITLE);
    case CAMERA_PERMISSION_DENIED:
      return l10n_util::GetNSString(
          IDS_IOS_QR_SCANNER_CAMERA_PERMISSIONS_HELP_TITLE_GO_TO_SETTINGS);
    case CAMERA_UNAVAILABLE_DUE_TO_SYSTEM_PRESSURE:
    case CAMERA_UNAVAILABLE:
      return l10n_util::GetNSString(
          IDS_IOS_QR_SCANNER_CAMERA_UNAVAILABLE_ALERT_TITLE);
    case MULTIPLE_FOREGROUND_APPS:
      return l10n_util::GetNSString(
          IDS_IOS_QR_SCANNER_MULTIPLE_FOREGROUND_APPS_ALERT_TITLE);
  }
}

#pragma mark -
#pragma mark Helpers for mocks

// Swizzles the CameraController method cameraControllerWithDelegate: to return
// |cameraControllerMock| instead of a new instance of CameraController.
- (void)swizzleCameraController:(id)cameraControllerMock {
  CameraController* (^swizzleCameraControllerBlock)(
      id<CameraControllerDelegate>) = ^(id<CameraControllerDelegate> delegate) {
    return cameraControllerMock;
  };

  camera_controller_swizzler_.reset(new ScopedBlockSwizzler(
      [CameraController class], @selector(cameraControllerWithDelegate:),
      swizzleCameraControllerBlock));
}

// Swizzles the LocationBarCoordinator loadGURLFromLocationBarBlock:transition:
// method to load |searchURL| instead of the generated search URL.
- (void)swizzleLocationBarCoordinatorLoadGURLFromLocationBar:
    (const GURL&)replacementURL {
  // The specific class to swizzle depends on whether the UIRefresh experiment
  // is enabled.
    void (^loadGURLFromLocationBarBlock)(LocationBarCoordinator*, const GURL&,
                                         ui::PageTransition) =
        ^void(LocationBarCoordinator* self, const GURL& url,
              ui::PageTransition transition) {
          web::NavigationManager::WebLoadParams params(replacementURL);
          params.transition_type = transition;
          [self.URLLoader loadURLWithParams:params];
          [self cancelOmniboxEdit];
        };
    load_GURL_from_location_bar_swizzler_.reset(
        new ScopedBlockSwizzler([LocationBarCoordinator class],
                                @selector(loadGURLFromLocationBar:transition:),
                                loadGURLFromLocationBarBlock));
}

// Creates a new CameraController mock with camera permission granted if
// |granted| is set to YES.
- (id)getCameraControllerMockWithAuthorizationStatus:
    (AVAuthorizationStatus)authorizationStatus {
  id mock = [OCMockObject mockForClass:[CameraController class]];
  [[[mock stub] andReturnValue:OCMOCK_VALUE(authorizationStatus)]
      getAuthorizationStatus];
  return mock;
}

#pragma mark delegate calls

// Calls |cameraStateChanged:| on the presented QRScannerViewController.
- (void)callCameraStateChanged:(CameraState)state {
  QRScannerViewController* vc =
      (QRScannerViewController*)[[self currentBVC] presentedViewController];
  [vc cameraStateChanged:state];
}

// Calls |torchStateChanged:| on the presented QRScannerViewController.
- (void)callTorchStateChanged:(BOOL)torchIsOn {
  QRScannerViewController* vc =
      (QRScannerViewController*)[[self currentBVC] presentedViewController];
  [vc torchStateChanged:torchIsOn];
}

// Calls |torchAvailabilityChanged:| on the presented QRScannerViewController.
- (void)callTorchAvailabilityChanged:(BOOL)torchIsAvailable {
  QRScannerViewController* vc =
      (QRScannerViewController*)[[self currentBVC] presentedViewController];
  [vc torchAvailabilityChanged:torchIsAvailable];
}

// Calls |receiveQRScannerResult:| on the presented QRScannerViewController.
- (void)callReceiveQRScannerResult:(NSString*)result {
  QRScannerViewController* vc =
      (QRScannerViewController*)[[self currentBVC] presentedViewController];
  [vc receiveQRScannerResult:result loadImmediately:NO];
}

#pragma mark expectations

// Adds functions which are expected to be called when the
// QRScannerViewController is presented to |cameraControllerMock|.
- (void)addCameraControllerInitializationExpectations:(id)cameraControllerMock {
  [[cameraControllerMock expect] setTorchMode:AVCaptureTorchModeOff];
  [[cameraControllerMock expect] loadCaptureSession:[OCMArg any]];
  [[cameraControllerMock expect] startRecording];
}

// Adds functions which are expected to be called when the
// QRScannerViewController is dismissed to |cameraControllerMock|.
- (void)addCameraControllerDismissalExpectations:(id)cameraControllerMock {
  [[cameraControllerMock expect] setTorchMode:AVCaptureTorchModeOff];
  [[cameraControllerMock expect] stopRecording];
}

// Adds functions which are expected to be called when the torch is switched on
// to |cameraControllerMock|.
- (void)addCameraControllerTorchOnExpectations:(id)cameraControllerMock {
  [[[cameraControllerMock expect] andReturnValue:@NO] isTorchActive];
  [[cameraControllerMock expect] setTorchMode:AVCaptureTorchModeOn];
}

// Adds functions which are expected to be called when the torch is switched off
// to |cameraControllerMock|.
- (void)addCameraControllerTorchOffExpectations:(id)cameraControllerMock {
  [[[cameraControllerMock expect] andReturnValue:@YES] isTorchActive];
  [[cameraControllerMock expect] setTorchMode:AVCaptureTorchModeOff];
}

#pragma mark -
#pragma mark Tests

// Tests that the close button, camera preview, viewport caption, and the torch
// button are visible if the camera is available. The preview is delayed.
- (void)testQRScannerUIIsShown {
  id cameraControllerMock =
      [self getCameraControllerMockWithAuthorizationStatus:
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
      [self getCameraControllerMockWithAuthorizationStatus:
                AVAuthorizationStatusAuthorized];
  [self swizzleCameraController:cameraControllerMock];

  // Open the QR scanner.
  [self showQRScannerAndCheckLayoutWithCameraMock:cameraControllerMock];

  // Torch becomes available.
  [self callTorchAvailabilityChanged:YES];
  [self assertQRScannerUIIsVisibleWithTorch:YES];

  // Turn torch on.
  [self addCameraControllerTorchOnExpectations:cameraControllerMock];
  [self assertTorchOffButtonIsVisible];
  TapButton(QrScannerTorchOffButton());
  [self assertTorchOffButtonIsVisible];

  // Torch becomes active.
  [self callTorchStateChanged:YES];
  [self assertTorchOnButtonIsVisible];

  // Turn torch off.
  [self addCameraControllerTorchOffExpectations:cameraControllerMock];
  TapButton(QrScannerTorchOnButton());
  [self assertTorchOnButtonIsVisible];

  // Torch becomes inactive.
  [self callTorchStateChanged:NO];
  [self assertTorchOffButtonIsVisible];

  // Close the QR scanner.
  [self closeQRScannerWithCameraMock:cameraControllerMock];
  [cameraControllerMock verify];
}

// Tests that if the QR scanner is closed while the torch is on, the torch is
// switched off and the correct button indicating that the torch is off is shown
// when the scanner is opened again.
- (void)testTorchButtonIsResetWhenQRScannerIsReopened {
// TODO(crbug.com/869176): Re-enable this test on iOS 10 iPad device.
#if !TARGET_IPHONE_SIMULATOR
  if (!base::ios::IsRunningOnIOS11OrLater() && IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 10 iPad device.");
  }
#endif

  id cameraControllerMock =
      [self getCameraControllerMockWithAuthorizationStatus:
                AVAuthorizationStatusAuthorized];
  [self swizzleCameraController:cameraControllerMock];

  // Open the QR scanner.
  [self showQRScannerAndCheckLayoutWithCameraMock:cameraControllerMock];
  [self assertQRScannerUIIsVisibleWithTorch:NO];
  [self callTorchAvailabilityChanged:YES];
  [self assertQRScannerUIIsVisibleWithTorch:YES];

  // Turn torch on.
  [self addCameraControllerTorchOnExpectations:cameraControllerMock];
  TapButton(QrScannerTorchOffButton());
  [self callTorchStateChanged:YES];
  [self assertTorchOnButtonIsVisible];

  // Close the QR scanner.
  [self closeQRScannerWithCameraMock:cameraControllerMock];

  // Reopen the QR scanner.
  [self showQRScannerAndCheckLayoutWithCameraMock:cameraControllerMock];
  [self callTorchAvailabilityChanged:YES];
  [self assertTorchOffButtonIsVisible];

  // Close the QR scanner again.
  [self closeQRScannerWithCameraMock:cameraControllerMock];
  [cameraControllerMock verify];
}

// Tests that the torch button is disabled when the camera reports that torch
// became unavailable.
- (void)testTorchButtonIsDisabledWhenTorchBecomesUnavailable {
// TODO(crbug.com/869176): Re-enable this test on iOS 10 iPad device.
#if !TARGET_IPHONE_SIMULATOR
  if (!base::ios::IsRunningOnIOS11OrLater() && IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 10 iPad device.");
  }
#endif
  id cameraControllerMock =
      [self getCameraControllerMockWithAuthorizationStatus:
                AVAuthorizationStatusAuthorized];
  [self swizzleCameraController:cameraControllerMock];

  // Open the QR scanner.
  [self showQRScannerAndCheckLayoutWithCameraMock:cameraControllerMock];

  // Torch becomes available.
  [self callTorchAvailabilityChanged:YES];
  [self assertQRScannerUIIsVisibleWithTorch:YES];

  // Torch becomes unavailable.
  [self callTorchAvailabilityChanged:NO];
  [self assertQRScannerUIIsVisibleWithTorch:NO];

  // Close the QR scanner.
  [self closeQRScannerWithCameraMock:cameraControllerMock];
  [cameraControllerMock verify];
}

#pragma mark dialogs

// Tests that a UIAlertController is presented instead of the
// QRScannerViewController if the camera is unavailable.
- (void)testCameraUnavailableDialog {
  UIViewController* bvc = [self currentBVC];
  [self assertModalOfClass:[QRScannerViewController class]
          isNotPresentedBy:bvc];
  [self assertModalOfClass:[UIAlertController class] isNotPresentedBy:bvc];
  id cameraControllerMock =
      [self getCameraControllerMockWithAuthorizationStatus:
                AVAuthorizationStatusDenied];
  [self swizzleCameraController:cameraControllerMock];

  ShowQRScanner();
  [self assertModalOfClass:[QRScannerViewController class]
          isNotPresentedBy:bvc];
  [self waitForModalOfClass:[UIAlertController class] toAppearAbove:bvc];

  TapButton(DialogCancelButton());
  [self waitForModalOfClass:[UIAlertController class] toDisappearFromAbove:bvc];
}

// Tests that a UIAlertController is presented by the QRScannerViewController if
// the camera state changes after the QRScannerViewController is presented.
- (void)testDialogIsDisplayedIfCameraStateChanges {
// TODO(crbug.com/869176): Re-enable this test on iOS 10 iPad device.
#if !TARGET_IPHONE_SIMULATOR
  if (!base::ios::IsRunningOnIOS11OrLater() && IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 10 iPad device.");
  }
#endif
  id cameraControllerMock =
      [self getCameraControllerMockWithAuthorizationStatus:
                AVAuthorizationStatusAuthorized];
  [self swizzleCameraController:cameraControllerMock];

  std::vector<CameraState> tests{MULTIPLE_FOREGROUND_APPS, CAMERA_UNAVAILABLE,
                                 CAMERA_PERMISSION_DENIED,
                                 CAMERA_IN_USE_BY_ANOTHER_APPLICATION};

  for (const CameraState& state : tests) {
    [self showQRScannerAndCheckLayoutWithCameraMock:cameraControllerMock];
    [self callCameraStateChanged:state];
    [self assertQRScannerIsPresentingADialogForState:state];

    // Close the dialog.
    [self addCameraControllerDismissalExpectations:cameraControllerMock];
    TapButton(DialogCancelButton());
    UIViewController* bvc = [self currentBVC];
    [self waitForModalOfClass:[QRScannerViewController class]
         toDisappearFromAbove:bvc];
    [self assertModalOfClass:[UIAlertController class] isNotPresentedBy:bvc];
  }

  [cameraControllerMock verify];
}

// Tests that a new dialog replaces an old dialog if the camera state changes.
- (void)testDialogIsReplacedIfCameraStateChanges {
  id cameraControllerMock =
      [self getCameraControllerMockWithAuthorizationStatus:
                AVAuthorizationStatusAuthorized];
  [self swizzleCameraController:cameraControllerMock];

  // Change state to CAMERA_UNAVAILABLE.
  CameraState currentState = CAMERA_UNAVAILABLE;
  [self showQRScannerAndCheckLayoutWithCameraMock:cameraControllerMock];
  [self callCameraStateChanged:currentState];
  [self assertQRScannerIsPresentingADialogForState:currentState];

  std::vector<CameraState> tests{
      CAMERA_PERMISSION_DENIED, MULTIPLE_FOREGROUND_APPS,
      CAMERA_IN_USE_BY_ANOTHER_APPLICATION, CAMERA_UNAVAILABLE};

  for (const CameraState& state : tests) {
    [self callCameraStateChanged:state];
    [self assertQRScannerIsPresentingADialogForState:state];
    [self assertQRScannerIsNotPresentingADialogForState:currentState];
    currentState = state;
  }

  // Cancel the dialog.
  [self addCameraControllerDismissalExpectations:cameraControllerMock];
  TapButton(DialogCancelButton());
  [self waitForModalOfClass:[QRScannerViewController class]
       toDisappearFromAbove:[self currentBVC]];
  [self assertModalOfClass:[UIAlertController class]
          isNotPresentedBy:[self currentBVC]];

  [cameraControllerMock verify];
}

// Tests that an error dialog is dismissed if the camera becomes available.
- (void)testDialogDismissedIfCameraBecomesAvailable {
// TODO(crbug.com/869176): Re-enable this test on iOS 10 iPad device.
#if !TARGET_IPHONE_SIMULATOR
  if (!base::ios::IsRunningOnIOS11OrLater() && IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 10 iPad device.");
  }
#endif

  id cameraControllerMock =
      [self getCameraControllerMockWithAuthorizationStatus:
                AVAuthorizationStatusAuthorized];
  [self swizzleCameraController:cameraControllerMock];

  std::vector<CameraState> tests{CAMERA_IN_USE_BY_ANOTHER_APPLICATION,
                                 CAMERA_UNAVAILABLE, MULTIPLE_FOREGROUND_APPS,
                                 CAMERA_PERMISSION_DENIED};

  for (const CameraState& state : tests) {
    [self showQRScannerAndCheckLayoutWithCameraMock:cameraControllerMock];
    [self callCameraStateChanged:state];
    [self assertQRScannerIsPresentingADialogForState:state];

    // Change state to CAMERA_AVAILABLE.
    [self callCameraStateChanged:CAMERA_AVAILABLE];
    [self assertQRScannerIsNotPresentingADialogForState:state];
    [self closeQRScannerWithCameraMock:cameraControllerMock];
  }

  [cameraControllerMock verify];
}

#pragma mark scanned result

// A helper function for testing that the view controller correctly passes the
// received results to its delegate and that pages can be loaded. The result
// received from the camera controller is in |result|, |response| is the
// expected response on the loaded page, and |editString| is a nullable string
// which can be appended to the response in the omnibox before the page is
// loaded.
- (void)doTestReceivingResult:(std::string)result
                     response:(std::string)response
                         edit:(NSString*)editString {
  id cameraControllerMock =
      [self getCameraControllerMockWithAuthorizationStatus:
                AVAuthorizationStatusAuthorized];
  [self swizzleCameraController:cameraControllerMock];

  // Open the QR scanner.
  [self showQRScannerAndCheckLayoutWithCameraMock:cameraControllerMock];
  [self callTorchAvailabilityChanged:YES];
  [self assertQRScannerUIIsVisibleWithTorch:YES];

  // Receive a scanned result from the camera.
  [self addCameraControllerDismissalExpectations:cameraControllerMock];
  [self callReceiveQRScannerResult:base::SysUTF8ToNSString(result)];

  [self waitForModalOfClass:[QRScannerViewController class]
       toDisappearFromAbove:[self currentBVC]];
  [cameraControllerMock verify];

  // Optionally edit the text in the omnibox before pressing return.
  [self assertOmniboxIsVisibleWithText:result];
  if (editString != nil) {
    EditOmniboxTextAndTapKeyboardReturn(result, editString);
  } else {
    TapKeyboardReturnKeyInOmniboxWithText(result);
  }
  [ChromeEarlGrey waitForWebViewContainingText:response];

  // Press the back button to get back to the NTP.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [self assertModalOfClass:[QRScannerViewController class]
          isNotPresentedBy:[self currentBVC]];
}

// Test that the correct page is loaded if the scanner result is a URL.
- (void)testReceivingQRScannerURLResult {
// TODO(crbug.com/869176): Re-enable this test on iOS 10 iPad device.
#if !TARGET_IPHONE_SIMULATOR
  if (!base::ios::IsRunningOnIOS11OrLater() && IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 10 iPad device.");
  }
#endif
  [self doTestReceivingResult:_testURL.GetContent()
                     response:kTestURLResponse
                         edit:nil];
}

// Test that the correct page is loaded if the scanner result is a URL which is
// then manually edited.
- (void)testReceivingQRScannerURLResultAndEditingTheURL {
// TODO(crbug.com/869176): Re-enable this test on iOS 10 iPad device.
#if !TARGET_IPHONE_SIMULATOR
  if (!base::ios::IsRunningOnIOS11OrLater() && IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 10 iPad device.");
  }
#endif
  // TODO(crbug.com/753098): Re-enable this test on iOS 11 iPad once
  // grey_typeText works on iOS 11.
  if (base::ios::IsRunningOnIOS11OrLater() && IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 11.");
  }

  [self doTestReceivingResult:_testURL.GetContent()
                     response:kTestURLEditedResponse
                         edit:@"\b\bedited/"];
}

// Test that the correct page is loaded if the scanner result is a search query.
- (void)testReceivingQRScannerSearchQueryResult {
  [self swizzleLocationBarCoordinatorLoadGURLFromLocationBar:_testQuery];
  [self doTestReceivingResult:kTestQuery response:kTestQueryResponse edit:nil];
}

// Test that the correct page is loaded if the scanner result is a search query
// which is then manually edited.
- (void)testReceivingQRScannerSearchQueryResultAndEditingTheQuery {
// TODO(crbug.com/869176): Re-enable this test on iOS 10 iPad device.
#if !TARGET_IPHONE_SIMULATOR
  if (!base::ios::IsRunningOnIOS11OrLater() && IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 10 iPad device.");
  }
#endif
  // TODO(crbug.com/753098): Re-enable this test on iOS 11 iPad once
  // grey_typeText works on iOS 11.
  if (base::ios::IsRunningOnIOS11OrLater() && IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 11.");
  }

  [self swizzleLocationBarCoordinatorLoadGURLFromLocationBar:_testQueryEdited];
  [self doTestReceivingResult:kTestQuery
                     response:kTestQueryEditedResponse
                         edit:@"\bedited"];
}

@end
