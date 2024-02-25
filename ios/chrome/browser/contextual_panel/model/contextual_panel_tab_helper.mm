// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model_service.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/web/public/web_state.h"

ContextualPanelTabHelper::ContextualPanelTabHelper(web::WebState* web_state)
    : web_state_(web_state), weak_ptr_factory_(this) {
  web_state_observation_.Observe(web_state_);
  ContextualPanelModelService* model_service =
      ContextualPanelModelServiceFactory::GetForBrowserState(
          ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState()));
  models_ = model_service->models();
}

ContextualPanelTabHelper::~ContextualPanelTabHelper() = default;

WEB_STATE_USER_DATA_KEY_IMPL(ContextualPanelTabHelper)

#pragma mark - WebStateObserver

// Asks the individual panel models for if they have an item.
void ContextualPanelTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  CleanUpModels();
  for (base::WeakPtr<ContextualPanelModel> model : models_) {
    if (!model) {
      continue;
    }
    model->FetchConfigurationForWebState(
        web_state,
        base::BindOnce(&ContextualPanelTabHelper::ModelCallbackReceived,
                       weak_ptr_factory_.GetWeakPtr(), model));
  }
}

void ContextualPanelTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_observation_.Reset();
  web_state_ = nullptr;
}

void ContextualPanelTabHelper::ModelCallbackReceived(
    base::WeakPtr<ContextualPanelModel> model,
    ContextualPanelItemConfiguration configuration) {
  // Store received configurations and alert.
}

void ContextualPanelTabHelper::CleanUpModels() {
  models_.erase(std::remove_if(models_.begin(), models_.end(),
                               [](base::WeakPtr<ContextualPanelModel> model) {
                                 return !model;
                               }),
                models_.end());
}
