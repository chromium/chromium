// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_state_list_observer.h"

#import "base/check_op.h"
#import "base/containers/contains.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_content_adjustment_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/web/public/web_state.h"

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

void FullscreenWebStateListObserver::WebStateListWillChange(
    WebStateList* web_state_list,
    const WebStateListChangeDetach& detach_change,
    const WebStateListStatus& status) {
  DCHECK_EQ(web_state_list_, web_state_list);
  if (!detach_change.is_closing()) {
    return;
  }

  WebStateWasRemoved(detach_change.detached_web_state());
}

void FullscreenWebStateListObserver::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  DCHECK_EQ(web_state_list_, web_state_list);
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // The activation is handled after this switch statement.
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
      if (inserted_web_state == web_state_list_->GetActiveWebState()) {
        // Reset the model if the active WebState is replaced.
        model_->ResetForNavigation();
        WebStateWasActivated(inserted_web_state);
      }
      break;
    }
    case WebStateListChange::Type::kInsert: {
      if (status.active_web_state_change()) {
        // Exit the fullscreen. Disable the animation if a session restoration
        // is in progress (see https://crbug.com/1485930 for details on the UI
        // glitch that animation can cause).
        if (web_state_list_->IsBatchInProgress()) {
          controller_->ExitFullscreenWithoutAnimation();
        } else {
          controller_->ExitFullscreen();
        }
      }
      break;
    }
    case WebStateListChange::Type::kGroupCreate:
      // Do nothing when a group is created.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Do nothing when a tab group's visual data are updated.
      break;
    case WebStateListChange::Type::kGroupMove:
      // Do nothing when a tab group is moved.
      break;
    case WebStateListChange::Type::kGroupDelete:
      // Do nothing when a group is deleted.
      break;
  }

  if (status.active_web_state_change()) {
    WebStateWasActivated(status.new_active_web_state);
  }
}

void FullscreenWebStateListObserver::WebStateWasActivated(
    web::WebState* web_state) {
  web_state_observer_.SetWebState(web_state);
  if (!web_state) {
    return;
  }
  if (!web_state->IsRealized() || !web_state->GetWebViewProxy()) {
    // TODO(crbug.com/40279169): This should not be reached. Investigate
    // when/why an active WebState doesn't have WebViewProxy.
    return;
  }
  if (!HasWebStateBeenActivated(web_state)) {
    MoveContentBelowHeader(web_state->GetWebViewProxy(), model_);
    activated_web_states_.insert(web_state);
  }
}

void FullscreenWebStateListObserver::WebStateWasRemoved(
    web::WebState* web_state) {
  if (HasWebStateBeenActivated(web_state)) {
    activated_web_states_.erase(web_state);
  }
}

bool FullscreenWebStateListObserver::HasWebStateBeenActivated(
    web::WebState* web_state) {
  return base::Contains(activated_web_states_, web_state);
}
