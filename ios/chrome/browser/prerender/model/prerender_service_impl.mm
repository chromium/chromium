// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/model/prerender_service_impl.h"

#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/prerender/model/preload_controller.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web/model/load_timing_tab_helper.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ui/base/page_transition_types.h"

PrerenderServiceImpl::PrerenderServiceImpl(ProfileIOS* profile)
    : controller_([[PreloadController alloc] initWithProfile:profile]) {}

PrerenderServiceImpl::~PrerenderServiceImpl() = default;

void PrerenderServiceImpl::Shutdown() {
  [controller_ profileDestroyed];
  controller_ = nil;
}

void PrerenderServiceImpl::SetDelegate(id<PreloadControllerDelegate> delegate) {
  controller_.delegate = delegate;
}

void PrerenderServiceImpl::StartPrerender(const GURL& url,
                                          const web::Referrer& referrer,
                                          ui::PageTransition transition,
                                          web::WebState* web_state_to_replace,
                                          bool immediately) {
  [controller_ prerenderURL:url
                   referrer:referrer
                 transition:transition
            currentWebState:web_state_to_replace
                immediately:immediately];
}

bool PrerenderServiceImpl::MaybeLoadPrerenderedURL(
    const GURL& url,
    ui::PageTransition transition,
    Browser* browser) {
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

  WebStateList* web_state_list = browser->GetWebStateList();

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
  ProfileIOS* profile = browser->GetProfile();
  SessionRestorationServiceFactory::GetForProfile(profile)
      ->ScheduleSaveSessions();
  return true;
}

bool PrerenderServiceImpl::IsLoadingPrerender() {
  return loading_prerender_;
}

void PrerenderServiceImpl::CancelPrerender() {
  [controller_ cancelPrerender];
}

bool PrerenderServiceImpl::HasPrerenderForUrl(const GURL& url) {
  return url == controller_.prerenderedURL;
}

bool PrerenderServiceImpl::IsWebStatePrerendered(web::WebState* web_state) {
  return [controller_ isWebStatePrerendered:web_state];
}
