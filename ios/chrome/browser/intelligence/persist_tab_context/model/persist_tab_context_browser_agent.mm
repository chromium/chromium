// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_browser_agent.h"

#import "base/strings/string_number_conversions.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

// TODO(crbug.com/445963646): Add metrics logging to browser agent.
// TODO(crbug.com/447646545): Add test coverage for persist tab context

PersistTabContextBrowserAgent::PersistTabContextBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {
  StartObserving(browser_->GetWebStateList(), Policy::kAccordingToFeature);
  persist_tab_context_state_agent_ = [[PersistTabContextStateAgent alloc]
      initWithTransitionCallback:
          base::BindRepeating(
              &PersistTabContextBrowserAgent::OnSceneActivationLevelChanged,
              weak_factory_.GetWeakPtr())];
  [browser->GetSceneState() addObserver:persist_tab_context_state_agent_];
}

PersistTabContextBrowserAgent::~PersistTabContextBrowserAgent() {
  StopObserving();
  [browser_->GetSceneState() removeObserver:persist_tab_context_state_agent_];
  page_context_wrapper_ = nil;
  web_state_observation_.Reset();
}

void PersistTabContextBrowserAgent::OnSceneActivationLevelChanged(
    SceneActivationLevel level) {
  if (level != SceneActivationLevelBackground) {
    return;
  }

  web::WebState* active_web_state = web_state_observation_.GetSource();

  if (active_web_state) {
    WasHidden(active_web_state);
  }
}

void PersistTabContextBrowserAgent::OnPageContextExtracted(
    const std::string& webstate_unique_id,
    PageContextWrapperCallbackResponse response) {
  if (!response.has_value()) {
    return;
  }

  // TODO(crbug.com/445959483): Serialize page context in response to the format
  // in which it will be stored.
  // TODO(crbug.com/447356981): Write serialized page context to storage.
}

void PersistTabContextBrowserAgent::WasHidden(web::WebState* web_state) {
  if (!web_state) {
    return;
  }

  // Cancel any ongoing page context operation.
  if (page_context_wrapper_) {
    page_context_wrapper_ = nil;
  }

  std::string webstate_unique_id =
      base::NumberToString(web_state->GetUniqueIdentifier().identifier());

  page_context_wrapper_ = [[PageContextWrapper alloc]
        initWithWebState:web_state
      completionCallback:
          base::BindOnce(&PersistTabContextBrowserAgent::OnPageContextExtracted,
                         weak_factory_.GetWeakPtr(), webstate_unique_id)];
  [page_context_wrapper_ setShouldGetAnnotatedPageContent:YES];
  [page_context_wrapper_ setShouldGetInnerText:YES];
  [page_context_wrapper_ populatePageContextFieldsAsync];
}

void PersistTabContextBrowserAgent::OnWebStateInserted(
    web::WebState* web_state) {
  // Nothing to do.
}

void PersistTabContextBrowserAgent::OnWebStateRemoved(
    web::WebState* web_state) {
  // Nothing to do.
}

void PersistTabContextBrowserAgent::OnWebStateDeleted(
    web::WebState* web_state) {
  // TODO(crbug.com/447358513): Read and deserialize page context from storage.
}

void PersistTabContextBrowserAgent::OnActiveWebStateChanged(
    web::WebState* old_active,
    web::WebState* new_active) {
  web_state_observation_.Reset();
  if (new_active) {
    web_state_observation_.Observe(new_active);
  }
}
