// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/page_placeholder_browser_agent.h"

#import "base/check.h"
#import "base/check_op.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web/model/page_placeholder_tab_helper.h"
#import "ios/web/common/features.h"
#import "ios/web/public/web_state.h"

namespace {

// Returns whether a page placeholder should be installed on `web_state`
// when it is realized.
bool ShouldInstallPagePlaceholder(web::WebState* web_state) {
  const GURL& visible_url = web_state->GetVisibleURL();
  return visible_url.is_valid() && visible_url != kChromeUINewTabURL;
}

}  // namespace

PagePlaceholderBrowserAgent::PagePlaceholderBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {
  // All the BrowserAgent are attached to the Browser during the creation,
  // the WebStateList must be empty at this point.
  DCHECK(browser_->GetWebStateList()->empty())
      << "PagePlaceholderBrowserAgent created for a Browser with a non-empty "
         "WebStateList.";

  web_state_list_observation_.Observe(browser_->GetWebStateList());
}

PagePlaceholderBrowserAgent::~PagePlaceholderBrowserAgent() = default;

bool PagePlaceholderBrowserAgent::IsPagePlaceholderPlannedForWebState(
    web::WebState* web_state) {
  if (web::features::CreateTabHelperOnlyForRealizedWebStates()) {
    if (!web_state->IsRealized()) {
      return ShouldInstallPagePlaceholder(web_state);
    }
  }

  return PagePlaceholderTabHelper::FromWebState(web_state)
      ->will_add_placeholder_for_next_navigation();
}

void PagePlaceholderBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Nothing to do.
      break;

    case WebStateListChange::Type::kDetach: {
      const auto& detach_change = change.As<WebStateListChangeDetach>();
      WebStateRemoved(detach_change.detached_web_state());
      break;
    }

    case WebStateListChange::Type::kMove:
      // Nothing do do.
      break;

    case WebStateListChange::Type::kReplace: {
      const bool force_placeholder = web_state_list->IsBatchInProgress();
      const auto& replace_change = change.As<WebStateListChangeReplace>();
      WebStateInserted(replace_change.inserted_web_state(), force_placeholder);
      WebStateRemoved(replace_change.replaced_web_state());
      break;
    }

    case WebStateListChange::Type::kInsert: {
      const bool force_placeholder = web_state_list->IsBatchInProgress();
      const auto& insert_change = change.As<WebStateListChangeInsert>();
      WebStateInserted(insert_change.inserted_web_state(), force_placeholder);
      break;
    }

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
  CHECK(web::features::CreateTabHelperOnlyForRealizedWebStates());
  web_state_observations_.RemoveObservation(web_state);
  AddPlaceholderToWebState(web_state);
}

void PagePlaceholderBrowserAgent::WebStateDestroyed(web::WebState* web_state) {
  CHECK(web::features::CreateTabHelperOnlyForRealizedWebStates());
  web_state_observations_.RemoveObservation(web_state);
}

void PagePlaceholderBrowserAgent::WebStateInserted(web::WebState* web_state,
                                                   bool force_placeholder) {
  if (web::features::CreateTabHelperOnlyForRealizedWebStates()) {
    if (!web_state->IsRealized()) {
      web_state_observations_.AddObservation(web_state);
      return;
    }
  }

  if (!web_state->IsRealized() || force_placeholder) {
    AddPlaceholderToWebState(web_state);
  }
}

void PagePlaceholderBrowserAgent::WebStateRemoved(web::WebState* web_state) {
  if (web::features::CreateTabHelperOnlyForRealizedWebStates()) {
    if (!web_state->IsRealized()) {
      web_state_observations_.RemoveObservation(web_state);
      return;
    }
  }
}

void PagePlaceholderBrowserAgent::AddPlaceholderToWebState(
    web::WebState* web_state) {
  if (ShouldInstallPagePlaceholder(web_state)) {
    PagePlaceholderTabHelper::FromWebState(web_state)
        ->AddPlaceholderForNextNavigation();
  }
}
