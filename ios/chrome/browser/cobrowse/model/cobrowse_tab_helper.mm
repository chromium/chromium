// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/cobrowse_tab_helper.h"

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

CobrowseTabHelper::CobrowseTabHelper(web::WebState* web_state,
                                     TemplateURLService* template_url_service)
    : template_url_service_(template_url_service) {
  CHECK(IsAimCobrowseEnabled());
  observation_.Observe(web_state);
}

CobrowseTabHelper::~CobrowseTabHelper() = default;

#pragma mark - Public

void CobrowseTabHelper::SetSceneCommandsHandler(id<SceneCommands> handler) {
  scene_handler_ = handler;
}

void CobrowseTabHelper::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

#pragma mark - WebStateObserver

void CobrowseTabHelper::WasShown(web::WebState* web_state) {
  if (!delegate_ || !scene_handler_) {
    return;
  }

  GURL url = web_state->GetVisibleURL();
  if (ShouldHideAssistant(web_state, url)) {
    [scene_handler_ hideAssistant];
    return;
  }

  if (delegate_->IsSessionActive()) {
    // Use a task on the main queue to ensure the view hierarchy is fully
    // established before showing the assistant. This avoids crashes during
    // transitions (e.g., from Tab Grid to Browser).
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&CobrowseTabHelper::ShowAssistant,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void CobrowseTabHelper::WasHidden(web::WebState* web_state) {
  if (!scene_handler_) {
    return;
  }

  [scene_handler_ hideAssistant];
}

void CobrowseTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!delegate_ || !scene_handler_) {
    return;
  }

  // Do not trigger the assistant when the web state is not currently visible.
  // This could happen e.g. for navigations indirectly caused by the APC
  // extraction process in the tab picker.
  if (!web_state->IsVisible()) {
    return;
  }

  // Do not trigger the assistant on reloads.
  if (ui::PageTransitionCoreTypeIs(navigation_context->GetPageTransition(),
                                   ui::PAGE_TRANSITION_RELOAD)) {
    return;
  }

  const GURL& url = navigation_context->GetUrl();

  // If the session is active and we navigate to an AIM URL, update the context
  // before `ShouldHideAssistant` hides the UI and returns early. This ensures
  // follow-up AIM queries (which happen in the same tab) correctly sync their
  // URL context. Note: We cannot use `ConfigureAssistantContextForWebState`
  // here because it extracts the URL from the tab's opener, which would fail
  // to capture the new URL of a same-tab navigation.
  if (delegate_->IsSessionActive() &&
      (IsAimURL(url) || IsAimZeroStateURL(url))) {
    delegate_->SetCobrowseContext([[CobrowseContext alloc] initWithURL:url]);
  }

  if (ShouldHideAssistant(web_state, url)) {
    [scene_handler_ hideAssistant];
    return;
  }

  if (delegate_->CanShowAssistantForWebState(web_state)) {
    delegate_->ConfigureAssistantContextForWebState(web_state);
    [scene_handler_ showAssistantInMinimizedState:YES];
    delegate_->SetSessionActive(true);
  }
}

void CobrowseTabHelper::WebStateDestroyed(web::WebState* web_state) {
  observation_.Reset();
}

#pragma mark - Private helpers

void CobrowseTabHelper::ShowAssistant() {
  web::WebState* web_state = observation_.GetSource();
  if (!web_state) {
    return;
  }

  if (!web_state->IsVisible()) {
    return;
  }

  if (ShouldHideAssistant(web_state, web_state->GetVisibleURL())) {
    return;
  }

  [scene_handler_ showAssistant];
}

bool CobrowseTabHelper::ShouldHideAssistant(web::WebState* web_state,
                                            const GURL& url) {
  if (delegate_ && delegate_->ShouldHideAssistantForWebState(web_state)) {
    return true;
  }

  if (IsAimURL(url) || IsAimZeroStateURL(url)) {
    return true;
  }

  if (!url.is_valid() || url.IsAboutBlank() || IsUrlNtp(url)) {
    return true;
  }

  return false;
}
