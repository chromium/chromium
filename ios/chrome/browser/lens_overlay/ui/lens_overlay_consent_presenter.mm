// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_consent_presenter.h"

#import "base/check.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_detents_manager.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_sheet_detent_state.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_consent_view_controller.h"

@interface LensOverlayConsentPresenter () <LensOverlayDetentsManagerDelegate>
@end

@implementation LensOverlayConsentPresenter {
  /// Orchestrates the change in detents of the associated bottom sheet.
  LensOverlayDetentsManager* _detentsManager;
  __weak UIViewController* _presentedConsentViewController;
  __weak UIViewController* _presentingViewController;
}

- (instancetype)initWithPresentingViewController:(UIViewController*)presentingVC
                  presentedConsentViewController:
                      (LensOverlayConsentViewController*)
                          presentedConsentViewController {
  self = [super init];
  if (self) {
    _presentedConsentViewController = presentedConsentViewController;
    _presentingViewController = presentingVC;
  }
  return self;
}

- (BOOL)isConsentVisible {
  return _presentingViewController.presentedViewController != nil &&
         _presentingViewController.presentedViewController ==
             _presentedConsentViewController;
}

- (void)showConsentViewController {
  // Configure sheet presentation
  UISheetPresentationController* sheet =
      _presentedConsentViewController.sheetPresentationController;
  sheet.prefersEdgeAttachedInCompactHeight = YES;

  UIWindow* window = _presentingViewController.view.window;
  _detentsManager =
      [[LensOverlayDetentsManager alloc] initWithBottomSheet:sheet
                                                      window:window];
  _detentsManager.delegate = self;
  [_detentsManager adjustDetentsForState:SheetDetentStateConsentDialog];
  [_presentingViewController
      presentViewController:_presentedConsentViewController
                   animated:YES
                 completion:nil];
}

- (void)dismissConsentViewControllerAnimated:(BOOL)animated
                                  completion:(void (^)(void))completion {
  // As the presenting view controller is not owned by the presenter it can be
  // released independently. If this is the case, make sure the completion is
  // called before exiting.
  if (!_presentingViewController) {
    completion();
    return;
  }

  [_presentingViewController dismissViewControllerAnimated:animated
                                                completion:completion];
}

#pragma mark - LensOverlayDetentsManagerDelegate

- (void)lensOverlayDetentsManagerDidChangeDimensionState:
    (LensOverlayDetentsManager*)detentsManager {
  if (detentsManager.sheetDimension == SheetDimensionState::kHidden) {
    [self.delegate requestDismissalOfConsentDialog:self];
  }
}

- (BOOL)lensOverlayDetentsManagerShouldDismissBottomSheet:
    (LensOverlayDetentsManager*)detentsManager {
  DCHECK(detentsManager.sheetDimension == SheetDimensionState::kConsent);
  return NO;
}

@end
