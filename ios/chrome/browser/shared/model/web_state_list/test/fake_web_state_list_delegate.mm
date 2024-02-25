// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"

#import "ios/web/public/web_state.h"

FakeWebStateListDelegate::FakeWebStateListDelegate()
    : FakeWebStateListDelegate(/* force_realization_on_activation */ false) {}

FakeWebStateListDelegate::FakeWebStateListDelegate(
    bool force_realization_on_activation)
    : force_realization_on_activation_(force_realization_on_activation) {}

FakeWebStateListDelegate::~FakeWebStateListDelegate() = default;

void FakeWebStateListDelegate::WillAddWebState(web::WebState* web_state) {}

void FakeWebStateListDelegate::WillActivateWebState(web::WebState* web_state) {
  if (force_realization_on_activation_) {
    web::IgnoreOverRealizationCheck();
    web_state->ForceRealized();
  }
}
