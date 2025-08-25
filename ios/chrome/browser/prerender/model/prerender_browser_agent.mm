// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/model/prerender_browser_agent.h"

#import "ios/chrome/browser/prerender/model/prerender_service.h"
#import "ios/chrome/browser/prerender/model/prerender_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

PrerenderBrowserAgent::~PrerenderBrowserAgent() = default;

PrerenderBrowserAgent::PrerenderBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {}

void PrerenderBrowserAgent::SetDelegate(
    id<PreloadControllerDelegate> delegate) {
  PrerenderServiceFactory::GetForProfile(browser_->GetProfile())
      ->SetDelegate(delegate);
}

void PrerenderBrowserAgent::StartPrerender(const GURL& url,
                                           const web::Referrer& referrer,
                                           ui::PageTransition transition,
                                           PrerenderPolicy policy) {
  PrerenderServiceFactory::GetForProfile(browser_->GetProfile())
      ->StartPrerender(url, referrer, transition,
                       browser_->GetWebStateList()->GetActiveWebState(),
                       /*immediately=*/policy == PrerenderPolicy::kNoDelay);
}

bool PrerenderBrowserAgent::ValidatePrerender(const GURL& url,
                                              ui::PageTransition transition) {
  return PrerenderServiceFactory::GetForProfile(browser_->GetProfile())
      ->MaybeLoadPrerenderedURL(url, transition, browser_);
}

bool PrerenderBrowserAgent::IsInsertingPrerender() const {
  return PrerenderServiceFactory::GetForProfile(browser_->GetProfile())
      ->IsLoadingPrerender();
}

void PrerenderBrowserAgent::CancelPrerender() {
  PrerenderServiceFactory::GetForProfile(browser_->GetProfile())
      ->CancelAllPrerenders();
}
