// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/ar_quick_look_coordinator.h"

#import <ARKit/ARKit.h>
#import <QuickLook/QuickLook.h>

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#import "ios/chrome/browser/download/ar_quick_look_tab_helper.h"
#import "ios/chrome/browser/download/ar_quick_look_tab_helper_delegate.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/web_state_list/web_state_dependency_installer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

@interface ARQuickLookCoordinator () <DependencyInstalling,
                                      ARQuickLookTabHelperDelegate,
                                      QLPreviewControllerDataSource,
                                      QLPreviewControllerDelegate> {
  // Bridge which observes WebStateList and alerts this coordinator when this
  // needs to register the Mediator with a new WebState.
  std::unique_ptr<WebStateDependencyInstallerBridge> _dependencyInstallerBridge;
}

// The file URL pointing to the downloaded USDZ format file.
@property(nonatomic, copy) NSURL* fileURL;

// Displays USDZ format files. Set as a weak reference so it only exists while
// its being presented by baseViewController.
@property(nonatomic, weak) QLPreviewController* viewController;

@property(nonatomic, assign) BOOL allowsContentScaling;

@property(nonatomic, assign) web::WebState* webState;

@end

@implementation ARQuickLookCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser {
  if (self = [super initWithBaseViewController:baseViewController
                                       browser:browser]) {
    _dependencyInstallerBridge =
        std::make_unique<WebStateDependencyInstallerBridge>(
            self, browser->GetWebStateList());
  }
  return self;
}

- (void)stop {
  // Reset this observer manually. We want this to go out of scope now, to
  // ensure it detaches before |browser| and its WebStateList get destroyed.
  _dependencyInstallerBridge.reset();

  self.viewController = nil;
  self.fileURL = nil;
}

#pragma mark - DependencyInstalling methods

- (void)installDependencyForWebState:(web::WebState*)webState {
  if (ARQuickLookTabHelper::FromWebState(webState)) {
    ARQuickLookTabHelper::FromWebState(webState)->set_delegate(self);
  }
}

- (void)uninstallDependencyForWebState:(web::WebState*)webState {
  if (ARQuickLookTabHelper::FromWebState(webState)) {
    ARQuickLookTabHelper::FromWebState(webState)->set_delegate(nil);
  }
}

#pragma mark - ARQuickLookTabHelperDelegate

- (void)ARQuickLookTabHelper:(ARQuickLookTabHelper*)tabHelper
    didFinishDowloadingFileWithURL:(NSURL*)fileURL
              allowsContentScaling:(BOOL)allowsScaling {
  self.fileURL = fileURL;
  self.allowsContentScaling = allowsScaling;

  base::UmaHistogramEnumeration(
      kIOSPresentQLPreviewControllerHistogram,
      GetHistogramEnum(self.baseViewController, self.fileURL));

  // QLPreviewController should not be presented if the file URL is nil.
  if (!self.fileURL) {
    return;
  }

  QLPreviewController* viewController = [[QLPreviewController alloc] init];
  viewController.dataSource = self;
  viewController.delegate = self;
  self.webState = tabHelper->web_state();
  __weak __typeof(self) weakSelf = self;

  [self.baseViewController
      presentViewController:viewController
                   animated:YES
                 completion:^{
                   if (weakSelf.webState)
                     weakSelf.webState->DidCoverWebContent();
                 }];
  self.viewController = viewController;
}

#pragma mark - QLPreviewControllerDataSource

- (NSInteger)numberOfPreviewItemsInPreviewController:
    (QLPreviewController*)controller {
  return 1;
}

- (id<QLPreviewItem>)previewController:(QLPreviewController*)controller
                    previewItemAtIndex:(NSInteger)index {
  ARQuickLookPreviewItem* item =
      [[ARQuickLookPreviewItem alloc] initWithFileAtURL:self.fileURL];
  item.allowsContentScaling = self.allowsContentScaling;
  return item;
}

#pragma mark - QLPreviewControllerDelegate

- (void)previewControllerDidDismiss:(QLPreviewController*)controller {
  if (self.webState)
    self.webState->DidRevealWebContent();

  self.webState = nullptr;
  self.viewController = nil;
  self.fileURL = nil;
}

@end
