// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_restoration_web_state_list_observer.h"

#import "base/check.h"
#import "base/containers/contains.h"
#import "ios/chrome/browser/sessions/model/session_restoration_web_state_observer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

SessionRestorationWebStateListObserver::SessionRestorationWebStateListObserver(
    WebStateList* web_state_list,
    WebStateListDirtyCallback callback)
    : web_state_list_(web_state_list), callback_(std::move(callback)) {
  DCHECK(web_state_list_->empty());
  web_state_list_->AddObserver(this);
}

SessionRestorationWebStateListObserver::
    ~SessionRestorationWebStateListObserver() {
  for (int index = 0; index < web_state_list_->count(); ++index) {
    web::WebState* web_state = web_state_list_->GetWebStateAt(index);
    SessionRestorationWebStateObserver::RemoveFromWebState(web_state);
  }

  web_state_list_->RemoveObserver(this);
}

void SessionRestorationWebStateListObserver::AddExpectedWebState(
    web::WebStateID expected_web_state_id) {
  expected_web_states_.insert(expected_web_state_id);
}

void SessionRestorationWebStateListObserver::ClearDirty() {
  for (web::WebState* web_state : dirty_web_states_) {
    SessionRestorationWebStateObserver::FromWebState(web_state)->clear_dirty();
  }

  is_web_state_list_dirty_ = false;
  dirty_web_states_.clear();
  detached_web_states_.clear();
  inserted_web_states_.clear();
  closed_web_states_.clear();
}

#pragma mark - WebStateListObserver

void SessionRestorationWebStateListObserver::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Nothing specific to do.
      break;

    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();

      DetachWebState(detach_change.detached_web_state(),
                     detach_change.is_closing());
      break;
    }

    case WebStateListChange::Type::kMove:
      // Nothing specific to do.
      break;

    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();

      // The replaced WebState is considered closed.
      DetachWebState(replace_change.replaced_web_state(),
                     /* is_closing */ true);

      DCHECK(replace_change.inserted_web_state()->IsRealized());
      AttachWebState(replace_change.inserted_web_state());
      break;
    }

    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();

      AttachWebState(insert_change.inserted_web_state());
      break;
    }

    case WebStateListChange::Type::kGroupCreate:
      // Nothing specific to do.
      break;

    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Nothing specific to do.
      break;

    case WebStateListChange::Type::kGroupMove:
      // Nothing specific to do.
      break;

    case WebStateListChange::Type::kGroupDelete:
      // Nothing specific to do.
      break;
  }

  if (!web_state_list->IsBatchInProgress()) {
    MarkDirty();
  }
}

void SessionRestorationWebStateListObserver::WillBeginBatchOperation(
    WebStateList* web_state_list) {}

void SessionRestorationWebStateListObserver::BatchOperationEnded(
    WebStateList* web_state_list) {
  // Assume the WebStateList is dirty after any batch operation.
  MarkDirty();
}

void SessionRestorationWebStateListObserver::WebStateListDestroyed(
    WebStateList* web_state_list) {
  NOTREACHED();
}

#pragma mark - Private methods

void SessionRestorationWebStateListObserver::DetachWebState(
    web::WebState* detached_web_state,
    bool is_closing) {
  // Don't try to save the state of the detached WebState. It will either be
  // closed (thus don't need to be saved) or will be inserted into another
  // Browser which will adopt it and take care of saving its state.
  dirty_web_states_.erase(detached_web_state);

  // If the detached WebState is still listed as recently inserted, then it
  // means it will still be considered up-for-adoption by another Browser.
  // In that case, remove the WebState from the list of inserted WebStates,
  // otherwise, add it to the list of detached WebState if unrealized.
  //
  // If the WebState is closed, always add it to the list of closed WebStates
  // (this allow deleting data when a WebState is moved between Browsers and
  // then closed before it the session could be saved).
  const web::WebStateID identifier = detached_web_state->GetUniqueIdentifier();
  if (base::Contains(inserted_web_states_, identifier)) {
    inserted_web_states_.erase(identifier);
  } else if (!is_closing && !detached_web_state->IsRealized()) {
    detached_web_states_.insert(identifier);
  }

  if (is_closing) {
    closed_web_states_.insert(identifier);
  }

  // Stop observing the detached WebState. If it is inserted in another
  // Browser, its state will be observed there.
  SessionRestorationWebStateObserver::RemoveFromWebState(detached_web_state);
}

void SessionRestorationWebStateListObserver::AttachWebState(
    web::WebState* attached_web_state) {
  // Start observing the attached WebState for change of its state.
  SessionRestorationWebStateObserver::CreateForWebState(
      attached_web_state,
      base::BindRepeating(
          &SessionRestorationWebStateListObserver::MarkWebStateDirty,
          base::Unretained(this)));

  // If the newly attached `WebState` can be serialized, then mark it as dirty
  // to force its serialization, otherwise adopt it (this will allow re-using
  // the existing data on disk).
  if (attached_web_state->IsRealized()) {
    MarkWebStateDirty(attached_web_state);
  } else {
    const auto web_state_id = attached_web_state->GetUniqueIdentifier();
    if (base::Contains(expected_web_states_, web_state_id)) {
      expected_web_states_.erase(web_state_id);
    } else if (base::Contains(detached_web_states_, web_state_id)) {
      detached_web_states_.erase(web_state_id);
    } else {
      inserted_web_states_.insert(web_state_id);
    }
  }
}

void SessionRestorationWebStateListObserver::MarkWebStateDirty(
    web::WebState* web_state) {
  // If the WebState cannot be serialized, ignore the event. This may happen
  // when a WebState transition to the realized state but has not completed
  // the restoration of the navigation history. Clear the dirty state of the
  // observer to be notified of the next event.
  if (!web_state->IsRealized()) {
    SessionRestorationWebStateObserver::FromWebState(web_state)->clear_dirty();
    return;
  }

  if (!base::Contains(dirty_web_states_, web_state)) {
    inserted_web_states_.erase(web_state->GetUniqueIdentifier());
    dirty_web_states_.insert(web_state);

    if (!is_web_state_list_dirty_) {
      callback_.Run(web_state_list_.get());
    }
  }
}

void SessionRestorationWebStateListObserver::MarkDirty() {
  if (is_web_state_list_dirty_) {
    return;
  }

  is_web_state_list_dirty_ = true;
  if (dirty_web_states_.empty()) {
    callback_.Run(web_state_list_.get());
  }
}
