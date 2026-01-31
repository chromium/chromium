// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_input_accessory/coordinator/keyboard_accessory_optional_update_scheduler.h"

#import "base/functional/bind.h"

KeyboardAccessoryOptionalUpdateScheduler::
    KeyboardAccessoryOptionalUpdateScheduler(
        id<KeyboardAccessoryOptionalUpdateSchedulerDelegate> delegate)
    : delegate_(delegate) {}

KeyboardAccessoryOptionalUpdateScheduler::
    ~KeyboardAccessoryOptionalUpdateScheduler() = default;

void KeyboardAccessoryOptionalUpdateScheduler::ScheduleOptionalUpdate() {
  if (_cooldownTimer &&
      _cooldownTimer->Elapsed() < kOptionalUpdateCooldownPeriod) {
    return;
  }
  _optionalUpdateTimer.Start(FROM_HERE, kOptionalUpdateDelay, base::BindOnce(^{
                               [delegate_ updateSuggestionsIfNeeded];
                             }));
}

void KeyboardAccessoryOptionalUpdateScheduler::RestartCooldownTimer() {
  _cooldownTimer = base::ElapsedTimer();
}

void KeyboardAccessoryOptionalUpdateScheduler::CancelOptionalUpdate() {
  _optionalUpdateTimer.Stop();
}
