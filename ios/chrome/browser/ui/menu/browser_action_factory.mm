// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/menu/browser_action_factory.h"

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

@interface BrowserActionFactory ()

// Current browser instance.
@property(nonatomic, assign) Browser* browser;

@end

@implementation BrowserActionFactory

- (instancetype)initWithBrowser:(Browser*)browser
                       scenario:(MenuScenario)scenario {
  DCHECK(browser);
  if (self = [super initWithScenario:scenario]) {
    _browser = browser;
  }
  return self;
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

- (UIAction*)actionToOpenInNewIncognitoTabWithURL:(const GURL)URL
                                       completion:(ProceduralBlock)completion {
  if (!_browser)
    return nil;

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

- (UIAction*)actionOpenImageWithURL:(const GURL)URL
                         completion:(ProceduralBlock)completion {
  UrlLoadingBrowserAgent* loadingAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  UIAction* action = [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENIMAGE)
                image:[UIImage imageNamed:@"open"]
                 type:MenuActionType::OpenInCurrentTab
                block:^{
                  loadingAgent->Load(UrlLoadParams::InCurrentTab(URL));
                  if (completion) {
                    completion();
                  }
                }];
  return action;
}

- (UIAction*)actionOpenImageInNewTabWithUrlLoadParams:(UrlLoadParams)params
                                           completion:
                                               (ProceduralBlock)completion {
  UrlLoadingBrowserAgent* loadingAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(
                                IDS_IOS_CONTENT_CONTEXT_OPENIMAGENEWTAB)
                      image:[UIImage imageNamed:@"open_image_in_new_tab"]
                       type:MenuActionType::OpenInNewTab
                      block:^{
                        loadingAgent->Load(params);
                        if (completion) {
                          completion();
                        }
                      }];
  return action;
}

@end
