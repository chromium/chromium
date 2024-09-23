// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui_bundled/ar_quick_look_coordinator.h"

#import <ARKit/ARKit.h>
#import <QuickLook/QuickLook.h>

#import <memory>

#import "base/ios/block_types.h"
#import "base/metrics/histogram_functions.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/download/model/ar_quick_look_tab_helper.h"
#import "ios/chrome/browser/download/model/ar_quick_look_tab_helper_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/web_state_list/model/web_state_dependency_installer_bridge.h"

const char kIOSPresentQLPreviewControllerHistogram[] =
    "Download.IOSPresentQLPreviewControllerResult";

namespace {

// Returns an enum for Download.IOSPresentQLPreviewControllerResult.
PresentQLPreviewController GetHistogramEnum(
    UIViewController* base_view_controller,
    bool is_file_valid) {
  if (!is_file_valid) {
    return PresentQLPreviewController::kInvalidFile;
  }

  if (!base_view_controller.presentedViewController) {
    return PresentQLPreviewController::kSuccessful;
  }

  if ([base_view_controller.presentedViewController
          isKindOfClass:[QLPreviewController class]]) {
    return PresentQLPreviewController::kAnotherQLPreviewControllerIsPresented;
  }

  return PresentQLPreviewController::kAnotherViewControllerIsPresented;
}

}  // namespace

// Helper class that acts as delegate and data source for the presented
// QLPreviewController. It informs the WebState of its visibility changes
// (as the USDZ preview will cover the WebState).
@interface ARQuickLookPreviewControllerDelegate
    : NSObject <QLPreviewControllerDataSource, QLPreviewControllerDelegate>

- (instancetype)initWithWebState:(web::WebState*)webState
                       sourceURL:(NSURL*)sourceURL
                    canonicalURL:(NSURL*)canonicalURL
                    allowScaling:(BOOL)allowScaling
                    dismissBlock:(ProceduralBlock)dismissBlock
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)viewPresented;

@end

@implementation ARQuickLookPreviewControllerDelegate {
  base::WeakPtr<web::WebState> _weakWebState;
  NSURL* _sourceURL;
  NSURL* _canonicalURL;
  BOOL _allowScaling;
  ProceduralBlock _dismissBlock;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                       sourceURL:(NSURL*)sourceURL
                    canonicalURL:(NSURL*)canonicalURL
                    allowScaling:(BOOL)allowScaling
                    dismissBlock:(ProceduralBlock)dismissBlock {
  if ((self = [super init])) {
    DCHECK(webState);
    DCHECK(sourceURL);
    _weakWebState = webState->GetWeakPtr();
    _sourceURL = sourceURL;
    _canonicalURL = canonicalURL;
    _allowScaling = allowScaling;
    _dismissBlock = dismissBlock;
  }
  return self;
}

- (void)viewPresented {
  web::WebState* webState = _weakWebState.get();
  if (webState)
    webState->DidCoverWebContent();
}

#pragma mark - QLPreviewControllerDataSource

- (NSInteger)numberOfPreviewItemsInPreviewController:
    (QLPreviewController*)controller {
  return 1;
}

- (id<QLPreviewItem>)previewController:(QLPreviewController*)controller
                    previewItemAtIndex:(NSInteger)index {
  ARQuickLookPreviewItem* item =
      [[ARQuickLookPreviewItem alloc] initWithFileAtURL:_sourceURL];
  item.allowsContentScaling = _allowScaling;
  item.canonicalWebPageURL = _canonicalURL;
  return item;
}

#pragma mark - QLPreviewControllerDelegate

- (void)previewControllerDidDismiss:(QLPreviewController*)controller {
  web::WebState* webState = _weakWebState.get();
  if (webState)
    webState->DidRevealWebContent();

  if (_dismissBlock) {
    _dismissBlock();
  }
}

@end

@interface ARQuickLookCoordinator () <DependencyInstalling,
                                      ARQuickLookTabHelperDelegate> {
  // Bridge which observes WebStateList and alerts this coordinator when this
  // needs to register the Mediator with a new WebState.
  std::unique_ptr<WebStateDependencyInstallerBridge> _dependencyInstallerBridge;
  // The delegate passed to the QLPreviewController. It informs the WebState
  // that it may be hidden (during the presentation of the USDZ file) and it
  // serves as a data source for the preview controller.
  ARQuickLookPreviewControllerDelegate* _delegate;
}

@end

@implementation ARQuickLookCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser {
  if ((self = [super initWithBaseViewController:baseViewController
                                        browser:browser])) {
    _dependencyInstallerBridge =
        std::make_unique<WebStateDependencyInstallerBridge>(
            self, browser->GetWebStateList());
  }
  return self;
}

- (void)stop {
  // Reset this observer manually. We want this to go out of scope now, to
  // ensure it detaches before `browser` and its WebStateList get destroyed.
  _dependencyInstallerBridge.reset();

  _delegate = nil;
}

#pragma mark - DependencyInstalling methods

- (void)installDependencyForWebState:(web::WebState*)webState {
  ARQuickLookTabHelper::GetOrCreateForWebState(webState)->set_delegate(self);
}

- (void)uninstallDependencyForWebState:(web::WebState*)webState {
  ARQuickLookTabHelper::GetOrCreateForWebState(webState)->set_delegate(nil);
}

#pragma mark - ARQuickLookTabHelperDelegate

- (void)presentUSDZFileWithURL:(NSURL*)fileURL
                  canonicalURL:(NSURL*)canonicalURL
                      webState:(web::WebState*)webState
           allowContentScaling:(BOOL)allowContentScaling {
  base::UmaHistogramEnumeration(
      kIOSPresentQLPreviewControllerHistogram,
      GetHistogramEnum(self.baseViewController, fileURL));

  // Do not present if the URL is invalid or if there is already
  // a preview in progress.
  if (!fileURL || _delegate)
    return;

  __weak ARQuickLookCoordinator* weakSelf = self;
  _delegate = [[ARQuickLookPreviewControllerDelegate alloc]
      initWithWebState:webState
             sourceURL:fileURL
          canonicalURL:canonicalURL
          allowScaling:allowContentScaling
          dismissBlock:^{
            [weakSelf previewDismissed];
          }];

  QLPreviewController* viewController = [[QLPreviewController alloc] init];
  viewController.dataSource = _delegate;
  viewController.delegate = _delegate;

  __weak ARQuickLookPreviewControllerDelegate* weakDelegate = _delegate;
  [self.baseViewController presentViewController:viewController
                                        animated:YES
                                      completion:^{
                                        [weakDelegate viewPresented];
                                      }];
}

#pragma mark - Private

- (void)previewDismissed {
  _delegate = nil;
}

@end
