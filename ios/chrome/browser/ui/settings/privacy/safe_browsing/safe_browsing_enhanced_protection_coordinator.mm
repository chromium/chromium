// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_enhanced_protection_coordinator.h"

#import "base/apple/foundation_util.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_enhanced_protection_view_controller.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface SafeBrowsingEnhancedProtectionCoordinator () <
    SafeBrowsingEnhancedProtectionViewControllerPresentationDelegate>

// View controller for privacy safe browsing enhanced protection.
@property(nonatomic, strong)
    SafeBrowsingEnhancedProtectionViewController* viewController;

@end

@implementation SafeBrowsingEnhancedProtectionCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  SafeBrowsingEnhancedProtectionViewController* viewController = nil;
    viewController = [[SafeBrowsingEnhancedProtectionViewController alloc]
        initWithStyle:UITableViewStyleGrouped];
  self.viewController = viewController;

  viewController.presentationDelegate = self;
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  viewController.applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  viewController.browserHandler =
      HandlerForProtocol(dispatcher, BrowserCommands);
  viewController.settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  viewController.snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);

  DCHECK(self.baseNavigationController);
  [self.baseNavigationController
      presentViewController:viewController.navigationController
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
