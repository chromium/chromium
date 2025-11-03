// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/providers/app_store_bundle/chromium_app_store_bundle_service.h"

#pragma mark - ChromiumAppStoreBundlePromo

// Chromium view controller for app store bundle promo. Displayed when built
// without internal.
@interface ChromiumAppStoreBundlePromo : UIViewController

// Block that would be invoked when the promo is dismissed.
@property(nonatomic, strong) ProceduralBlock dismissHandler;

@end

@implementation ChromiumAppStoreBundlePromo

- (void)viewDidLoad {
  // Content.
  UILabel* label = [[UILabel alloc] init];
  label.text = @"App Store Bundle Promo";
  label.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:label];
  // Navigation bar.
  __weak ChromiumAppStoreBundlePromo* weakSelf = self;
  UIAction* dismissAction = [UIAction actionWithHandler:^(UIAction* action) {
    ChromiumAppStoreBundlePromo* strongSelf = weakSelf;
    [strongSelf.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:^{
                             if (strongSelf.dismissHandler) {
                               strongSelf.dismissHandler();
                             }
                           }];
  }];
  UINavigationItem* navigationItem = [[UINavigationItem alloc] init];
  navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                    primaryAction:dismissAction];
  UINavigationBar* navigationBar = [[UINavigationBar alloc] init];
  navigationBar.items = @[ navigationItem ];
  navigationBar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:navigationBar];
  // Layout.
  [NSLayoutConstraint activateConstraints:@[
    [self.view.topAnchor constraintEqualToAnchor:navigationBar.topAnchor],
    [self.view.safeAreaLayoutGuide.leadingAnchor
        constraintEqualToAnchor:navigationBar.leadingAnchor],
    [self.view.safeAreaLayoutGuide.trailingAnchor
        constraintEqualToAnchor:navigationBar.trailingAnchor],
    [self.view.centerXAnchor constraintEqualToAnchor:label.centerXAnchor],
    [self.view.centerYAnchor constraintEqualToAnchor:label.centerYAnchor],
  ]];
  self.view.backgroundColor = UIColor.systemBackgroundColor;
  self.modalPresentationStyle = UIModalPresentationPageSheet;
}

@end

#pragma mark - AppStoreBundleService

int ChromiumAppStoreBundleService::GetInstalledAppCount() {
  // Chromium is unable to check the number of installed apps in the app store
  // bundle.
  return 0;
}

void ChromiumAppStoreBundleService::PresentAppStoreBundlePromo(
    UIViewController* base_view_controller,
    ProceduralBlock dismiss_handler) {
  // Presents the Chromium open-sourced version of the app store bundle promo.
  ChromiumAppStoreBundlePromo* promo =
      [[ChromiumAppStoreBundlePromo alloc] init];
  promo.dismissHandler = dismiss_handler;
  [base_view_controller presentViewController:promo
                                     animated:YES
                                   completion:nil];
}
