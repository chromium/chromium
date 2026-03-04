// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/unexpected_mode_toast_util.h"

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

void ShowToastWhenOpenInUnexpectedMode(
    SceneState* scene_state,
    ApplicationModeForTabOpening target_mode) {
  id<SnackbarCommands> handler = HandlerForProtocol(
      scene_state.browserProviderInterface.currentBrowserProvider.browser
          ->GetCommandDispatcher(),
      SnackbarCommands);

  UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kChromeUIManagementURL));
  params.web_params.transition_type = ui::PAGE_TRANSITION_TYPED;

  id<TabOpening> tabOpener = scene_state.controller;
  __weak id<TabOpening> weakTabOpener = tabOpener;
  ProceduralBlock moreAction = ^{
    [weakTabOpener dismissModalsAndMaybeOpenSelectedTabInMode:target_mode
                                            withUrlLoadParams:params
                                               dismissOmnibox:YES
                                                   completion:nil];
  };

  SnackbarMessageAction* action = [[SnackbarMessageAction alloc] init];
  action.handler = moreAction;
  action.title = l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_MORE_BUTTON);
  action.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_MORE_BUTTON);

  NSString* text =
      target_mode == ApplicationModeForTabOpening::INCOGNITO
          ? l10n_util::GetNSString(IDS_IOS_SNACKBAR_MESSAGE_INCOGNITO_FORCED)
          : l10n_util::GetNSString(IDS_IOS_SNACKBAR_MESSAGE_INCOGNITO_DISABLED);

  SnackbarMessage* message = [[SnackbarMessage alloc] initWithTitle:text];
  message.action = action;

  [handler showSnackbarMessage:message
                withHapticType:UINotificationFeedbackTypeError];
}
