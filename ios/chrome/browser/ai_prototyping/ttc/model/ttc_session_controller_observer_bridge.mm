// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller_observer_bridge.h"

TtcSessionControllerObserverBridge::TtcSessionControllerObserverBridge(
    id<TTCSessionControllerObserver> observer)
    : observer_(observer) {}

TtcSessionControllerObserverBridge::~TtcSessionControllerObserverBridge() =
    default;

void TtcSessionControllerObserverBridge::OnSessionLifecycleChanged(
    TtcSessionController* controller,
    TTCSessionLifecycle lifecycle) {
  [observer_ sessionController:controller didChangeLifecycle:lifecycle];
}

void TtcSessionControllerObserverBridge::OnUserAudioLevelUpdated(
    TtcSessionController* controller,
    float audio_level) {
  [observer_ sessionController:controller didUpdateAudioLevel:audio_level];
}

void TtcSessionControllerObserverBridge::OnSessionError(
    TtcSessionController* controller,
    ttc::ErrorCode error) {
  [observer_ sessionController:controller didFailWithError:error];
}
