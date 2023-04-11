// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/incognito_interstitial/incognito_interstitial_coordinator.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/incognito_interstitial/incognito_interstitial_constants.h"
#import "ios/chrome/browser/ui/incognito_interstitial/incognito_interstitial_coordinator_delegate.h"
#import "ios/chrome/browser/ui/incognito_interstitial/incognito_interstitial_view_controller.h"
#import "ios/chrome/browser/ui/incognito_interstitial/incognito_interstitial_view_controller_delegate.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/ntp/incognito/incognito_view_util.h"
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

@property(nonatomic, assign)
    IncognitoInterstitialActions incognitoInterstitialAction;

@end

@implementation IncognitoInterstitialCoordinator

- (void)start {
  self.incognitoInterstitialViewController =
      [[IncognitoInterstitialViewController alloc] init];
  self.incognitoInterstitialViewController.delegate = self;
  self.incognitoInterstitialViewController.URLLoaderDelegate = self;
  self.incognitoInterstitialViewController.URLText =
      base::SysUTF8ToNSString(self.urlLoadParams.web_params.url.spec());
  self.incognitoInterstitialViewController.modalPresentationStyle =
      UIModalPresentationFormSheet;

  [self.baseViewController
      presentViewController:self.incognitoInterstitialViewController
                   animated:YES
                 completion:nil];

  // The default recorded action is "External dismissed".
  // This value is changed right before the "Open in Chrome Incognito", "Open in
  // Chrome", "Cancel" action buttons or the "Learn more about Incognito" link
  // trigger the dismissal of the interstitial.
  self.incognitoInterstitialAction =
      IncognitoInterstitialActions::kExternalDismissed;
}

- (void)stop {
  [self.incognitoInterstitialViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  self.incognitoInterstitialViewController = nil;

  UMA_HISTOGRAM_ENUMERATION(kIncognitoInterstitialActionsHistogram,
                            self.incognitoInterstitialAction);
}

#pragma mark - IncognitoInterstitialViewControllerDelegate

- (void)didTapPrimaryActionButton {
  // Dismiss modals (including interstitial) and open link in incognito tab.
  __weak __typeof(self) weakSelf = self;
  UrlLoadParams copyOfUrlLoadParams = self.urlLoadParams;
  void (^dismissModalsAndOpenTab)() = ^{
    __typeof(self) strongSelf = weakSelf;
    strongSelf.incognitoInterstitialAction =
        IncognitoInterstitialActions::kOpenInChromeIncognito;
    [strongSelf.tabOpener
        dismissModalsAndMaybeOpenSelectedTabInMode:
            ApplicationModeForTabOpening::INCOGNITO
                                 withUrlLoadParams:copyOfUrlLoadParams
                                    dismissOmnibox:YES
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
  self.incognitoInterstitialAction =
      IncognitoInterstitialActions::kOpenInChrome;
  [self.tabOpener dismissModalsAndMaybeOpenSelectedTabInMode:
                      ApplicationModeForTabOpening::NORMAL
                                           withUrlLoadParams:self.urlLoadParams
                                              dismissOmnibox:YES
                                                  completion:nil];
}

- (void)didTapCancelButton {
  self.incognitoInterstitialAction = IncognitoInterstitialActions::kCancel;
  [self.delegate shouldStopIncognitoInterstitial:self];
}

#pragma mark - NewTabPageURLLoaderDelegate

- (void)loadURLInTab:(const GURL&)URL {
  DCHECK(URL == GetLearnMoreIncognitoUrl());
  self.incognitoInterstitialAction = IncognitoInterstitialActions::kLearnMore;
  [self.tabOpener
      dismissModalsAndMaybeOpenSelectedTabInMode:ApplicationModeForTabOpening::
                                                     NORMAL
                               withUrlLoadParams:UrlLoadParams::InNewTab(URL)
                                  dismissOmnibox:YES
                                      completion:nil];
}

@end
