// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/cobrowse_tab_helper.h"

#import "base/functional/bind.h"
#import "base/task/sequenced_task_runner.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
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

  if (ShouldHideAssistantForURL(web_state->GetLastCommittedURL())) {
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

void CobrowseTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!delegate_ || !scene_handler_) {
    return;
  }

  if (ShouldHideAssistantForURL(navigation_context->GetUrl())) {
    [scene_handler_ hideAssistant];
    delegate_->SetSessionActive(false);
    return;
  }

  if (delegate_->CanShowAssistantForWebState(web_state)) {
    delegate_->SetSessionActive(true);
    delegate_->ConfigureAssistantContextForWebState(web_state);
    [scene_handler_ showAssistant];
  }
}

void CobrowseTabHelper::WebStateDestroyed(web::WebState* web_state) {
  observation_.Reset();
}

#pragma mark - Private helpers

bool CobrowseTabHelper::ShouldHideAssistantForURL(const GURL& url) {
  if (IsUrlNtp(url)) {
    return true;
  }

  // Do not show the cobrowse AIM assistant sheet if navigating to a search URL.
  const TemplateURL* default_search_provider =
      template_url_service_ ? template_url_service_->GetDefaultSearchProvider()
                            : nullptr;
  if (default_search_provider &&
      default_search_provider->IsSearchURL(
          url, template_url_service_->search_terms_data())) {
    return true;
  }

  return false;
}

void CobrowseTabHelper::ShowAssistant() {
  [scene_handler_ showAssistant];
}
