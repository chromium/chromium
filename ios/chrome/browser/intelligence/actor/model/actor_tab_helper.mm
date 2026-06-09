// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"

#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper_observer.h"

ActorTabHelper::ActorTabHelper(web::WebState* web_state)
    : web_state_(web_state) {}

ActorTabHelper::~ActorTabHelper() = default;

void ActorTabHelper::SetActuating(bool actuating) {
  if (is_actuating_ == actuating) {
    return;
  }
  is_actuating_ = actuating;
  for (ActorTabHelperObserver& observer : observers_) {
    observer.OnActuationStateChanged(this, web_state_, is_actuating_);
  }
}

bool ActorTabHelper::IsActuating() const {
  return is_actuating_;
}

void ActorTabHelper::AddObserver(ActorTabHelperObserver* observer) {
  observers_.AddObserver(observer);
}

void ActorTabHelper::RemoveObserver(ActorTabHelperObserver* observer) {
  observers_.RemoveObserver(observer);
}
