// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/sync_error_browser_agent.h"

#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/authentication/re_signin_infobar_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(SyncErrorBrowserAgent)

SyncErrorBrowserAgent::SyncErrorBrowserAgent(Browser* browser)
    : browser_(browser) {
  DCHECK(browser_);
  browser->AddObserver(this);
  browser->GetWebStateList()->AddObserver(this);
}

SyncErrorBrowserAgent::~SyncErrorBrowserAgent() {
  DCHECK(!browser_);
}

void SyncErrorBrowserAgent::SetUIProviders(
    id<SigninPresenter> signin_presenter_provider,
    id<SyncPresenter> sync_presenter_provider) {
  DCHECK(signin_presenter_provider);
  DCHECK(sync_presenter_provider);
  signin_presenter_provider_ = signin_presenter_provider;
  sync_presenter_provider_ = sync_presenter_provider;

  // Re-evaluate all web states.
  web_state_observations_.RemoveAllObservations();
  WebStateList* web_state_list = browser_->GetWebStateList();
  for (int i = 0; i < web_state_list->count(); i++) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    CreateReSignInInfoBarDelegate(web_state);
  }
}

void SyncErrorBrowserAgent::ClearUIProviders() {
  signin_presenter_provider_ = nil;
  sync_presenter_provider_ = nil;
}

// Browser Observer methods:
void SyncErrorBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser, browser_);
  browser->GetWebStateList()->RemoveObserver(this);
  browser->RemoveObserver(this);
  browser_ = nullptr;
}

// WesStateList Observer methods:
void SyncErrorBrowserAgent::WebStateInsertedAt(WebStateList* web_state_list,
                                               web::WebState* web_state,
                                               int index,
                                               bool activating) {
  CreateReSignInInfoBarDelegate(web_state);
}

void SyncErrorBrowserAgent::WebStateReplacedAt(WebStateList* web_state_list,
                                               web::WebState* old_web_state,
                                               web::WebState* new_web_state,
                                               int index) {
  if (!old_web_state->IsRealized()) {
    web_state_observations_.RemoveObservation(old_web_state);
  }
  CreateReSignInInfoBarDelegate(new_web_state);
}

void SyncErrorBrowserAgent::WebStateDetachedAt(WebStateList* web_state_list,
                                               web::WebState* web_state,
                                               int index) {
  if (!web_state->IsRealized()) {
    web_state_observations_.RemoveObservation(web_state);
  }
}

void SyncErrorBrowserAgent::WebStateDestroyed(web::WebState* web_state) {
  web_state_observations_.RemoveObservation(web_state);
}

void SyncErrorBrowserAgent::WebStateRealized(web::WebState* web_state) {
  web_state_observations_.RemoveObservation(web_state);
  CreateReSignInInfoBarDelegate(web_state);
}

void SyncErrorBrowserAgent::CreateReSignInInfoBarDelegate(
    web::WebState* web_state) {
  if (!web_state->IsRealized()) {
    web_state_observations_.AddObservation(web_state);
    return;
  }

  if (!signin_presenter_provider_ || !sync_presenter_provider_) {
    return;
  }

  ChromeBrowserState* browser_state = browser_->GetBrowserState();
  if (!ReSignInInfoBarDelegate::Create(browser_state, web_state,
                                       signin_presenter_provider_)) {
    DisplaySyncErrors(browser_state, web_state, sync_presenter_provider_);
  }
}
