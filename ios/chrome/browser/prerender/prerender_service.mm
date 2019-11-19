// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/prerender_service.h"

#include "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/prerender/preload_controller.h"
#import "ios/chrome/browser/sessions/session_window_restoring.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/web/load_timing_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#include "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PrerenderService::PrerenderService(ios::ChromeBrowserState* browser_state)
    : controller_(
          [[PreloadController alloc] initWithBrowserState:browser_state]),
      loading_prerender_(false) {}

PrerenderService::~PrerenderService() {}

void PrerenderService::Shutdown() {
  [controller_ browserStateDestroyed];
  controller_ = nil;
}

void PrerenderService::SetDelegate(id<PreloadControllerDelegate> delegate) {
  controller_.delegate = delegate;
}

void PrerenderService::StartPrerender(const GURL& url,
                                      const web::Referrer& referrer,
                                      ui::PageTransition transition,
                                      bool immediately) {
  [controller_ prerenderURL:url
                   referrer:referrer
                 transition:transition
                immediately:immediately];
}

bool PrerenderService::MaybeLoadPrerenderedURL(
    const GURL& url,
    ui::PageTransition transition,
    WebStateList* web_state_list,
    id<SessionWindowRestoring> restorer) {
  if (!HasPrerenderForUrl(url)) {
    CancelPrerender();
    return false;
  }

  std::unique_ptr<web::WebState> new_web_state =
      [controller_ releasePrerenderContents];
  if (!new_web_state) {
    CancelPrerender();
    return false;
  }

  // Due to some security workarounds inside ios/web, sometimes a restored
  // webState may mark new navigations as renderer initiated instead of browser
  // initiated. As a result 'visible url' of preloaded web state will be
  // 'last committed  url', and not 'url typed by the user'. As these
  // navigations are uncommitted, and make the omnibox (or NTP) look strange,
  // simply drop them.  See crbug.com/1020497 for the strange UI, and
  // crbug.com/1010765 for the triggering security fixes.
  if (web_state_list->GetActiveWebState()->GetVisibleURL() ==
      new_web_state->GetVisibleURL()) {
    CancelPrerender();
    return false;
  }

  DCHECK_NE(WebStateList::kInvalidIndex, web_state_list->active_index());

  web::NavigationManager* active_navigation_manager =
      web_state_list->GetActiveWebState()->GetNavigationManager();
  int lastIndex = active_navigation_manager->GetLastCommittedItemIndex();
  UMA_HISTOGRAM_COUNTS_100("Prerender.PrerenderLoadedOnIndex", lastIndex);

  BOOL onFirstNTP =
      IsVisibleURLNewTabPage(web_state_list->GetActiveWebState()) &&
      lastIndex == 0;
  UMA_HISTOGRAM_BOOLEAN("Prerender.PrerenderLoadedOnFirstNTP", onFirstNTP);

  web::NavigationManager* new_navigation_manager =
      new_web_state->GetNavigationManager();

  bool slim_navigation_manager_enabled =
      web::GetWebClient()->IsSlimNavigationManagerEnabled();
  if (new_navigation_manager->CanPruneAllButLastCommittedItem() ||
      slim_navigation_manager_enabled) {
    if (!slim_navigation_manager_enabled) {
      new_navigation_manager->CopyStateFromAndPrune(active_navigation_manager);
    }
    loading_prerender_ = true;
    web_state_list->ReplaceWebStateAt(web_state_list->active_index(),
                                      std::move(new_web_state));
    loading_prerender_ = false;
    // new_web_state is now null after the std::move, so grab a new pointer to
    // it for further updates.
    web::WebState* active_web_state = web_state_list->GetActiveWebState();

    bool typed_or_generated_transition =
        PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) ||
        PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_GENERATED);
    if (typed_or_generated_transition) {
      LoadTimingTabHelper::FromWebState(active_web_state)
          ->DidPromotePrerenderTab();
    }

    [restorer saveSessionImmediately:NO];
    return true;
  }

  CancelPrerender();
  return false;
}

void PrerenderService::CancelPrerender() {
  [controller_ cancelPrerender];
}

bool PrerenderService::HasPrerenderForUrl(const GURL& url) {
  return url == controller_.prerenderedURL;
}

bool PrerenderService::IsWebStatePrerendered(web::WebState* web_state) {
  return [controller_ isWebStatePrerendered:web_state];
}
