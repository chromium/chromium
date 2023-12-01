// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_content_notification_promo_coordinator.h"

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_content_notification_promo_coordinator_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_content_notification_promo_view_controller.h"

@implementation SetUpListContentNotificationPromoCoordinator {
  // The view controller that displays the content notification promo.
  SetUpListContentNotificationPromoViewController* _viewController;

  // Application is used to open the OS settings for this app.
  UIApplication* _application;

  // Whether or not the Set Up List Item should be marked complete.
  BOOL _markItemComplete;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               application:(UIApplication*)application {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _application = application;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController =
      [[SetUpListContentNotificationPromoViewController alloc] init];
  _viewController.delegate = self;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
  _viewController.presentationController.delegate = self;
}

- (void)stop {
  _viewController.presentationController.delegate = nil;

  ProceduralBlock completion = nil;
  if (_markItemComplete) {
    PrefService* localState = GetApplicationContext()->GetLocalState();
    completion = ^{
      set_up_list_prefs::MarkItemComplete(
          localState, SetUpListItemType::kContentNotification);
    };
  }
  [_viewController dismissViewControllerAnimated:YES completion:completion];
  _viewController = nil;
  self.delegate = nil;
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  // TODO(b/311068390): implementing.
}

- (void)didTapSecondaryActionButton {
  // TODO(b/311068390): implementing.
}

- (void)didTapTertiaryActionButton {
  // TODO(b/311068390): implementing.
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate setUpListContentNotificationPromoDidFinish];
}

@end
