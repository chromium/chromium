// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_TEST_TEST_FULLSCREEN_MODEL_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_TEST_TEST_FULLSCREEN_MODEL_OBSERVER_H_

#import "ios/chrome/browser/ui/fullscreen/fullscreen_model_observer.h"

// Test observer for FullscreenModel.
class TestFullscreenModelObserver : public FullscreenModelObserver {
 public:
  TestFullscreenModelObserver();

  // The values provided to the observer.
  CGFloat progress() { return progress_; }
  bool enabled() { return enabled_; }
  bool scroll_end_received() { return scroll_end_received_; }
  bool reset_called() { return reset_called_; }

 private:
  // FullscreenModelObserver:
  void FullscreenModelProgressUpdated(FullscreenModel* model) override;
  void FullscreenModelEnabledStateChanged(FullscreenModel* model) override;
  void FullscreenModelScrollEventEnded(FullscreenModel* model) override;
  void FullscreenModelWasReset(FullscreenModel* model) override;

  CGFloat progress_ = 0.0;
  bool enabled_ = true;
  bool scroll_end_received_ = false;
  bool reset_called_ = false;
};

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_TEST_TEST_FULLSCREEN_MODEL_OBSERVER_H_
