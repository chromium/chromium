// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_model_observer.h"

#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"

TestFullscreenModelObserver::TestFullscreenModelObserver() = default;

void TestFullscreenModelObserver::FullscreenModelProgressUpdated(
    FullscreenModel* model) {
  progress_ = model->progress();
}

void TestFullscreenModelObserver::FullscreenModelEnabledStateChanged(
    FullscreenModel* model) {
  enabled_ = model->enabled();
}

void TestFullscreenModelObserver::FullscreenModelScrollEventEnded(
    FullscreenModel* model) {
  scroll_end_received_ = true;
}

void TestFullscreenModelObserver::FullscreenModelWasReset(
    FullscreenModel* model) {
  reset_called_ = true;
  progress_ = model->progress();
}
