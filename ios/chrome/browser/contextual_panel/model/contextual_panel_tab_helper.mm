// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

ContextualPanelTabHelper::ContextualPanelTabHelper(
    web::WebState* web_state,
    std::map<ContextualPanelItemType, raw_ptr<ContextualPanelModel>> models)
    : web_state_(web_state), models_(models), weak_ptr_factory_(this) {
  web_state_observation_.Observe(web_state_);
}

ContextualPanelTabHelper::~ContextualPanelTabHelper() {
  for (auto& observer : observers_) {
    observer.ContextualPanelTabHelperDestroyed(this);
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(ContextualPanelTabHelper)

void ContextualPanelTabHelper::AddObserver(
    ContextualPanelTabHelperObserver* observer) {
  observers_.AddObserver(observer);
}

void ContextualPanelTabHelper::RemoveObserver(
    ContextualPanelTabHelperObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool ContextualPanelTabHelper::HasCachedConfigsAvailable() {
  return !sorted_weak_configurations_.empty();
}

base::WeakPtr<ContextualPanelItemConfiguration>
ContextualPanelTabHelper::GetFirstCachedConfig() {
  return HasCachedConfigsAvailable() ? sorted_weak_configurations_[0] : nullptr;
}

bool ContextualPanelTabHelper::WasLargeEntrypointShown() {
  return large_entrypoint_shown_for_curent_page_navigation_;
}

void ContextualPanelTabHelper::SetLargeEntrypointShown(bool shown) {
  large_entrypoint_shown_for_curent_page_navigation_ = shown;
}

#pragma mark - WebStateObserver

void ContextualPanelTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state_, web_state);

  // If the navigation was started for the same document, do nothing.
  if (navigation_context->IsSameDocument()) {
    return;
  }

  large_entrypoint_shown_for_curent_page_navigation_ = false;

  // Clear the configs and notify the observers.
  sorted_weak_configurations_.clear();
  for (auto& observer : observers_) {
    observer.ContextualPanelHasNewData(this, sorted_weak_configurations_);
  }
}

// Asks the individual panel models for if they have an item.
void ContextualPanelTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state_, web_state);
  QueryModels();
}

void ContextualPanelTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_observation_.Reset();
  web_state_ = nullptr;
}

void ContextualPanelTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  DCHECK_EQ(web_state_, web_state);
}

#pragma mark - Private

void ContextualPanelTabHelper::QueryModels() {
  responses_.clear();

  // First, create all the response objects, to track completed responses
  // correctly if a response returns synchronously.
  for (const auto& [key, model] : models_) {
    if (!model) {
      continue;
    }
    responses_[key] = ModelResponse();
  }

  // Second, query all the models.
  for (const auto& [key, model] : models_) {
    model->FetchConfigurationForWebState(
        web_state_,
        base::BindOnce(&ContextualPanelTabHelper::ModelCallbackReceived,
                       weak_ptr_factory_.GetWeakPtr(), key));
  }
}

void ContextualPanelTabHelper::ModelCallbackReceived(
    ContextualPanelItemType item_type,
    std::unique_ptr<ContextualPanelItemConfiguration> configuration) {
  DCHECK(!responses_[item_type].completed);
  responses_[item_type] = ModelResponse(std::move(configuration));

  // Check if all models have returned.
  for (const auto& [key, response] : responses_) {
    if (!response.completed) {
      return;
    }
  }
  AllRequestsFinished();
}

void ContextualPanelTabHelper::AllRequestsFinished() {
  sorted_weak_configurations_.clear();

  // The active configurations passed to observers as weak ptrs.
  // TODO(crbug.com/332927986): See if this can be an instance variable passed
  // as a reference once the UI lifetime is more stable.
  for (const auto& [key, response] : responses_) {
    DCHECK(response.completed);

    if (response.configuration) {
      sorted_weak_configurations_.push_back(
          response.configuration->AsWeakPtr());
    }
  }

  // Sort configurations so the highest relevance is first.
  std::sort(sorted_weak_configurations_.begin(),
            sorted_weak_configurations_.end(),
            [](base::WeakPtr<ContextualPanelItemConfiguration> first,
               base::WeakPtr<ContextualPanelItemConfiguration> second) {
              if (!first) {
                return false;
              }
              if (!second) {
                return true;
              }
              return first->relevance > second->relevance;
            });

  for (auto& observer : observers_) {
    observer.ContextualPanelHasNewData(this, sorted_weak_configurations_);
  }
}

ContextualPanelTabHelper::ModelResponse::ModelResponse()
    : completed(false), configuration(nullptr) {}

ContextualPanelTabHelper::ModelResponse::ModelResponse(
    std::unique_ptr<ContextualPanelItemConfiguration>&& configuration)
    : completed(true), configuration(std::move(configuration)) {}

ContextualPanelTabHelper::ModelResponse::~ModelResponse() {}
