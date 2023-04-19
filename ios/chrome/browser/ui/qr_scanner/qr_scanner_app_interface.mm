// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/qr_scanner/qr_scanner_app_interface.h"

#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/search_engines/template_url_service.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_provider.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_url_loader.h"
#import "ios/chrome/browser/ui/qr_scanner/qr_scanner_camera_controller.h"
#import "ios/chrome/browser/ui/qr_scanner/qr_scanner_view_controller.h"
#import "ios/chrome/browser/ui/scanner/camera_controller.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/testing/nserror_util.h"
#import "net/base/mac/url_conversions.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using scanner::CameraState;

@implementation QRScannerAppInterface

+ (UIViewController*)currentBrowserViewController {
  MainController* mainController = chrome_test_util::GetMainController();
  return mainController.browserProviderInterface.mainBrowserProvider
      .viewController;
}

+ (NSString*)closeIconAccessibilityLabel {
  return [ChromeIcon closeIcon].accessibilityLabel;
}

#pragma mark Swizzling

+ (id)cameraControllerSwizzleBlockWithMock:(id)cameraControllerMock {
  QRScannerCameraController* (^swizzleCameraControllerBlock)(
      id<QRScannerCameraControllerDelegate>) =
      ^(id<QRScannerCameraControllerDelegate> delegate) {
        return cameraControllerMock;
      };

  return swizzleCameraControllerBlock;
}

#pragma mark Search engine override

+ (void)overrideSearchEngine:(NSString*)templateURL {
  TemplateURLData data;
  data.SetShortName(u"testSearchEngine");
  data.SetKeyword(u"testSearchEngine");
  GURL searchableURL(base::SysNSStringToUTF8(templateURL));
  data.SetURL(searchableURL.possibly_invalid_spec());
  data.favicon_url = TemplateURL::GenerateFaviconURL(searchableURL);
  data.last_visited = base::Time::Now();

  TemplateURLService* service =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  TemplateURL* url = service->Add(std::make_unique<TemplateURL>(data));
  service->SetUserSelectedDefaultSearchProvider(url);
}

+ (void)resetSearchEngine {
  TemplateURLService* service =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  TemplateURL* templateURL = service->GetTemplateURLForHost("google.com");
  service->SetUserSelectedDefaultSearchProvider(templateURL);
}

#pragma mark Mocking and Expectations

+ (id)cameraControllerMockWithAuthorizationStatus:
    (AVAuthorizationStatus)authorizationStatus {
  id mock = [OCMockObject mockForClass:[QRScannerCameraController class]];
  [[[mock stub] andReturnValue:OCMOCK_VALUE(authorizationStatus)]
      authorizationStatus];
  return mock;
}

+ (void)addCameraControllerInitializationExpectations:(id)cameraControllerMock {
  [[cameraControllerMock expect] setTorchMode:AVCaptureTorchModeOff];
  [[cameraControllerMock expect] loadCaptureSession:[OCMArg any]];
  [[cameraControllerMock expect] startRecording];
}

+ (void)addCameraControllerDismissalExpectations:(id)cameraControllerMock {
  [[cameraControllerMock expect] setTorchMode:AVCaptureTorchModeOff];
  [[cameraControllerMock expect] stopRecording];
}

+ (void)addCameraControllerTorchOnExpectations:(id)cameraControllerMock {
  [[[cameraControllerMock expect] andReturnValue:@NO] isTorchActive];
  [[cameraControllerMock expect] setTorchMode:AVCaptureTorchModeOn];
}

+ (void)addCameraControllerTorchOffExpectations:(id)cameraControllerMock {
  [[[cameraControllerMock expect] andReturnValue:@YES] isTorchActive];
  [[cameraControllerMock expect] setTorchMode:AVCaptureTorchModeOff];
}

#pragma mark CameraControllerDelegate calls

+ (void)callCameraStateChanged:(CameraState)state {
  QRScannerViewController* vc = (QRScannerViewController*)
      [self.currentBrowserViewController presentedViewController];
  [vc cameraStateChanged:state];
}

+ (void)callTorchStateChanged:(BOOL)torchIsOn {
  QRScannerViewController* vc = (QRScannerViewController*)
      [self.currentBrowserViewController presentedViewController];
  [vc torchStateChanged:torchIsOn];
}

