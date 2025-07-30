// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_page_state_change_handler.h"

#import <UIKit/UIKit.h>

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation BWGPageStateChangeHandler {
  // The pref service used by this handler.
  raw_ptr<PrefService> _prefService;

  // The base view controller the BWG overlay is currently presented on.
  __weak UIViewController* _baseViewController;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _prefService = prefService;
  }
  return self;
}

- (void)setBaseViewController:(UIViewController*)baseViewController {
  _baseViewController = baseViewController;
}

#pragma mark - BWGPageStateChangeDelegate

- (void)requestPageContextSharingStatusWithCompletion:
    (void (^)(BOOL sharingEnabled))completionCallBack {
  if (_prefService->GetBoolean(prefs::kIOSBWGPageContentSetting)) {
    completionCallBack(YES);
    return;
  }

  CHECK(_baseViewController);
  [self promptUserToSharePageContextWithCompletion:completionCallBack];
}

#pragma mark - Private

// Creates an alert and presents it to the user with proper button callbacks.
- (void)promptUserToSharePageContextWithCompletion:
    (void (^)(BOOL))completionCallBack {
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_BWG_PAGE_CONTENT_SHARING_ALERT_TITLE)
                       message:
                           l10n_util::GetNSString(
                               IDS_IOS_BWG_PAGE_CONTENT_SHARING_ALERT_MESSAGE)
                preferredStyle:UIAlertControllerStyleAlert];

  __weak BWGPageStateChangeHandler* weakSelf = self;

  UIAlertAction* acceptAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_BWG_PAGE_CONTENT_SHARING_ALERT_ACCEPT_BUTTON)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                [weakSelf enablePageContentSharingPref];
                completionCallBack(YES);
              }];

  UIAlertAction* denyAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_BWG_PAGE_CONTENT_SHARING_ALERT_DECLINE_BUTTON)
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction* action) {
                completionCallBack(NO);
              }];

  [alert addAction:acceptAction];
  [alert addAction:denyAction];

  UIViewController* presenterViewController = _baseViewController;
  while (presenterViewController.presentedViewController) {
    presenterViewController = presenterViewController.presentedViewController;
  }

  [presenterViewController presentViewController:alert
                                        animated:YES
                                      completion:nil];
}

// Enables PageContext sharing at the pref level.
- (void)enablePageContentSharingPref {
  _prefService->SetBoolean(prefs::kIOSBWGPageContentSetting, true);
}

@end
