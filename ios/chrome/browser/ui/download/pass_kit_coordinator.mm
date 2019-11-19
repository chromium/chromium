// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/pass_kit_coordinator.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/infobars/core/simple_alert_infobar_delegate.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

@interface PassKitCoordinator ()<CRWWebStateObserver,
                                 PKAddPassesViewControllerDelegate> {
  // Present the "Add pkpass UI".
  PKAddPassesViewController* _viewController;
  // Allows PassKitCoordinator to observe a web state.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
}
@end

@implementation PassKitCoordinator
@synthesize pass = _pass;
@synthesize webState = _webState;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController {
  self = [super initWithBaseViewController:viewController];
  if (self) {
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
  }
  return self;
}

- (void)start {
  DCHECK(self.webState);
  if (self.pass) {
    [self presentAddPassUI];
  } else {
    [self presentErrorUI];
  }
}

- (void)stop {
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;
  _pass = nil;
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webState = nullptr;
  }
}

- (void)setWebState:(web::WebState*)webState {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
  }
  _webState = webState;
  if (webState) {
    webState->AddObserver(_webStateObserver.get());
  }
}

#pragma mark - Private

// Presents PKAddPassesViewController.
- (void)presentAddPassUI {
  if (![PKAddPassesViewController canAddPasses]) {
    [self stop];
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(kUmaPresentAddPassesDialogResult,
                            GetUmaResult(self.baseViewController),
                            PresentAddPassesDialogResult::kCount);
  if (_viewController)
    return;

  _viewController = [[PKAddPassesViewController alloc] initWithPass:self.pass];
  _viewController.delegate = self;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

// Presents "failed to add pkpass" infobar.
- (void)presentErrorUI {
  DCHECK(InfoBarManagerImpl::FromWebState(_webState));
  SimpleAlertInfoBarDelegate::Create(
      InfoBarManagerImpl::FromWebState(_webState),
      infobars::InfoBarDelegate::SHOW_PASSKIT_ERROR_INFOBAR_DELEGATE_IOS,
      /*vector_icon=*/nullptr,
      l10n_util::GetStringUTF16(IDS_IOS_GENERIC_PASSKIT_ERROR));

  // Infobar does not provide callback on dismissal.
  [self stop];
}

#pragma mark - PassKitTabHelperDelegate

- (void)passKitTabHelper:(PassKitTabHelper*)tabHelper
    presentDialogForPass:(PKPass*)pass
                webState:(web::WebState*)webState {
  self.pass = pass;
  self.webState = webState;
  [self start];
}

#pragma mark - PKAddPassesViewControllerDelegate

- (void)addPassesViewControllerDidFinish:
    (PKAddPassesViewController*)controller {
  [self stop];
}

#pragma mark - WebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(webState, _webState);
  webState->RemoveObserver(_webStateObserver.get());
  _webState = nil;
}

@end
