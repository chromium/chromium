// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/web/public/web_state.h"

ContextualPanelTabHelper::ContextualPanelTabHelper(
    web::WebState* web_state,
    std::map<ContextualPanelItemType, raw_ptr<ContextualPanelModel>> models)
    : web_state_(web_state), models_(models), weak_ptr_factory_(this) {
  web_state_observation_.Observe(web_state_);
}

ContextualPanelTabHelper::~ContextualPanelTabHelper() = default;

WEB_STATE_USER_DATA_KEY_IMPL(ContextualPanelTabHelper)

void ContextualPanelTabHelper::AddObserver(
    ContextualPanelTabHelperObserver* observer) {
  observers_.AddObserver(observer);
}

void ContextualPanelTabHelper::RemoveObserver(
    ContextualPanelTabHelperObserver* observer) {
  observers_.RemoveObserver(observer);
}

#pragma mark - WebStateObserver

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
    std::optional<ContextualPanelItemConfiguration> configuration) {
  DCHECK(!responses_[item_type].completed);
  responses_[item_type] = ModelResponse(configuration);

  // Check if all models have returned.
  for (const auto& [key, response] : responses_) {
    if (!response.completed) {
      return;
    }
  }
  AllRequestsFinished();
}

void ContextualPanelTabHelper::AllRequestsFinished() {
  std::vector<ContextualPanelItemConfiguration> configurations;
  for (const auto& [key, response] : responses_) {
    DCHECK(response.completed);

    if (response.configuration) {
      configurations.push_back(std::move(response.configuration).value());
    }
  }

  responses_.clear();

  // Sort configurations so the highest relevance is first.
  std::sort(configurations.begin(), configurations.end(),
            [](ContextualPanelItemConfiguration first,
               ContextualPanelItemConfiguration second) {
              return first.relevance > second.relevance;
            });

  for (auto& observer : observers_) {
    observer.ContextualPanelHasNewData(this, configurations);
  }
}

ContextualPanelTabHelper::ModelResponse::ModelResponse()
    : completed(false), configuration(std::nullopt) {}

ContextualPanelTabHelper::ModelResponse::ModelResponse(
    std::optional<ContextualPanelItemConfiguration> configuration)
    : completed(true), configuration(std::move(configuration)) {}

ContextualPanelTabHelper::ModelResponse::~ModelResponse() {}
