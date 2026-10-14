// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_SESSION_CONTROLLER_OBSERVER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_SESSION_CONTROLLER_OBSERVER_H_

#import "base/observer_list_types.h"
#import "components/ttc/app/public/error_codes.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_states.h"

class TtcSessionController;

// Observer interface for events and state transitions occurring within an
// active `TtcSessionController`.
class TtcSessionControllerObserver : public base::CheckedObserver {
 public:
  // Invoked when the session lifecycle state transitions (e.g. from
  // initializing to live, or to finished).
  virtual void OnSessionLifecycleChanged(TtcSessionController* controller,
                                         TTCSessionLifecycle lifecycle) {}

  // Invoked when the user's captured microphone audio level is updated.
  // `audio_level` is normalized in the range [0.0, 1.0].
  virtual void OnUserAudioLevelUpdated(TtcSessionController* controller,
                                       float audio_level) {}

  // Invoked when a fatal or non-fatal session error occurs.
  virtual void OnSessionError(TtcSessionController* controller,
                              ttc::ErrorCode error) {}
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_SESSION_CONTROLLER_OBSERVER_H_
