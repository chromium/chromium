// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/menu/action_factory.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ActionFactory ()

// Current browser instance.
@property(nonatomic, assign) Browser* browser;

// Histogram to record executed actions.
@property(nonatomic, assign) const char* histogram;

@end

@implementation ActionFactory

- (instancetype)initWithBrowser:(Browser*)browser
                       scenario:(MenuScenario)scenario {
  DCHECK(browser);

  if (self = [super init]) {
    _browser = browser;
    _histogram = GetActionsHistogramName(scenario);
  }
  return self;
}

- (UIAction*)actionWithTitle:(NSString*)title
                       image:(UIImage*)image
                        type:(MenuActionType)type
                       block:(ProceduralBlock)block {
  // Capture only the histogram name's pointer to be copied by the block.
  const char* histogram = self.histogram;
  return [UIAction actionWithTitle:title
                             image:image
                        identifier:nil
                           handler:^(UIAction* action) {
                             base::UmaHistogramEnumeration(histogram, type);
                             if (block) {
                               block();
                             }
                           }];
}

- (UIAction*)actionToCopyURL:(const GURL)URL {
  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_COPY_LINK_ACTION_TITLE)
                image:[UIImage imageNamed:@"copy_link_url"]
                 type:MenuActionType::Copy
                block:^{
                  StoreURLInPasteboard(URL);
                }];
}

- (UIAction*)actionToShareWithBlock:(ProceduralBlock)block {
  return
      [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_SHARE_BUTTON_LABEL)
                      image:[UIImage imageNamed:@"share"]
                       type:MenuActionType::Share
                      block:block];
}

- (UIAction*)actionToDeleteWithBlock:(ProceduralBlock)block {
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_DELETE_ACTION_TITLE)
                      image:[UIImage imageNamed:@"delete"]
                       type:MenuActionType::Delete
                      block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionToOpenInNewTabWithURL:(const GURL)URL
                              completion:(ProceduralBlock)completion {
  UrlLoadParams params = UrlLoadParams::InNewTab(URL);
  UrlLoadingBrowserAgent* loadingAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  return [self actionToOpenInNewTabWithBlock:^{
    loadingAgent->Load(params);
    if (completion) {
      completion();
    }
  }];
}

- (UIAction*)actionToOpenInNewTabWithBlock:(ProceduralBlock)block {
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)
                         image:[UIImage imageNamed:@"open_in_new_tab"]
                          type:MenuActionType::OpenInNewTab
                         block:block];
}

- (UIAction*)actionToOpenAllTabsWithBlock:(ProceduralBlock)block {
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_OPEN_ALL_LINKS)
                         image:[UIImage systemImageNamed:@"plus"]
                          type:MenuActionType::OpenAllInNewTabs
                         block:block];
}

- (UIAction*)actionToOpenInNewIncognitoTabWithURL:(const GURL)URL
                                       completion:(ProceduralBlock)completion {
  UrlLoadParams params = UrlLoadParams::InNewTab(URL);
  params.in_incognito = YES;
  UrlLoadingBrowserAgent* loadingAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  return [self actionToOpenInNewIncognitoTabWithBlock:^{
    loadingAgent->Load(params);
    if (completion) {
      completion();
    }
  }];
}

- (UIAction*)actionToOpenInNewIncognitoTabWithBlock:(ProceduralBlock)block {
  // Wrap the block with the incognito auth check, if necessary.
  if (base::FeatureList::IsEnabled(kIncognitoAuthentication)) {
    IncognitoReauthSceneAgent* reauthAgent = [IncognitoReauthSceneAgent
        agentFromScene:SceneStateBrowserAgent::FromBrowser(self.browser)
                           ->GetSceneState()];
    if (reauthAgent.authenticationRequired) {
      block = ^{
        [reauthAgent
            authenticateIncognitoContentWithCompletionBlock:^(BOOL success) {
              if (success && block != nullptr) {
                block();
              }
            }];
      };
    }
  }

  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_OPEN_IN_INCOGNITO_ACTION_TITLE)
                         image:[UIImage imageNamed:@"open_in_incognito"]
                          type:MenuActionType::OpenInNewIncognitoTab
                         block:block];
}

- (UIAction*)actionToOpenInNewWindowWithURL:(const GURL)URL
                             activityOrigin:
                                 (WindowActivityOrigin)activityOrigin {
  id<ApplicationCommands> windowOpener = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  NSUserActivity* activity = ActivityToLoadURL(activityOrigin, URL);
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_OPENINNEWWINDOW)
                         image:[UIImage imageNamed:@"open_new_window"]
                          type:MenuActionType::OpenInNewWindow
                         block:^{
                           [windowOpener openNewWindowWithActivity:activity];
                         }];
}

- (UIAction*)actionToRemoveWithBlock:(ProceduralBlock)block {
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_REMOVE_ACTION_TITLE)
                      image:[UIImage imageNamed:@"remove"]
                       type:MenuActionType::Remove
                      block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionToEditWithBlock:(ProceduralBlock)block {
  return [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_EDIT_ACTION_TITLE)
                         image:[UIImage imageNamed:@"edit"]
                          type:MenuActionType::Edit
                         block:block];
}

- (UIAction*)actionToHideWithBlock:(ProceduralBlock)block {
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(
                                IDS_IOS_RECENT_TABS_HIDE_MENU_OPTION)
                      image:[UIImage imageNamed:@"remove"]
                       type:MenuActionType::Hide
                      block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionToMoveFolderWithBlock:(ProceduralBlock)block {
  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)
                image:[UIImage imageNamed:@"move_folder"]
                 type:MenuActionType::Move
                block:block];
}

- (UIAction*)actionToMarkAsReadWithBlock:(ProceduralBlock)block {
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_READING_LIST_MARK_AS_READ_ACTION)
                         image:[UIImage imageNamed:@"mark_read"]
                          type:MenuActionType::Read
                         block:block];
}

- (UIAction*)actionToMarkAsUnreadWithBlock:(ProceduralBlock)block {
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_READING_LIST_MARK_AS_UNREAD_ACTION)
                         image:[UIImage imageNamed:@"remove"]
                          type:MenuActionType::Unread
                         block:block];
}

- (UIAction*)actionToOpenOfflineVersionInNewTabWithBlock:
    (ProceduralBlock)block {
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_READING_LIST_OPEN_OFFLINE_BUTTON)
                         image:[UIImage imageNamed:@"offline"]
                          type:MenuActionType::ViewOffline
                         block:block];
}

- (UIAction*)actionToOpenJavascriptWithBlock:(ProceduralBlock)block {
  return
      [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPEN)
                      image:[UIImage imageNamed:@"open"]
                       type:MenuActionType::OpenJavascript
                      block:block];
}

@end
