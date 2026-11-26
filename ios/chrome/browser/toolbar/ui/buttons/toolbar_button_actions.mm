// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_actions.h"

#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/intents/model/intents_donation_helper.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/actor_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/ui/util/disabled_buttons_tap_gesture_recognizer.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_buttons_mutator.h"

namespace {

// Records the metric and plays the haptic feedback of a menu being shown.
void RecordMenuTriggered() {
  base::RecordAction(base::UserMetricsAction("MobileMenuToolbarMenuTriggered"));
  TriggerHapticFeedbackForImpact(UIImpactFeedbackStyleHeavy);
}

// Records the metric and plays the haptic feedback of a button's menu being
// triggered, on `button`'s menu action.
void AddMenuTriggeredActionToButton(ToolbarButton* button) {
  [button addAction:[UIAction actionWithHandler:^(UIAction*) {
            RecordMenuTriggered();
          }]
      forControlEvents:UIControlEventMenuActionTriggered];
}

}  // namespace

@implementation ToolbarButtonActions {
  // The context menu of the tab grid buttons.
  UIMenu* _tabGridMenu;
}

#pragma mark - Public

- (void)configureButton:(ToolbarButton*)button ofType:(ToolbarButtonType)type {
  SEL tapAction = nullptr;
  switch (type) {
    case ToolbarButtonTypeBack:
      tapAction = @selector(backButtonTapped);
      AddMenuTriggeredActionToButton(button);
      break;
    case ToolbarButtonTypeForward:
      tapAction = @selector(forwardButtonTapped);
      AddMenuTriggeredActionToButton(button);
      break;
    case ToolbarButtonTypeReload:
      tapAction = @selector(reloadButtonTapped);
      break;
    case ToolbarButtonTypeStop:
      tapAction = @selector(stopButtonTapped);
      break;
    case ToolbarButtonTypeShare:
      tapAction = @selector(shareButtonTapped:);
      break;
    case ToolbarButtonTypeAssistant:
      tapAction = @selector(assistantButtonTapped);
      break;
    case ToolbarButtonTypeTabGrid:
      tapAction = @selector(tabGridTouchUp);
      [button addTarget:self
                    action:@selector(tabGridTouchDown)
          forControlEvents:UIControlEventTouchDown];
      button.menu = [self makeTabGridButtonMenu];
      break;
    case ToolbarButtonTypeTools:
      tapAction = @selector(toolsMenuButtonTapped);
      break;
    case ToolbarButtonTypeNewTab:
      tapAction = @selector(newTabButtonTapped:);
      AddMenuTriggeredActionToButton(button);
      break;
  }
  CHECK(tapAction);
  [button addTarget:self
                action:tapAction
      forControlEvents:UIControlEventTouchUpInside];
}

- (UIGestureRecognizer*)disabledNavigationTapRecognizerForButtons:
    (NSArray<ToolbarButton*>*)buttons {
  return [[DisabledButtonsTapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(disabledNavigationButtonTapped)
             buttons:buttons];
}

#pragma mark - ToolbarTabGridMenuConsumer

- (void)setTabGridMenu:(UIMenu*)menu {
  _tabGridMenu = menu;
}

#pragma mark - Private

// Returns the menu of a tab grid button. Its items are those of the latest
// tab grid menu, read when it opens.
- (UIMenu*)makeTabGridButtonMenu {
  __weak __typeof(self) weakSelf = self;
  UIDeferredMenuElement* element =
      [UIDeferredMenuElement elementWithUncachedProvider:^(
                                 void (^completion)(NSArray<UIMenuElement*>*)) {
        completion([weakSelf tabGridMenuElements] ?: @[]);
      }];
  return [UIMenu menuWithChildren:@[ element ]];
}

// Returns the elements of the tab grid buttons' menu, recording that it's
// shown and playing its haptic feedback, as for the other buttons' menus. That
// is only done when it has items: with none, UIKit shows nothing, while
// `UIControlEventMenuActionTriggered` is still sent.
- (NSArray<UIMenuElement*>*)tabGridMenuElements {
  if (_tabGridMenu.children.count == 0) {
    return @[];
  }
  RecordMenuTriggered();
  return _tabGridMenu.children;
}

// Handles taps on disabled navigation buttons.
- (void)disabledNavigationButtonTapped {
  [self.actorOverlayHandler showActorInteractionLimitedSnackbar];
}

// Handles back button tap.
- (void)backButtonTapped {
  [self.mutator recordUserActionsForButtonTapped:ToolbarButtonTypeBack];
  [self.mutator goBack];
}

// Handles forward button tap.
- (void)forwardButtonTapped {
  [self.mutator recordUserActionsForButtonTapped:ToolbarButtonTypeForward];
  [self.mutator goForward];
}

// Handles reload button tap.
- (void)reloadButtonTapped {
  [self.mutator recordUserActionsForButtonTapped:ToolbarButtonTypeReload];
  [self.mutator reload];
}

// Handles stop button tap.
- (void)stopButtonTapped {
  [self.mutator recordUserActionsForButtonTapped:ToolbarButtonTypeStop];
  [self.mutator stop];
}

// Handles share button tap.
- (void)shareButtonTapped:(UIView*)sender {
  [self.mutator recordUserActionsForButtonTapped:ToolbarButtonTypeShare];
  [self.activityServiceHandler showShareSheetFromShareButton:sender];
}

// Handles assistant button tap.
- (void)assistantButtonTapped {
  [self.mutator recordUserActionsForButtonTapped:ToolbarButtonTypeAssistant];
  [self.mutator assistantButtonTapped];
}

// Handles tools menu button tap.
- (void)toolsMenuButtonTapped {
  [self.mutator recordUserActionsForButtonTapped:ToolbarButtonTypeTools];
  [self.popupMenuHandler showToolsMenuPopup];
}

// Handles new tab button tap.
- (void)newTabButtonTapped:(UIView*)sender {
  [self.mutator recordUserActionsForButtonTapped:ToolbarButtonTypeNewTab];
  [self.mutator openNewTabFromPoint:[sender.superview convertPoint:sender.center
                                                            toView:nil]];
}

// Handles tab grid button touch down.
- (void)tabGridTouchDown {
  [IntentDonationHelper donateIntent:IntentType::kOpenTabGrid];
  [self.sceneHandler prepareTabSwitcher];
}

// Handles tab grid button touch up.
- (void)tabGridTouchUp {
  [self.mutator recordUserActionsForButtonTapped:ToolbarButtonTypeTabGrid];
  [self.sceneHandler displayTabGridInMode:TabGridOpeningMode::kDefault];
}

@end
