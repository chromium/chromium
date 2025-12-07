// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_LAST_TAP_LOCATION_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_LAST_TAP_LOCATION_TAB_HELPER_H_

#import <CoreGraphics/CGGeometry.h>

#import "base/scoped_observation.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@class LastTapLocationController;
@class UITapGestureRecognizer;

// A tab helper that tracks the last tap location on the web view using a
// UITapGestureRecognizer.
class LastTapLocationTabHelper
    : public web::WebStateUserData<LastTapLocationTabHelper>,
      public web::WebStateObserver {
 public:
  ~LastTapLocationTabHelper() override;

  // Returns the last tap point in the web view coordinate system.
  CGPoint GetLastTapPoint() const;

 private:
  explicit LastTapLocationTabHelper(web::WebState* web_state);
  friend class web::WebStateUserData<LastTapLocationTabHelper>;

  // web::WebStateObserver implementation.
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;

  // Called when the user taps on the web view.
  void HandleTap(UITapGestureRecognizer* sender);

  base::ScopedObservation<web::WebState, web::WebStateObserver> observation_{
      this};

  // Tap gesture recognizer to track tap events on the web view.
  UITapGestureRecognizer* tap_gesture_recognizer_ = nil;
  // Target of the tap gesture recognizer.
  LastTapLocationController* tap_gesture_target_ = nil;
  // Last tap point in the web view.
  CGPoint last_tap_point_ = CGPointZero;

  base::WeakPtrFactory<LastTapLocationTabHelper> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_LAST_TAP_LOCATION_TAB_HELPER_H_
