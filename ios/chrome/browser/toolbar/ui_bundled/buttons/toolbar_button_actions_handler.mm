// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_button_actions_handler.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/bubble/model/tab_based_iph_browser_agent.h"
#import "ios/chrome/browser/intents/model/intents_donation_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "url/gurl.h"

@implementation ToolbarButtonActionsHandler {
  raw_ptr<feature_engagement::Tracker> _engagementTracker;
}

- (instancetype)initWithEngagementTracker:
    (feature_engagement::Tracker*)engagementTracker {
  self = [super init];
  if (self) {
    CHECK(engagementTracker);
    _engagementTracker = engagementTracker;
  }
  return self;
}

- (void)backAction {
  self.navigationAgent->GoBack();
  self.tabBasedIPHAgent->NotifyBackForwardButtonTap();
}

- (void)forwardAction {
  self.navigationAgent->GoForward();
  self.tabBasedIPHAgent->NotifyBackForwardButtonTap();
}

- (void)tabGridTouchDown {
  [IntentDonationHelper donateIntent:IntentType::kOpenTabGrid];
  [self.applicationHandler prepareTabSwitcher];
}

- (void)tabGridTouchUp {
  [self.applicationHandler displayTabGridInMode:TabGridOpeningMode::kDefault];
}

- (void)toolsMenuAction {
  [self.menuHandler showToolsMenuPopup];
}

- (void)shareAction {
  [self.activityHandler showShareSheet];
}

- (void)reloadAction {
  self.navigationAgent->Reload();
}

- (void)stopAction {
  self.navigationAgent->StopLoading();
}

- (void)newTabAction:(id)sender {
  UIView* senderView = base::apple::ObjCCastStrict<UIView>(sender);
  CGPoint center = [senderView.superview convertPoint:senderView.center
                                               toView:nil];
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithIncognito:self.incognito
                                  originPoint:center];
  [self.applicationHandler openURLInNewTab:command];

  [IntentDonationHelper donateIntent:IntentType::kOpenNewTab];
}

- (void)diamondPrototypeAction:(id)sender {
  CHECK(IsDiamondPrototypeEnabled());
  id<LensOverlayCommands> overlayHandler =
      (id<LensOverlayCommands>)self.applicationHandler;
  [overlayHandler createAndShowLensUI:YES
                           entrypoint:LensOverlayEntrypoint::kLocationBar
                           completion:nil];
}

- (void)cancelOmniboxFocusAction {
  [self.omniboxHandler cancelOmniboxEdit];
}

@end
