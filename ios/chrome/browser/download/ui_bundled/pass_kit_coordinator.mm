// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui_bundled/pass_kit_coordinator.h"

#import <memory>

#import "base/metrics/histogram_functions.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/infobars/core/simple_alert_infobar_delegate.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_utils.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"

const char kUmaPresentAddPassesDialogResult[] =
    "Download.IOSPresentAddPassesDialogResult";

namespace {

// Returns PresentAddPassesDialogResult for the given base view
// controller.
PresentAddPassesDialogResult GetUmaResult(
    UIViewController* base_view_controller) {
  if (!base_view_controller.presentedViewController)
    return PresentAddPassesDialogResult::kSuccessful;

  if ([base_view_controller.presentedViewController
          isKindOfClass:[PKAddPassesViewController class]])
    return PresentAddPassesDialogResult::
        kAnotherAddPassesViewControllerIsPresented;

  return PresentAddPassesDialogResult::kAnotherViewControllerIsPresented;
}

}  // namespace

@interface PassKitCoordinator () <PKAddPassesViewControllerDelegate> {
  // Native OS view controller for handling passkit additions.
  PKAddPassesViewController* _viewController;
}
@end

@implementation PassKitCoordinator

- (void)start {
  if (self.passes.count > 0) {
    [self presentAddPassUI];
  } else {
    [self presentErrorUI];
  }
}

- (void)stop {
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;
  _passes = nil;
}

#pragma mark - Private

// Presents PKAddPassesViewController.
- (void)presentAddPassUI {
  if (![PKAddPassesViewController canAddPasses]) {
    [self stop];
    return;
  }

  base::UmaHistogramEnumeration(kUmaPresentAddPassesDialogResult,
                                GetUmaResult(self.baseViewController),
                                PresentAddPassesDialogResult::kCount);
  if (_viewController)
    return;

  _viewController =
      [[PKAddPassesViewController alloc] initWithPasses:self.passes];
  _viewController.delegate = self;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

// Presents "failed to add pkpass" infobar.
- (void)presentErrorUI {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  InfoBarManagerImpl::FromWebState(currentWebState)
      ->AddInfoBar(CreateConfirmInfoBar(std::make_unique<
                                        SimpleAlertInfoBarDelegate>(
          infobars::InfoBarDelegate::SHOW_PASSKIT_ERROR_INFOBAR_DELEGATE_IOS,
          /*vector_icon=*/nullptr,
          l10n_util::GetStringUTF16(IDS_IOS_GENERIC_PASSKIT_ERROR),
          /*auto_expire=*/true,
          /*should_animate=*/true)));

  // Infobar does not provide callback on dismissal.
  [self stop];
}

#pragma mark - PKAddPassesViewControllerDelegate

- (void)addPassesViewControllerDidFinish:
    (PKAddPassesViewController*)controller {
  [self stop];
}

@end
