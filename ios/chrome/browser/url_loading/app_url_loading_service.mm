// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/app_url_loading_service.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/url_loading/url_loading_service.h"
#import "ios/chrome/browser/url_loading/url_loading_service_factory.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

AppUrlLoadingService::AppUrlLoadingService() {}

void AppUrlLoadingService::SetDelegate(
    id<AppURLLoadingServiceDelegate> delegate) {
  delegate_ = delegate;
}

void AppUrlLoadingService::LoadUrlInNewTab(const UrlLoadParams& params) {
  DCHECK(delegate_);

  if (params.web_params.url.is_valid()) {
    UrlLoadParams saved_params = params;
    saved_params.web_params.transition_type = ui::PAGE_TRANSITION_TYPED;

    if (params.from_chrome) {
      [delegate_
          dismissModalDialogsWithCompletion:^{
            [delegate_ openSelectedTabInMode:ApplicationMode::NORMAL
                           withUrlLoadParams:saved_params
                                  completion:nil];
          }
                             dismissOmnibox:YES];
    } else {
      ApplicationMode mode = params.in_incognito ? ApplicationMode::INCOGNITO
                                                 : ApplicationMode::NORMAL;
      [delegate_
          dismissModalDialogsWithCompletion:^{
            [delegate_ setCurrentInterfaceForMode:mode];
            UrlLoadingServiceFactory::GetForBrowserState(
                [delegate_ currentBrowserState])
                ->Load(saved_params);
          }
                             dismissOmnibox:YES];
    }
  } else {
    if ([delegate_ currentBrowserState] -> IsOffTheRecord() !=
                                               params.in_incognito) {
      // Must take a snapshot of the tab before we switch the incognito mode
      // because the currentTab will change after the switch.
      web::WebState* currentWebState =
          [delegate_ currentTabModel].webStateList->GetActiveWebState();
      if (currentWebState) {
        SnapshotTabHelper::FromWebState(currentWebState)
            ->UpdateSnapshotWithCallback(nil);
      }

      // Not for this browser state, switch and try again.
      ApplicationMode mode = params.in_incognito ? ApplicationMode::INCOGNITO
                                                 : ApplicationMode::NORMAL;
      [delegate_ expectNewForegroundTabForMode:mode];
      [delegate_ setCurrentInterfaceForMode:mode];
      LoadUrlInNewTab(params);
      return;
    }

    // TODO(crbug.com/907527): move the following lines to Browser level making
    // openNewTabFromOriginPoint a delegate there. openNewTabFromOriginPoint is
    // only called from here.
    [delegate_ openNewTabFromOriginPoint:params.origin_point
                            focusOmnibox:params.should_focus_omnibox];
  }
}

ios::ChromeBrowserState* AppUrlLoadingService::GetCurrentBrowserState() {
  return [delegate_ currentBrowserState];
}
