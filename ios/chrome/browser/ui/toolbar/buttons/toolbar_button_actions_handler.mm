// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_actions_handler.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "url/gurl.h"

@implementation ToolbarButtonActionsHandler {
  feature_engagement::Tracker* _engagementTracker;
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
}

- (void)forwardAction {
  self.navigationAgent->GoForward();
}

- (void)tabGridTouchDown {
  [self.applicationHandler prepareTabSwitcher];
}

- (void)tabGridTouchUp {
  [self.applicationHandler displayTabSwitcherInGridLayout];

  _engagementTracker->NotifyEvent(
      feature_engagement::events::kTabGridToolbarItemUsed);
}

- (void)toolsMenuAction {
  [self.menuHandler showToolsMenuPopup];
}

- (void)shareAction {
  [self.activityHandler sharePage];

  _engagementTracker->NotifyEvent(
      feature_engagement::events::kShareToolbarItemUsed);
}

- (void)reloadAction {
  self.navigationAgent->Reload();
}

- (void)stopAction {
  self.navigationAgent->StopLoading();
}

- (void)newTabAction:(id)sender {
  UIView* senderView = base::mac::ObjCCastStrict<UIView>(sender);
  CGPoint center = [senderView.superview convertPoint:senderView.center
                                               toView:nil];
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithIncognito:self.incognito
                                  originPoint:center];
  [self.applicationHandler openURLInNewTab:command];

  _engagementTracker->NotifyEvent(
      feature_engagement::events::kNewTabToolbarItemUsed);
}

- (void)cancelOmniboxFocusAction {
  [self.omniboxHandler cancelOmniboxEdit];
}

@end
