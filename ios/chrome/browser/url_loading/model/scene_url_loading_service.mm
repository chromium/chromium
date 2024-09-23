// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/model/scene_url_loading_service.h"

#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"

SceneUrlLoadingService::SceneUrlLoadingService() {}

void SceneUrlLoadingService::SetDelegate(
    id<SceneURLLoadingServiceDelegate> delegate) {
  delegate_ = delegate;
}

void SceneUrlLoadingService::LoadUrlInNewTab(const UrlLoadParams& params) {
  DCHECK(delegate_);

  Browser* browser = delegate_.currentBrowserForURLLoading;
  ProfileIOS* profile = browser->GetProfile();

  if (params.web_params.url.is_valid()) {
    UrlLoadParams saved_params = params;
    saved_params.web_params.transition_type = ui::PAGE_TRANSITION_TYPED;

    if (params.from_chrome) {
      auto dismiss_completion = ^{
        ApplicationModeForTabOpening mode =
            ((IsIncognitoModeForced(profile->GetPrefs()) ||
              saved_params.in_incognito) &&
             !IsIncognitoModeDisabled(profile->GetPrefs()))
                ? ApplicationModeForTabOpening::INCOGNITO
                : ApplicationModeForTabOpening::NORMAL;
        [delegate_ openSelectedTabInMode:mode
                       withUrlLoadParams:saved_params
                              completion:nil];
      };
      [delegate_ dismissModalDialogsWithCompletion:dismiss_completion
                                    dismissOmnibox:YES];
    } else {
      ApplicationMode mode = params.in_incognito ? ApplicationMode::INCOGNITO
                                                 : ApplicationMode::NORMAL;

      PrefService* prefs = profile->GetPrefs();
      // Don't open the url in below situations:
      // 1. When the url is supposed to be opened in an incognito tab, but the
      // incognito mode is disabled by policy.
      // 2. When the url is supposed to be opened in a normal tab, but the
      // normal mode is disabled by policy.
      if ((params.in_incognito && IsIncognitoModeDisabled(prefs)) ||
          (!params.in_incognito && IsIncognitoModeForced(prefs))) {
        return;
      }

      auto dismiss_completion = ^{
        [delegate_ setCurrentInterfaceForMode:mode];
        UrlLoadingBrowserAgent::FromBrowser(browser)->Load(saved_params);
      };
      [delegate_ dismissModalDialogsWithCompletion:dismiss_completion
                                    dismissOmnibox:YES];
    }
  } else {
    if (profile->IsOffTheRecord() != params.in_incognito) {
      // Must take a snapshot of the tab before we switch the incognito mode
      // because the currentTab will change after the switch.
      web::WebState* currentWebState =
          delegate_.currentBrowserForURLLoading->GetWebStateList()
              ->GetActiveWebState();
      if (currentWebState) {
        SnapshotTabHelper::FromWebState(currentWebState)
            ->UpdateSnapshotWithCallback(nil);
      }

      // Not for this profile, switch and try again.
      ApplicationMode mode = params.in_incognito ? ApplicationMode::INCOGNITO
                                                 : ApplicationMode::NORMAL;
      [delegate_ expectNewForegroundTabForMode:mode];
      [delegate_ setCurrentInterfaceForMode:mode];
      LoadUrlInNewTab(params);
      return;
    }

    // TODO(crbug.com/41427539): move the following lines to Browser level
    // making openNewTabFromOriginPoint a delegate there.
    // openNewTabFromOriginPoint is only called from here.
    [delegate_ openNewTabFromOriginPoint:params.origin_point
                            focusOmnibox:params.should_focus_omnibox
                           inheritOpener:params.inherit_opener];
  }
}

Browser* SceneUrlLoadingService::GetCurrentBrowser() {
  return [delegate_ currentBrowserForURLLoading];
}
