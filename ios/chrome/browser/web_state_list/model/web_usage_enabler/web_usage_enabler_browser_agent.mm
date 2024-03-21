// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"

#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent_observer.h"
#import "ios/web/public/navigation/navigation_manager.h"

BROWSER_USER_DATA_KEY_IMPL(WebUsageEnablerBrowserAgent)

WebUsageEnablerBrowserAgent::WebUsageEnablerBrowserAgent(Browser* browser)
    : browser_(browser) {
  browser_observation_.Observe(browser_.get());

  WebStateList* web_state_list = browser_->GetWebStateList();
  web_state_list_observation_.Observe(browser->GetWebStateList());

  // All the BrowserAgent are attached to the Browser during the creation,
  // the WebStateList must be empty at this point.
  DCHECK(web_state_list->empty()) << "WebUsageEnablerBrowserAgent created for "
                                     "a Browser with a non-empty WebStateList.";
}

WebUsageEnablerBrowserAgent::~WebUsageEnablerBrowserAgent() = default;

bool WebUsageEnablerBrowserAgent::IsWebUsageEnabled() const {
  return web_usage_enabled_;
}

void WebUsageEnablerBrowserAgent::SetWebUsageEnabled(bool web_usage_enabled) {
  if (web_usage_enabled_ == web_usage_enabled) {
    return;
  }

  web_usage_enabled_ = web_usage_enabled;
  UpdateWebUsageForAllWebStates();
  for (auto& observer : observers_) {
    observer.WebUsageEnablerValueChanged(this);
  }
}

void WebUsageEnablerBrowserAgent::AddObserver(
    WebUsageEnablerBrowserAgentObserver* observer) {
  observers_.AddObserver(observer);
}

void WebUsageEnablerBrowserAgent::RemoveObserver(
    WebUsageEnablerBrowserAgentObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WebUsageEnablerBrowserAgent::UpdateWebUsageForAllWebStates() {
  WebStateList* web_state_list = browser_->GetWebStateList();
  for (int index = 0; index < web_state_list->count(); ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    UpdateWebUsageForAddedWebState(web_state, /*triggers_initial_load=*/false);
  }
}

void WebUsageEnablerBrowserAgent::UpdateWebUsageForAddedWebState(
    web::WebState* web_state,
    bool triggers_initial_load) {
  if (web_state->IsRealized()) {
    web_state->SetWebUsageEnabled(web_usage_enabled_);
    if (web_usage_enabled_ && triggers_initial_load) {
      web_state->GetNavigationManager()->LoadIfNecessary();
    }
  } else if (!web_state_observations_.IsObservingSource(web_state)) {
    web_state_observations_.AddObservation(web_state);
  }
}

#pragma mark - BrowserObserver

void WebUsageEnablerBrowserAgent::BrowserDestroyed(Browser* browser) {
  web_state_observations_.RemoveAllObservations();
  web_state_list_observation_.Reset();
  browser_observation_.Reset();
}

#pragma mark - WebStateListObserver

void WebUsageEnablerBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      web::WebState* detached_web_state = detach_change.detached_web_state();
      if (web_state_observations_.IsObservingSource(detached_web_state)) {
        web_state_observations_.RemoveObservation(detached_web_state);
      }
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      web::WebState* replaced_web_state = replace_change.replaced_web_state();
      if (web_state_observations_.IsObservingSource(replaced_web_state)) {
        web_state_observations_.RemoveObservation(replaced_web_state);
      }

      UpdateWebUsageForAddedWebState(replace_change.inserted_web_state(),
                                     /*triggers_initial_load=*/true);
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      UpdateWebUsageForAddedWebState(
          insert_change.inserted_web_state(),
          /*triggers_initial_load=*/status.active_web_state_change());
      break;
    }
    case WebStateListChange::Type::kGroupCreate:
      // Do nothing when a group is created.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Do nothing when a tab group's visual data are updated.
      break;
    case WebStateListChange::Type::kGroupMove:
      // Do nothing when a tab group is moved.
      break;
    case WebStateListChange::Type::kGroupDelete:
      // Do nothing when a group is deleted.
      break;
  }
}

void WebUsageEnablerBrowserAgent::WebStateRealized(web::WebState* web_state) {
  UpdateWebUsageForAddedWebState(web_state, /*triggers_initial_load=*/false);
  web_state_observations_.RemoveObservation(web_state);
}

void WebUsageEnablerBrowserAgent::WebStateDestroyed(web::WebState* web_state) {
  web_state_observations_.RemoveObservation(web_state);
}