+ (void)callTorchAvailabilityChanged:(BOOL)torchIsAvailable {
  QRScannerViewController* vc = (QRScannerViewController*)
      [self.currentBrowserViewController presentedViewController];
  [vc torchAvailabilityChanged:torchIsAvailable];
}

+ (void)callReceiveQRScannerResult:(NSString*)result {
  QRScannerViewController* vc = (QRScannerViewController*)
      [self.currentBrowserViewController presentedViewController];
  [vc receiveQRScannerResult:result loadImmediately:NO];
}

#pragma mark Modal helpers for dialogs

+ (NSError*)assertModalOfClass:(NSString*)className
                 isPresentedBy:(UIViewController*)viewController {
  Class klass = NSClassFromString(className);
  UIViewController* modal = [viewController presentedViewController];
  NSString* errorString = [NSString
      stringWithFormat:@"A modal of class %@ should be presented by %@.", klass,
                       [viewController class]];
  BOOL condition = modal && [modal isKindOfClass:klass];
  if (!condition) {
    return testing::NSErrorWithLocalizedDescription(errorString);
  }
  return nil;
}

+ (NSError*)assertModalOfClass:(NSString*)className
              isNotPresentedBy:(UIViewController*)viewController {
  Class klass = NSClassFromString(className);
  UIViewController* modal = [viewController presentedViewController];
  NSString* errorString = [NSString
      stringWithFormat:@"A modal of class %@ should not be presented by %@.",
                       klass, [viewController class]];
  BOOL condition = !modal || ![modal isKindOfClass:klass];
  if (!condition) {
    return testing::NSErrorWithLocalizedDescription(errorString);
  }
  return nil;
}

+ (BOOL (^)())blockForWaitingForModalOfClass:(NSString*)className
                        toDisappearFromAbove:(UIViewController*)viewController {
  Class klass = NSClassFromString(className);
  BOOL (^waitingBlock)() = ^BOOL {
    UIViewController* modal = [viewController presentedViewController];
    return !modal || ![modal isKindOfClass:klass];
  };
  return waitingBlock;
}

// Returns the expected title for the dialog which is presented for `state`.
+ (NSString*)dialogTitleForState:(CameraState)state {
  std::u16string appName = base::UTF8ToUTF16(version_info::GetProductName());
  switch (state) {
    case scanner::CAMERA_AVAILABLE:
    case scanner::CAMERA_NOT_LOADED:
      return nil;
    case scanner::CAMERA_IN_USE_BY_ANOTHER_APPLICATION:
      return l10n_util::GetNSString(
          IDS_IOS_QR_SCANNER_CAMERA_IN_USE_ALERT_TITLE);
    case scanner::CAMERA_PERMISSION_DENIED:
      return l10n_util::GetNSString(
          IDS_IOS_SCANNER_CAMERA_PERMISSIONS_HELP_TITLE_GO_TO_SETTINGS);
    case scanner::CAMERA_UNAVAILABLE_DUE_TO_SYSTEM_PRESSURE:
    case scanner::CAMERA_UNAVAILABLE:
      return l10n_util::GetNSString(
          IDS_IOS_QR_SCANNER_CAMERA_UNAVAILABLE_ALERT_TITLE);
    case scanner::MULTIPLE_FOREGROUND_APPS:
      return l10n_util::GetNSString(
          IDS_IOS_QR_SCANNER_MULTIPLE_FOREGROUND_APPS_ALERT_TITLE);
  }
}

#pragma mark VoiceOver Overrides

+ (void)overrideVoiceOverCheckForQRScannerViewController:
            (UIViewController*)qrScanner
                                                    isOn:(BOOL)isOn {
  QRScannerViewController* qrScannerViewController =
      base::mac::ObjCCast<QRScannerViewController>(qrScanner);
  [qrScannerViewController overrideVoiceOverCheck:isOn];
}

+ (void)postScanEndVoiceoverAnnouncement {
  NSString* scannedAnnouncement = l10n_util::GetNSString(
      IDS_IOS_SCANNER_SCANNED_ACCESSIBILITY_ANNOUNCEMENT);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIAccessibilityAnnouncementDidFinishNotification
                    object:nil
                  userInfo:@{
                    UIAccessibilityAnnouncementKeyStringValue :
                        scannedAnnouncement
                  }];
}

@end
