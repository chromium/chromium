// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/page_placeholder_browser_agent.h"

#import "base/check.h"
#import "base/check_op.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tabs/model/features.h"
#import "ios/chrome/browser/web/model/page_placeholder_tab_helper.h"
#import "ios/web/public/web_state.h"

PagePlaceholderBrowserAgent::PagePlaceholderBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {
  // All the BrowserAgent are attached to the Browser during the creation,
  // the WebStateList must be empty at this point.
  DCHECK(browser_->GetWebStateList()->empty())
      << "PagePlaceholderBrowserAgent created for a Browser with a non-empty "
         "WebStateList.";

  ProfileIOS* profile = browser_->GetProfile();
  session_restoration_service_observation_.Observe(
      SessionRestorationServiceFactory::GetForProfile(profile));
}

PagePlaceholderBrowserAgent::~PagePlaceholderBrowserAgent() = default;

#pragma mark - Public

void PagePlaceholderBrowserAgent::ExpectNewForegroundTab() {
  expecting_foreground_tab_ = true;
}

void PagePlaceholderBrowserAgent::AddPagePlaceholder() {
  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  if (web_state && expecting_foreground_tab_) {
    PagePlaceholderTabHelper::FromWebState(web_state)
        ->AddPlaceholderForNextNavigation();
  }
}

void PagePlaceholderBrowserAgent::CancelPagePlaceholder() {
  if (!expecting_foreground_tab_) {
    return;
  }

  // Now that the new tab has been displayed, return to normal. Rather than
  // keep a reference to the previous tab, just turn off preview mode for all
  // tabs (since doing so is a no-op for the tabs that don't have it set).
  expecting_foreground_tab_ = false;

  WebStateList* web_state_list = browser_->GetWebStateList();
  const int web_state_list_size = web_state_list->count();
  for (int index = 0; index < web_state_list_size; ++index) {
    web::WebState* web_state_at_index = web_state_list->GetWebStateAt(index);
    PagePlaceholderTabHelper::FromWebState(web_state_at_index)
        ->CancelPlaceholderForNextNavigation();
  }
}

#pragma mark - SessionRestorationObserver

void PagePlaceholderBrowserAgent::WillStartSessionRestoration(
    Browser* browser) {
  // Nothing to do.
}

void PagePlaceholderBrowserAgent::SessionRestorationFinished(
    Browser* browser,
    const std::vector<web::WebState*>& restored_web_states) {
  // Ignore the event if it does not correspond to the browser this
  // object is bound to (which can happen with the optimised session
  // storage code).
  if (browser_.get() != browser) {
    return;
  }

  // Setup the placeholder for the restored tabs if necessary.
  for (web::WebState* web_state : restored_web_states) {
    AddPlaceholderToWebState(web_state);
  }
}

void PagePlaceholderBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  CHECK(CreateTabHelperOnlyForRealizedWebStates());
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Nothing to do.
      break;

    case WebStateListChange::Type::kDetach:
      StopObservingWebState(
          change.As<WebStateListChangeDetach>().detached_web_state());
      break;

    case WebStateListChange::Type::kMove:
      // Nothing do do.
      break;

    case WebStateListChange::Type::kReplace:
      StopObservingWebState(
          change.As<WebStateListChangeReplace>().replaced_web_state());
      break;

    case WebStateListChange::Type::kInsert:
      // Nothing to do.
      break;

    case WebStateListChange::Type::kGroupCreate:
      // Nothing to do.
      break;

    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Nothing to do.
      break;

    case WebStateListChange::Type::kGroupMove:
      // Nothing to do.
      break;

    case WebStateListChange::Type::kGroupDelete:
      // Nothing to do.
      break;
  }
}

void PagePlaceholderBrowserAgent::WebStateRealized(web::WebState* web_state) {
  CHECK(CreateTabHelperOnlyForRealizedWebStates());
  AddPlaceholderToWebState(web_state);
  StopObservingWebState(web_state);
}

void PagePlaceholderBrowserAgent::WebStateDestroyed(web::WebState* web_state) {
  CHECK(CreateTabHelperOnlyForRealizedWebStates());
  StopObservingWebState(web_state);
}

void PagePlaceholderBrowserAgent::StartObservingWebState(
    web::WebState* web_state) {
  CHECK(CreateTabHelperOnlyForRealizedWebStates());
  if (!web_state_observations_.IsObservingAnySource()) {
    web_state_list_observation_.Observe(browser_->GetWebStateList());
  }
  web_state_observations_.AddObservation(web_state);
}

void PagePlaceholderBrowserAgent::StopObservingWebState(
    web::WebState* web_state) {
  CHECK(CreateTabHelperOnlyForRealizedWebStates());
  if (web_state_observations_.IsObservingSource(web_state)) {
    web_state_observations_.RemoveObservation(web_state);
    if (!web_state_observations_.IsObservingAnySource()) {
      web_state_list_observation_.Reset();
    }
  }
}

void PagePlaceholderBrowserAgent::AddPlaceholderToWebState(
    web::WebState* web_state) {
  if (CreateTabHelperOnlyForRealizedWebStates()) {
    if (!web_state->IsRealized()) {
      StartObservingWebState(web_state);
      return;
    }
  }

  const GURL& visible_url = web_state->GetVisibleURL();
  if (visible_url.is_valid() && visible_url != kChromeUINewTabURL) {
    PagePlaceholderTabHelper::FromWebState(web_state)
        ->AddPlaceholderForNextNavigation();
  }
}
