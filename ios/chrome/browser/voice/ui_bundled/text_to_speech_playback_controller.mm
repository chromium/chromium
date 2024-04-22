// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_playback_controller.h"

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_notification_handler.h"
#import "ios/web/public/web_state.h"

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

void TextToSpeechPlaybackController::WebStateListWillChange(
    WebStateList* web_state_list,
    const WebStateListChangeDetach& detach_change,
    const WebStateListStatus& status) {
  if (!detach_change.is_closing()) {
    return;
  }

  if (web_state_.get() == detach_change.detached_web_state()) {
    SetWebState(nullptr);
  }
}

void TextToSpeechPlaybackController::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  if (change.type() == WebStateListChange::Type::kDetach) {
    const WebStateListChangeDetach& detach_change =
        change.As<WebStateListChangeDetach>();
    if (web_state_.get() == detach_change.detached_web_state()) {
      SetWebState(nullptr);
    }
  }

  if (status.active_web_state_change()) {
    SetWebState(status.new_active_web_state);
  }
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
