// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_restoration_web_state_list_observer.h"

#import "base/check.h"
#import "base/containers/contains.h"
#import "ios/chrome/browser/sessions/session_restoration_web_state_observer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

namespace {

// Returns whether `web_state` can be serialized or not.
bool CanSerializeWebState(const web::WebState* web_state) {
  return web_state->IsRealized() &&
         !web_state->GetNavigationManager()->IsRestoreSessionInProgress();
}

}  // namespace

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

void SessionRestorationWebStateListObserver::ClearDirty() {
  for (web::WebState* web_state : dirty_web_states_) {
    SessionRestorationWebStateObserver::FromWebState(web_state)->clear_dirty();
  }

  is_web_state_list_dirty_ = false;
  dirty_web_states_.clear();
  detached_web_states_.clear();
  inserted_web_states_.clear();
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
  NOTREACHED_NORETURN();
}

#pragma mark - Private methods

void SessionRestorationWebStateListObserver::DetachWebState(
    web::WebState* detached_web_state,
    bool is_closing) {
  // Don't try to save the state of the detached WebState. It will either be
  // closed (thus don't need to be saved) or will be inserted into another
  // Browser which will adopt it and take care of saving its state.
  dirty_web_states_.erase(detached_web_state);

  // If the WebState has been inserted and marked for adoption, but detached
  // before it could be adopted, then it should still be listed as detached
  // from the original WebStateList. In that case, do not mark it orphaned
  // here (the state won't be accessible from the WebStateList).
  //
  // Otherwise, list it as orphaned unless it is closed and or serializable
  // (as the WebStateList where it is inserted can just serialize it to get
  // the state).
  const web::WebStateID identifier = detached_web_state->GetUniqueIdentifier();
  if (base::Contains(inserted_web_states_, identifier)) {
    inserted_web_states_.erase(identifier);
  } else if (!is_closing && !CanSerializeWebState(detached_web_state)) {
    detached_web_states_.insert(identifier);
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

  // If the newly attached `WebState` can be serialized, the mark it as dirty
  // to force its serialization, otherwise adtop it (this will allow re-using
  // the existing data on disk).
  if (CanSerializeWebState(attached_web_state)) {
    MarkWebStateDirty(attached_web_state);
  } else {
    inserted_web_states_.insert(attached_web_state->GetUniqueIdentifier());
  }
}

void SessionRestorationWebStateListObserver::MarkWebStateDirty(
    web::WebState* web_state) {
  // If the WebState cannot be serialized, ignore the event. This may happen
  // when a WebState transition to the realized state but has not completed
  // the restoration of the navigation history. Clear the dirty state of the
  // observer to be notified of the next event.
  if (!CanSerializeWebState(web_state)) {
    SessionRestorationWebStateObserver::FromWebState(web_state)->clear_dirty();
    return;
  }

  if (!base::Contains(dirty_web_states_, web_state)) {
    inserted_web_states_.erase(web_state->GetUniqueIdentifier());
    dirty_web_states_.insert(web_state);

    if (!is_web_state_list_dirty_) {
      callback_.Run(web_state_list_);
    }
  }
}

void SessionRestorationWebStateListObserver::MarkDirty() {
  if (is_web_state_list_dirty_) {
    return;
  }

  is_web_state_list_dirty_ = true;
  if (dirty_web_states_.empty()) {
    callback_.Run(web_state_list_);
  }
}
