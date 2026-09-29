// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_SESSION_CONTROLLER_OBSERVER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_SESSION_CONTROLLER_OBSERVER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_states.h"

@class TTCSessionController;

// Observer protocol for events and state transitions occurring within an
// active TTCSessionController.
@protocol TTCSessionControllerObserver <NSObject>

@optional

// Invoked when the session lifecycle state transitions (e.g. from initializing
// to live, or to finished).
- (void)sessionController:(TTCSessionController*)controller
       didChangeLifecycle:(TTCSessionLifecycle)lifecycle;

// Invoked when the user's captured microphone audio level is updated.
// `audioLevel` is normalized in the range [0.0, 1.0].
- (void)sessionController:(TTCSessionController*)controller
      didUpdateAudioLevel:(float)audioLevel;

// Invoked when a fatal or non-fatal session error occurs.
- (void)sessionController:(TTCSessionController*)controller
         didFailWithError:(NSError*)error;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_SESSION_CONTROLLER_OBSERVER_H_
