// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_state_list_observer.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_content_adjustment_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FullscreenWebStateListObserver::FullscreenWebStateListObserver(
    FullscreenController* controller,
    FullscreenModel* model,
    FullscreenMediator* mediator)
    : controller_(controller),
      model_(model),
      web_state_observer_(controller, model, mediator) {
  DCHECK(controller_);
  DCHECK(model_);
}

FullscreenWebStateListObserver::~FullscreenWebStateListObserver() {
  // Disconnect() should be called before destruction.
  DCHECK(!web_state_list_);
}

void FullscreenWebStateListObserver::SetWebStateList(
    WebStateList* web_state_list) {
  if (web_state_list_ == web_state_list)
    return;
  if (web_state_list_)
    web_state_list_->RemoveObserver(this);
  web_state_list_ = web_state_list;
  if (web_state_list_) {
    web_state_list_->AddObserver(this);
    WebStateWasActivated(web_state_list_->GetActiveWebState());
  } else {
    web_state_observer_.SetWebState(nullptr);
  }
}

const WebStateList* FullscreenWebStateListObserver::GetWebStateList() const {
  return web_state_list_;
}

WebStateList* FullscreenWebStateListObserver::GetWebStateList() {
  return web_state_list_;
}

void FullscreenWebStateListObserver::Disconnect() {
  SetWebStateList(nullptr);
}

void FullscreenWebStateListObserver::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  DCHECK_EQ(web_state_list_, web_state_list);
  if (activating)
    controller_->ExitFullscreen();
}

void FullscreenWebStateListObserver::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  WebStateWasRemoved(old_web_state);
  if (new_web_state == web_state_list->GetActiveWebState()) {
    // Reset the model if the active WebState is replaced.
    model_->ResetForNavigation();
    WebStateWasActivated(new_web_state);
  }
}

void FullscreenWebStateListObserver::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    int reason) {
  WebStateWasActivated(new_web_state);
}

void FullscreenWebStateListObserver::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  WebStateWasRemoved(web_state);
}

void FullscreenWebStateListObserver::WillCloseWebStateAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool user_action) {
  WebStateWasRemoved(web_state);
}

void FullscreenWebStateListObserver::WebStateWasActivated(
    web::WebState* web_state) {
  web_state_observer_.SetWebState(web_state);
  if (web_state && !HasWebStateBeenActivated(web_state)) {
    MoveContentBelowHeader(web_state->GetWebViewProxy(), model_);
    activated_web_states_.insert(web_state);
  }
}

void FullscreenWebStateListObserver::WebStateWasRemoved(
    web::WebState* web_state) {
  if (HasWebStateBeenActivated(web_state))
    activated_web_states_.erase(web_state);
}

bool FullscreenWebStateListObserver::HasWebStateBeenActivated(
    web::WebState* web_state) {
  return activated_web_states_.find(web_state) != activated_web_states_.end();
}
