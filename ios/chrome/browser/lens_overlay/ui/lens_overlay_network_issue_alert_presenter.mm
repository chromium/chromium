// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_network_issue_alert_presenter.h"

#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation LensOverlayNetworkIssueAlertPresenter {
  __weak UIViewController* _baseViewController;
}

- (instancetype)initWithBaseViewController:
    (UIViewController*)baseViewController {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
  }

  return self;
}

- (void)showNoInternetAlert {
  [self.delegate onNetworkIssueAlertWillShow];
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(IDS_IOS_LENS_ALERT_TITLE)
                       message:l10n_util::GetNSString(
                                   IDS_IOS_LENS_ALERT_SUBTITLE)
                preferredStyle:UIAlertControllerStyleAlert];

  __weak __typeof(self) weakSelf = self;
  UIAlertAction* defaultAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_LENS_ALERT_CLOSE_ACTION)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                [weakSelf.delegate onNetworkIssueAlertAcknowledged];
              }];
  [alert addAction:defaultAction];
  [_baseViewController presentViewController:alert animated:YES completion:nil];
}

@end
