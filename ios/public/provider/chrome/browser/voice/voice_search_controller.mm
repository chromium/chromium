// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/voice/voice_search_controller.h"

#import <UIKit/UIKit.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

VoiceSearchController::VoiceSearchController() {}

VoiceSearchController::~VoiceSearchController() {}

void VoiceSearchController::SetDispatcher(id<LoadQueryCommands> dispatcher) {}

void VoiceSearchController::PrepareToAppear() {}

void VoiceSearchController::StartRecognition(
    UIViewController* presenting_view_controller,
    web::WebState* current_web_state) {}

bool VoiceSearchController::IsTextToSpeechEnabled() {
  return false;
}

bool VoiceSearchController::IsTextToSpeechSupported() {
  return false;
}

bool VoiceSearchController::IsVisible() {
  return false;
}

bool VoiceSearchController::IsPlayingAudio() {
  return false;
}

void VoiceSearchController::DismissMicPermissionsHelp() {}
