// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_state_list_observer.h"

#import "base/check_op.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_content_adjustment_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
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

#pragma mark - WebStateListObserver

void FullscreenWebStateListObserver::WebStateListChanged(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateSelection& selection) {
  switch (change.type()) {
    case WebStateListChange::Type::kSelectionOnly:
      // TODO(crbug.com/1442546): Move the implementation from
      // WebStateActivatedAt() to here. Note that here is reachable only when
      // `reason` == ActiveWebStateChangeReason::Activated.
      break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      WebStateWasRemoved(detach_change.detached_web_state());
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      WebStateWasRemoved(replace_change.replaced_web_state());
      web::WebState* inserted_web_state = replace_change.inserted_web_state();
      if (inserted_web_state == web_state_list->GetActiveWebState()) {
        // Reset the model if the active WebState is replaced.
        model_->ResetForNavigation();
        WebStateWasActivated(inserted_web_state);
      }
      break;
    }
    case WebStateListChange::Type::kInsert: {
      DCHECK_EQ(web_state_list_, web_state_list);
      if (selection.activating) {
        controller_->ExitFullscreen();
      }
      break;
    }
  }
}

void FullscreenWebStateListObserver::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    ActiveWebStateChangeReason reason) {
  WebStateWasActivated(new_web_state);
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
