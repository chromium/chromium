// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_playback_controller.h"

#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_notification_handler.h"

TextToSpeechPlaybackController::TextToSpeechPlaybackController()
    : notification_helper_([[TextToSpeechNotificationHandler alloc] init]) {}

TextToSpeechPlaybackController::~TextToSpeechPlaybackController() = default;

void TextToSpeechPlaybackController::SetWebStateList(
    WebStateList* web_state_list) {
  if (web_state_list &&
      web_state_list_observation_.IsObservingSource(web_state_list)) {
    return;
  }

  web_state_list_observation_.Reset();
  if (web_state_list) {
    web_state_list_observation_.Observe(web_state_list);
    SetWebState(web_state_list->GetActiveWebState());
  } else {
    SetWebState(nullptr);
  }
}

bool TextToSpeechPlaybackController::IsEnabled() const {
  return notification_helper_.enabled;
}

void TextToSpeechPlaybackController::SetEnabled(bool enabled) {
  notification_helper_.enabled = enabled;
}

#pragma mark Private

void TextToSpeechPlaybackController::SetWebState(web::WebState* web_state) {
  if (web_state && active_web_state_observation_.IsObservingSource(web_state)) {
    return;
  }

  if (active_web_state_observation_.IsObserving()) {
    active_web_state_observation_.Reset();
    [notification_helper_ cancelPlayback];
  }

  if (web_state) {
    active_web_state_observation_.Observe(web_state);
  }
}

#pragma mark KeyedService

void TextToSpeechPlaybackController::Shutdown() {
  SetWebStateList(nullptr);
  notification_helper_ = nil;
}

#pragma mark WebStateListObserver

void TextToSpeechPlaybackController::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
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
  SetWebState(nullptr);
}
