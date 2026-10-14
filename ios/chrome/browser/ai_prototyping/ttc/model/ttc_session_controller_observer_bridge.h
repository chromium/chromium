// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_SESSION_CONTROLLER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_SESSION_CONTROLLER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "components/ttc/app/public/error_codes.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller_observer.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_states.h"

class TtcSessionController;

// Objective-C protocol for observing `TtcSessionController` events via
// `TtcSessionControllerObserverBridge`.
@protocol TTCSessionControllerObserver <NSObject>

// Invoked when the session lifecycle state transitions (e.g. from initializing
// to live, or to finished).
- (void)sessionController:(TtcSessionController*)controller
       didChangeLifecycle:(TTCSessionLifecycle)lifecycle;

// Invoked when the user's captured microphone audio level is updated.
// `audioLevel` is normalized in the range [0.0, 1.0].
- (void)sessionController:(TtcSessionController*)controller
      didUpdateAudioLevel:(float)audioLevel;

// Invoked when a fatal or non-fatal session error occurs.
- (void)sessionController:(TtcSessionController*)controller
         didFailWithError:(ttc::ErrorCode)error;

@end

// Adapter to use an `id<TTCSessionControllerObserver>` as a
// `TtcSessionControllerObserver`.
class TtcSessionControllerObserverBridge : public TtcSessionControllerObserver {
 public:
  explicit TtcSessionControllerObserverBridge(
      id<TTCSessionControllerObserver> observer);
  TtcSessionControllerObserverBridge(
      const TtcSessionControllerObserverBridge&) = delete;
  TtcSessionControllerObserverBridge& operator=(
      const TtcSessionControllerObserverBridge&) = delete;
  ~TtcSessionControllerObserverBridge() override;

  // `TtcSessionControllerObserver` implementation:
  void OnSessionLifecycleChanged(TtcSessionController* controller,
                                 TTCSessionLifecycle lifecycle) override;
  void OnUserAudioLevelUpdated(TtcSessionController* controller,
                               float audio_level) override;
  void OnSessionError(TtcSessionController* controller,
                      ttc::ErrorCode error) override;

 private:
  __weak id<TTCSessionControllerObserver> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_SESSION_CONTROLLER_OBSERVER_BRIDGE_H_
