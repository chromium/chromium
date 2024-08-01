// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_CONFIRMATION_CONSUMER_H_
#define IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_CONFIRMATION_CONSUMER_H_

#import "base/timer/timer.h"

@protocol IdleTimeoutConfirmationConsumer <NSObject>

// Sets the timer countdown to update the under title view until it is
// dismissed.
- (void)setCountdown:(base::TimeDelta)countdown;

@end
#endif  // IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_CONFIRMATION_CONSUMER_H_
