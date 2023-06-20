// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/voice/text_to_speech_playback_controller.h"

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/voice/text_to_speech_notification_handler.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TextToSpeechPlaybackController::TextToSpeechPlaybackController()
    : notification_helper_([[TextToSpeechNotificationHandler alloc] init]) {}

void TextToSpeechPlaybackController::SetWebStateList(
    WebStateList* web_state_list) {
  if (web_state_list_ == web_state_list)
    return;

  if (web_state_list_)
    web_state_list_->RemoveObserver(this);
  web_state_list_ = web_state_list;
  if (web_state_list_)
    web_state_list_->AddObserver(this);

  SetWebState(web_state_list_ ? web_state_list_->GetActiveWebState() : nullptr);
}

bool TextToSpeechPlaybackController::IsEnabled() const {
  return notification_helper_.enabled;
}

void TextToSpeechPlaybackController::SetEnabled(bool enabled) {
  notification_helper_.enabled = enabled;
}

#pragma mark Private

void TextToSpeechPlaybackController::SetWebState(web::WebState* web_state) {
  if (web_state_ == web_state)
    return;

  if (web_state_) {
    web_state_->RemoveObserver(this);
    [notification_helper_ cancelPlayback];
  }
  web_state_ = web_state;
  if (web_state_)
    web_state_->AddObserver(this);
}

#pragma mark KeyedService

void TextToSpeechPlaybackController::Shutdown() {
  [notification_helper_ cancelPlayback];
  notification_helper_ = nil;
  SetWebStateList(nullptr);
}

#pragma mark WebStateListObserver

void TextToSpeechPlaybackController::WebStateListChanged(
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
      if (web_state_ == detach_change.detached_web_state()) {
        SetWebState(nullptr);
      }
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      web::WebState* inserted_web_state = replace_change.inserted_web_state();
      if (inserted_web_state == web_state_list->GetActiveWebState()) {
        SetWebState(inserted_web_state);
      }
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      if (selection.activating) {
        SetWebState(insert_change.inserted_web_state());
      }
      break;
    }
  }
}

void TextToSpeechPlaybackController::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    ActiveWebStateChangeReason reason) {
  SetWebState(new_web_state);
}

void TextToSpeechPlaybackController::WillCloseWebStateAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool user_action) {
  if (web_state_ == web_state)
    SetWebState(nullptr);
}

#pragma mark WebStateObserver

void TextToSpeechPlaybackController::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  [notification_helper_ cancelPlayback];
}

void TextToSpeechPlaybackController::WebStateDestroyed(
    web::WebState* web_state) {
  [notification_helper_ cancelPlayback];
  SetWebState(nullptr);
}
