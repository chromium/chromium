// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/welcome/welcome_screen_coordinator.h"

#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/ui/first_run/welcome/static_file_view_controller.h"
#include "ios/chrome/browser/ui/first_run/welcome/welcome_screen_view_controller.h"
#include "ios/chrome/browser/ui/util/terms_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WelcomeScreenCoordinator () <WelcomeScreenViewControllerDelegate>

// Welcome screen view controller.
@property(nonatomic, strong) WelcomeScreenViewController* viewController;

@property(nonatomic, weak) id<FirstRunScreenDelegate> delegate;

@end

@implementation WelcomeScreenCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _delegate = delegate;
  }
  return self;
}

- (void)start {
  // TODO(crbug.com/1189815): Check if welcome screen need to be shown.
  // if not:
  // [self.delegate willFinishPresenting]
  // if yes:
  self.viewController = [[WelcomeScreenViewController alloc] init];
  self.viewController.delegate = self;
  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ self.viewController ]
                                           animated:animated];
}

- (void)stop {
  self.delegate = nil;
  self.viewController = nil;
}

#pragma mark - WelcomeScreenViewControllerDelegate

- (void)didTapTOSLink {
  // Create a StaticFileViewController to show the terms of service page.
  NSString* title = l10n_util::GetNSString(IDS_IOS_FIRSTRUN_TERMS_TITLE);

  std::string TOS = GetTermsOfServicePath();
  NSString* path = [[base::mac::FrameworkBundle() bundlePath]
      stringByAppendingPathComponent:base::SysUTF8ToNSString(TOS)];
  NSURLComponents* components = [[NSURLComponents alloc] init];
  [components setScheme:@"file"];
  [components setHost:@""];
  [components setPath:path];
  NSURL* TOSURL = [components URL];

  StaticFileViewController* staticViewController =
      [[StaticFileViewController alloc]
          initWithBrowserState:self.browser->GetBrowserState()
                           URL:TOSURL];
  [staticViewController setTitle:title];

  [self.baseNavigationController pushViewController:staticViewController
                                           animated:YES];
}

- (void)didTapPrimaryActionButton {
  // TODO(crbug.com/1189815):
  // 1. Update the pref seervice if the checkbox is selected.
  // 2. Store a status that the welcome screen has been shown to an
  // NSUserDefault object.

  [self.delegate willFinishPresenting];
}

@end
