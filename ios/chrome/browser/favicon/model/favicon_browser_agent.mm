// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/favicon/model/favicon_browser_agent.h"

#import "base/check.h"
#import "components/favicon/core/favicon_service.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/common/features.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

FaviconBrowserAgent::FaviconBrowserAgent(Browser* browser)
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

FaviconBrowserAgent::~FaviconBrowserAgent() = default;

#pragma mark - SessionRestorationObserver

void FaviconBrowserAgent::WillStartSessionRestoration(Browser* browser) {
  // Nothing to do.
}

void FaviconBrowserAgent::SessionRestorationFinished(
    Browser* browser,
    const std::vector<web::WebState*>& restored_web_states) {
  // Ignore the event if it does not correspond to the browser this
  // object is bound to (which can happen with the optimised session
  // storage code).
  if (browser_.get() != browser) {
    return;
  }

  // Start fetching the favicon for the restored WebStates if necessary.
  for (web::WebState* web_state : restored_web_states) {
    FetchFaviconForWebState(web_state);
  }
}

void FaviconBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  CHECK(web::features::CreateTabHelperOnlyForRealizedWebStates());
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

void FaviconBrowserAgent::WebStateRealized(web::WebState* web_state) {
  CHECK(web::features::CreateTabHelperOnlyForRealizedWebStates());
  FetchFaviconForWebState(web_state);
  StopObservingWebState(web_state);
}

void FaviconBrowserAgent::WebStateDestroyed(web::WebState* web_state) {
  CHECK(web::features::CreateTabHelperOnlyForRealizedWebStates());
  StopObservingWebState(web_state);
}

void FaviconBrowserAgent::StartObservingWebState(web::WebState* web_state) {
  CHECK(web::features::CreateTabHelperOnlyForRealizedWebStates());
  if (!web_state_observations_.IsObservingAnySource()) {
    web_state_list_observation_.Observe(browser_->GetWebStateList());
  }
  web_state_observations_.AddObservation(web_state);
}

void FaviconBrowserAgent::StopObservingWebState(web::WebState* web_state) {
  CHECK(web::features::CreateTabHelperOnlyForRealizedWebStates());
  if (web_state_observations_.IsObservingSource(web_state)) {
    web_state_observations_.RemoveObservation(web_state);
    if (!web_state_observations_.IsObservingAnySource()) {
      web_state_list_observation_.Reset();
    }
  }
}

void FaviconBrowserAgent::FetchFaviconForWebState(web::WebState* web_state) {
  if (web::features::CreateTabHelperOnlyForRealizedWebStates()) {
    if (!web_state->IsRealized()) {
      StartObservingWebState(web_state);
      return;
    }
  }

  const GURL& visible_url = web_state->GetVisibleURL();
  if (visible_url.is_valid()) {
    favicon::WebFaviconDriver::FromWebState(web_state)->FetchFavicon(
        visible_url, /*is_same_document=*/false);
  }
}
