// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/stringprintf.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer.h"
#import "ios/chrome/browser/contextual_panel/utils/contextual_panel_metrics.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "ui/base/page_transition_types.h"

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

std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
ContextualPanelTabHelper::GetCurrentCachedConfigurations() {
  return sorted_weak_configurations_;
}

base::WeakPtr<ContextualPanelItemConfiguration>
ContextualPanelTabHelper::GetFirstCachedConfig() {
  return HasCachedConfigsAvailable() ? sorted_weak_configurations_[0] : nullptr;
}

void ContextualPanelTabHelper::SetContextualSheetHandler(
    id<ContextualSheetCommands> handler) {
  contextual_sheet_handler_ = handler;
}

bool ContextualPanelTabHelper::IsContextualPanelCurrentlyOpened() {
  return is_contextual_panel_currently_opened_;
}

void ContextualPanelTabHelper::OpenContextualPanel() {
  if (is_contextual_panel_currently_opened_) {
    return;
  }
  is_contextual_panel_currently_opened_ = true;
  for (auto& observer : observers_) {
    observer.ContextualPanelOpened(this);
  }
}

void ContextualPanelTabHelper::CloseContextualPanel() {
  if (!is_contextual_panel_currently_opened_) {
    return;
  }
  is_contextual_panel_currently_opened_ = false;
  for (auto& observer : observers_) {
    observer.ContextualPanelClosed(this);
  }
}

bool ContextualPanelTabHelper::WasLoudMomentEntrypointShown() {
  return loud_moment_entrypoint_shown_for_curent_page_navigation_;
}

void ContextualPanelTabHelper::SetLoudMomentEntrypointShown(bool shown) {
  loud_moment_entrypoint_shown_for_curent_page_navigation_ = shown;
}

std::optional<ContextualPanelTabHelper::EntrypointMetricsData>&
ContextualPanelTabHelper::GetMetricsData() {
  return metrics_data_;
}

void ContextualPanelTabHelper::SetMetricsData(
    ContextualPanelTabHelper::EntrypointMetricsData data) {
  metrics_data_ = data;
}

bool ContextualPanelTabHelper::ShouldRefreshData(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Refresh data if navigation is to a new URL (ignoring ref) or a new
  // document.
  return previous_url_ != navigation_context->GetUrl().GetWithoutRef() ||
         !navigation_context->IsSameDocument();
}

#pragma mark - WebStateObserver

void ContextualPanelTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state_, web_state);

  if (!ShouldRefreshData(web_state, navigation_context)) {
    return;
  }

  if (IsContextualPanelCurrentlyOpened()) {
    base::UmaHistogramEnumeration(
        "IOS.ContextualPanel.DismissedReason",
        ContextualPanelDismissedReason::NavigationInitiated);
    [contextual_sheet_handler_ hideContextualSheet];
    CloseContextualPanel();
  }

  metrics_data_ = std::nullopt;
  loud_moment_entrypoint_shown_for_curent_page_navigation_ = false;

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

  if (!ShouldRefreshData(web_state, navigation_context)) {
    return;
  }

  // Don't track the URL's ref.
  previous_url_ = navigation_context->GetUrl().GetWithoutRef();

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

void ContextualPanelTabHelper::WasShown(web::WebState* web_state) {
  if (IsContextualPanelCurrentlyOpened()) {
    [contextual_sheet_handler_ showContextualSheetUIIfActive];
  }
}

void ContextualPanelTabHelper::WasHidden(web::WebState* web_state) {
  if (IsContextualPanelCurrentlyOpened()) {
    base::UmaHistogramEnumeration("IOS.ContextualPanel.DismissedReason",
                                  ContextualPanelDismissedReason::TabChanged);
    [contextual_sheet_handler_ hideContextualSheet];
  }
}

#pragma mark - Private

void ContextualPanelTabHelper::QueryModels() {
  // Invalidate existing weak pointers to cancel any in-flight
  // fetches/callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  responses_.clear();

  request_start_time_ = base::Time::Now();

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
  if (configuration) {
    DCHECK_EQ(item_type, configuration->item_type);
  }
  responses_[item_type] = ModelResponse(std::move(configuration));

  std::string histogram_name =
      base::StringPrintf("IOS.ContextualPanel.%s.ModelResponseTime",
                         StringForItemType(item_type).c_str());
  base::UmaHistogramTimes(histogram_name,
                          base::Time::Now() - request_start_time_);

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
          response.configuration->weak_ptr_factory.GetWeakPtr());
    }
  }

  // Sort configurations so the highest relevance is first.
  std::sort(sorted_weak_configurations_.begin(),
            sorted_weak_configurations_.end(),
            [](const base::WeakPtr<ContextualPanelItemConfiguration>& first,
               const base::WeakPtr<ContextualPanelItemConfiguration>& second) {
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

  FireRequestsFinishedMetrics();
}

void ContextualPanelTabHelper::FireRequestsFinishedMetrics() {
  base::UmaHistogramExactLinear(
      "IOS.ContextualPanel.Model.InfoBlocksWithContentCount",
      sorted_weak_configurations_.size(),
      static_cast<int>(ContextualPanelItemType::kMaxValue));

  for (const auto& [key, response] : responses_) {
    std::string item_type = StringForItemType(key);
    std::string histogram_name =
        std::string("IOS.ContextualPanel.Model.Relevance.").append(item_type);
    ModelRelevanceType relevance_type;
    if (!response.configuration) {
      relevance_type = ModelRelevanceType::NoData;
    } else {
      int relevance = response.configuration->relevance;
      if (relevance >= ContextualPanelItemConfiguration::high_relevance) {
        relevance_type = ModelRelevanceType::High;
      } else {
        relevance_type = ModelRelevanceType::Low;
      }
    }
    base::UmaHistogramEnumeration(histogram_name, relevance_type);
  }
}

ContextualPanelTabHelper::ModelResponse::ModelResponse()
    : completed(false), configuration(nullptr) {}

ContextualPanelTabHelper::ModelResponse::ModelResponse(
    std::unique_ptr<ContextualPanelItemConfiguration>&& configuration)
    : completed(true), configuration(std::move(configuration)) {}

ContextualPanelTabHelper::ModelResponse::~ModelResponse() {}
