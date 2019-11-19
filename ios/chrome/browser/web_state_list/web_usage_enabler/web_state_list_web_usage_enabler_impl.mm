// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler_impl.h"

#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebStateListWebUsageEnablerImpl::WebStateListWebUsageEnablerImpl()
    : WebStateListWebUsageEnabler() {}

void WebStateListWebUsageEnablerImpl::SetWebStateList(
    WebStateList* web_state_list) {
  if (web_state_list_ == web_state_list)
    return;
  if (web_state_list_)
    web_state_list_->RemoveObserver(this);
  web_state_list_ = web_state_list;
  if (web_state_list_)
    web_state_list_->AddObserver(this);
  UpdateWebUsageEnabled();
}

bool WebStateListWebUsageEnablerImpl::IsWebUsageEnabled() const {
  return web_usage_enabled_;
}

void WebStateListWebUsageEnablerImpl::SetWebUsageEnabled(
    bool web_usage_enabled) {
  if (web_usage_enabled_ == web_usage_enabled)
    return;
  web_usage_enabled_ = web_usage_enabled;
  UpdateWebUsageEnabled();
}

bool WebStateListWebUsageEnablerImpl::TriggersInitialLoad() const {
  return triggers_initial_load_;
}

void WebStateListWebUsageEnablerImpl::SetTriggersInitialLoad(
    bool triggers_initial_load) {
  triggers_initial_load_ = triggers_initial_load;
}

void WebStateListWebUsageEnablerImpl::UpdateWebUsageEnabled() {
  if (!web_state_list_)
    return;
  for (int index = 0; index < web_state_list_->count(); ++index) {
    web::WebState* web_state = web_state_list_->GetWebStateAt(index);
    web_state->SetWebUsageEnabled(web_usage_enabled_);
  }
}

void WebStateListWebUsageEnablerImpl::UpdateWebUsageForAddedWebState(
    web::WebState* web_state) {
  web_state->SetWebUsageEnabled(web_usage_enabled_);
  if (web_usage_enabled_ && triggers_initial_load_)
    web_state->GetNavigationManager()->LoadIfNecessary();
}

void WebStateListWebUsageEnablerImpl::Shutdown() {
  SetWebStateList(nullptr);
}

void WebStateListWebUsageEnablerImpl::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  UpdateWebUsageForAddedWebState(web_state);
}

void WebStateListWebUsageEnablerImpl::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  UpdateWebUsageForAddedWebState(new_web_state);
}
