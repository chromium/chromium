// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_coordinator.h"

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/commands/popup_menu_commands.h"
#import "ios/chrome/browser/ui/main/layout_guide_util.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SecondaryToolbarCoordinator ()
@property(nonatomic, strong) SecondaryToolbarViewController* viewController;
@end

@implementation SecondaryToolbarCoordinator

@dynamic viewController;

#pragma mark - AdaptiveToolbarCoordinator

- (void)start {
  self.viewController = [[SecondaryToolbarViewController alloc] init];
  self.viewController.buttonFactory = [self buttonFactoryWithType:SECONDARY];
  self.viewController.omniboxCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), OmniboxCommands);
  self.viewController.popupMenuCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PopupMenuCommands);
  self.viewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);

  [super start];
}

#pragma mark - ToolbarCommands

- (void)triggerToolbarSlideInAnimation {
  [self.viewController triggerToolbarSlideInAnimationFromBelow:YES];
}

@end
