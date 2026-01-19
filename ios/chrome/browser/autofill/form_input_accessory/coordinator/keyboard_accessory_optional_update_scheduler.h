// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_COORDINATOR_KEYBOARD_ACCESSORY_OPTIONAL_UPDATE_SCHEDULER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_COORDINATOR_KEYBOARD_ACCESSORY_OPTIONAL_UPDATE_SCHEDULER_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "base/time/time.h"
#import "base/timer/elapsed_timer.h"
#import "base/timer/timer.h"

// The cooldown period before accepting optional updates after a
// -retrieveSuggestionsForForm call.
inline constexpr base::TimeDelta kOptionalUpdateCooldownPeriod =
    base::Milliseconds(2000);

// The period from when an optional update is requested to when it is carried
// out. Any new optional update request will cancel the pending optional update,
// and restart the delay.
inline constexpr base::TimeDelta kOptionalUpdateDelay = base::Milliseconds(500);

// Delegate for the optional update scheduler.
@protocol KeyboardAccessoryOptionalUpdateSchedulerDelegate <NSObject>
// Updates suggestions if needed.
- (void)updateSuggestionsIfNeeded;
@end

// Scheduler for optional updates. It accepts an optional update and carries it
// out after a delay. It only accepts an optional update when it is not in a
// cooldown period. If it accepts a new optional update, the existing scheduled
// update will be cancelled.
class KeyboardAccessoryOptionalUpdateScheduler {
 public:
  explicit KeyboardAccessoryOptionalUpdateScheduler(
      id<KeyboardAccessoryOptionalUpdateSchedulerDelegate> delegate);
  ~KeyboardAccessoryOptionalUpdateScheduler();

  // Schedules an optional update if allowed, meaning the cooldown period has
  // expired. If an optional update is already scheduled, this function
  // reschedules the optional update.
  void ScheduleOptionalUpdate();

  // Restarts the cooldown period for optional updates.
  void RestartCooldownTimer();

  // Cancels any pending optional update.
  void CancelOptionalUpdate();

 private:
  // The timer to track the cooldown period for optional updates.
  std::optional<base::ElapsedTimer> _cooldownTimer;

  // The oneshot timer for scheduling an optional update.
  base::OneShotTimer _optionalUpdateTimer;

  // The delegate for the optional update scheduler to trigger updates.
  __weak id<KeyboardAccessoryOptionalUpdateSchedulerDelegate> delegate_;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_COORDINATOR_KEYBOARD_ACCESSORY_OPTIONAL_UPDATE_SCHEDULER_H_
