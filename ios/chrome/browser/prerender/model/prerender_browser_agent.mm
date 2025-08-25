// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/model/prerender_browser_agent.h"

#import "base/auto_reset.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "ios/chrome/browser/prerender/model/preload_controller.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web/model/load_timing_tab_helper.h"

PrerenderBrowserAgent::~PrerenderBrowserAgent() {
  [controller_ profileDestroyed];
  controller_ = nil;
}

PrerenderBrowserAgent::PrerenderBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {
  controller_ =
      [[PreloadController alloc] initWithProfile:browser->GetProfile()];
}

void PrerenderBrowserAgent::SetDelegate(
    id<PreloadControllerDelegate> delegate) {
  controller_.delegate = delegate;
}

void PrerenderBrowserAgent::StartPrerender(const GURL& url,
                                           const web::Referrer& referrer,
                                           ui::PageTransition transition,
                                           PrerenderPolicy policy) {
  [controller_ prerenderURL:url
                   referrer:referrer
                 transition:transition
            currentWebState:browser_->GetWebStateList()->GetActiveWebState()
                immediately:(policy == PrerenderPolicy::kNoDelay)];
}

bool PrerenderBrowserAgent::ValidatePrerender(const GURL& url,
                                              ui::PageTransition transition) {
  // Ensure that the pre-render is cancelled when this method complete.
  base::ScopedClosureRunner cancel_prerender_on_cleanup(base::BindOnce(
      &PrerenderBrowserAgent::CancelPrerender, base::Unretained(this)));

  if (controller_.prerenderedURL != url) {
    return false;
  }

  WebStateList* web_state_list = browser_->GetWebStateList();
  const int active_index = web_state_list->active_index();
  if (active_index == WebStateList::kInvalidIndex) {
    return false;
  }

  web::WebState* active_web_state = web_state_list->GetWebStateAt(active_index);
  CHECK(active_web_state);

  std::unique_ptr<web::WebState> new_web_state =
      [controller_ releasePrerenderContents];
  if (!new_web_state) {
    return false;
  }

  // Due to some security workarounds inside //ios/web, sometimes a restored
  // WebState may mark new navigations as renderer initiated instead of browser
  // initiated. As a result "visible url" of the preloaded WebState will be the
  // "last committed url" and not "url typed by the user". As there navigations
  // are uncommitted, and make the omnibox (or NTP) look stange, drop them.
  // See crbug.com/1020497 for the strange UI, and crbug.com/1010765 for the
  // triggering security fixes.
  if (active_web_state->GetVisibleURL() == new_web_state->GetVisibleURL()) {
    return false;
  }

  {
    base::AutoReset<bool> scoped_reset(&loading_prerender_, true);
    web_state_list->ReplaceWebStateAt(active_index, std::move(new_web_state));
    active_web_state = web_state_list->GetWebStateAt(active_index);
  }

  if (PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) ||
      PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_GENERATED)) {
    LoadTimingTabHelper::FromWebState(active_web_state)
        ->DidPromotePrerenderTab();
  }

  SessionRestorationServiceFactory::GetForProfile(browser_->GetProfile())
      ->ScheduleSaveSessions();

  // There is no need to call CancelPrerender(), clear the ScopedClosureRunner.
  std::ignore = cancel_prerender_on_cleanup.Release();
  return true;
}

bool PrerenderBrowserAgent::IsInsertingPrerender() const {
  return loading_prerender_;
}

void PrerenderBrowserAgent::CancelPrerender() {
  [controller_ cancelPrerender];
}
