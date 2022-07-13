// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/incognito_interstitial/incognito_interstitial_coordinator.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/browser/ui/incognito_interstitial/incognito_interstitial_coordinator_delegate.h"
#import "ios/chrome/browser/ui/incognito_interstitial/incognito_interstitial_view_controller.h"
#import "ios/chrome/browser/ui/incognito_interstitial/incognito_interstitial_view_controller_delegate.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_url_loader_delegate.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface IncognitoInterstitialCoordinator () <
    IncognitoInterstitialViewControllerDelegate,
    NewTabPageURLLoaderDelegate>

@property(nonatomic, strong)
    IncognitoInterstitialViewController* incognitoInterstitialViewController;

@end

@implementation IncognitoInterstitialCoordinator

- (void)startWithCompletion:(ProceduralBlock)completion {
  self.incognitoInterstitialViewController =
      [[IncognitoInterstitialViewController alloc] init];
  self.incognitoInterstitialViewController.delegate = self;
  self.incognitoInterstitialViewController.URLLoaderDelegate = self;
  self.incognitoInterstitialViewController.subtitleText =
      base::SysUTF8ToNSString(self.urlLoadParams.web_params.url.spec());

  [self.baseViewController
      presentViewController:self.incognitoInterstitialViewController
                   animated:YES
                 completion:completion];
}

- (void)stopWithCompletion:(ProceduralBlock)completion {
  __weak __typeof(self) weakSelf = self;
  [self.incognitoInterstitialViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           weakSelf.incognitoInterstitialViewController = nil;
                           completion();
                         }];
}

- (void)start {
  [self startWithCompletion:nil];
}

- (void)stop {
  [self stopWithCompletion:nil];
}

#pragma mark - IncognitoInterstitialViewControllerDelegate

- (void)didTapPrimaryActionButton {
  // Dismiss modals (including interstitial) and open link in incognito tab.
  __weak __typeof(self) weakSelf = self;
  UrlLoadParams params = self.urlLoadParams;
  BOOL dismissOmnibox = self.shouldDismissOmnibox;
  void (^dismissModalsAndOpenTab)() = ^{
    [weakSelf.tabOpener dismissModalsAndOpenSelectedTabInMode:
                            ApplicationModeForTabOpening::INCOGNITO
                                            withUrlLoadParams:params
                                               dismissOmnibox:dismissOmnibox
                                                   completion:nil];
  };

  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  IncognitoReauthSceneAgent* reauthAgent =
      [IncognitoReauthSceneAgent agentFromScene:sceneState];
  if (reauthAgent.authenticationRequired) {
    [reauthAgent
        authenticateIncognitoContentWithCompletionBlock:^(BOOL success) {
          if (success) {
            dismissModalsAndOpenTab();
          }
        }];
  } else {
    dismissModalsAndOpenTab();
  }
}

- (void)didTapSecondaryActionButton {
  // Dismiss modals (including interstitial) and open link in regular tab.
  [self.tabOpener
      dismissModalsAndOpenSelectedTabInMode:ApplicationModeForTabOpening::NORMAL
                          withUrlLoadParams:self.urlLoadParams
                             dismissOmnibox:self.shouldDismissOmnibox
                                 completion:nil];
}

- (void)didTapCancelButton {
  [self.delegate shouldStopIncognitoInterstitial:self];
}

#pragma mark - NewTabPageURLLoaderDelegate

- (void)loadURLInTab:(const GURL&)URL {
  [self.tabOpener
      dismissModalsAndOpenSelectedTabInMode:ApplicationModeForTabOpening::
                                                INCOGNITO
                          withUrlLoadParams:UrlLoadParams::InCurrentTab(URL)
                             dismissOmnibox:YES
                                 completion:nil];
}

@end
