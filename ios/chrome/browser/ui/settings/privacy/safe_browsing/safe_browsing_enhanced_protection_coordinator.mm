// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_enhanced_protection_coordinator.h"

#import "base/mac/foundation_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_enhanced_protection_mediator.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_enhanced_protection_view_controller.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SafeBrowsingEnhancedProtectionCoordinator () <
    SafeBrowsingEnhancedProtectionViewControllerPresentationDelegate>

// View controller for privacy safe browsing enhanced protection.
@property(nonatomic, strong)
    SafeBrowsingEnhancedProtectionViewController* viewController;
// Mediator instantiated by coordinator.
@property(nonatomic, strong) SafeBrowsingEnhancedProtectionMediator* mediator;

@end

@implementation SafeBrowsingEnhancedProtectionCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  if ([super initWithBaseViewController:navigationController browser:browser]) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  self.viewController = [[SafeBrowsingEnhancedProtectionViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.viewController.presentationDelegate = self;
  self.mediator = [[SafeBrowsingEnhancedProtectionMediator alloc] init];
  self.mediator.consumer = self.viewController;

  self.viewController.dispatcher = static_cast<
      id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>>(
      self.browser->GetCommandDispatcher());
  DCHECK(self.baseNavigationController);
  [self.baseNavigationController
      presentViewController:self.viewController.navigationController
                   animated:YES
                 completion:nil];
}

#pragma mark - SafeBrowsingEnhancedProtectionViewControllerPresentationDelegate

- (void)safeBrowsingEnhancedProtectionViewControllerDidRemove:
    (SafeBrowsingEnhancedProtectionViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate safeBrowsingEnhancedProtectionCoordinatorDidRemove:self];
}

@end
