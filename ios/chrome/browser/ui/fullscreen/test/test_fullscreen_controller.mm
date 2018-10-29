// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_controller.h"

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_observer.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TestFullscreenController::TestFullscreenController(FullscreenModel* model)
    : FullscreenController(),
      model_(model),
      broadcaster_([[ChromeBroadcaster alloc] init]) {}

TestFullscreenController::~TestFullscreenController() = default;

ChromeBroadcaster* TestFullscreenController::broadcaster() {
  return broadcaster_;
}

void TestFullscreenController::SetWebStateList(WebStateList* web_state_list) {}

const WebStateList* TestFullscreenController::GetWebStateList() const {
  return nullptr;
}

WebStateList* TestFullscreenController::GetWebStateList() {
  return nullptr;
}

void TestFullscreenController::AddObserver(
    FullscreenControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void TestFullscreenController::RemoveObserver(
    FullscreenControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool TestFullscreenController::IsEnabled() const {
  return model_ && model_->enabled();
}

void TestFullscreenController::IncrementDisabledCounter() {
  if (model_)
    model_->IncrementDisabledCounter();
}

void TestFullscreenController::DecrementDisabledCounter() {
  if (model_)
    model_->DecrementDisabledCounter();
}

CGFloat TestFullscreenController::GetProgress() const {
  return model_ ? model_->progress() : 0.0;
}

void TestFullscreenController::Shutdown() {
  for (auto& observer : observers_) {
    observer.FullscreenControllerWillShutDown(this);
  }
}

void TestFullscreenController::EnterFullscreen() {}

void TestFullscreenController::ExitFullscreen() {
  if (model_)
    model_->ResetForNavigation();
}
