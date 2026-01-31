// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_DEBUGGER_UI_AIM_DEBUGGER_CONSUMER_H_
#define IOS_CHROME_BROWSER_AIM_DEBUGGER_UI_AIM_DEBUGGER_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "base/containers/enum_set.h"

// AIM eligibility status conditions.
enum class AimEligibilityCheck {
  kIsEligible,
  kIsEligibleByPolicy,
  kIsEligibleByDse,
  kIsEligibleByServer,
  kIsServerEligibilityEnabled,
  kMinValue = kIsEligible,
  kMaxValue = kIsServerEligibilityEnabled,
};

// Bitset holding eligibility status.
using AimEligibilitySet = base::EnumSet<AimEligibilityCheck,
                                        AimEligibilityCheck::kMinValue,
                                        AimEligibilityCheck::kMaxValue>;

@protocol AimDebuggerConsumer <NSObject>

// Updates the eligibility status.
- (void)setEligibilityStatus:(AimEligibilitySet)status;

// Updates the "Server Response" text view.
- (void)setServerResponse:(NSString*)base64Response;

// Updates the "Source" label (e.g., "Server", "Prefs").
- (void)setResponseSource:(NSString*)source;

@end

#endif  // IOS_CHROME_BROWSER_AIM_DEBUGGER_UI_AIM_DEBUGGER_CONSUMER_H_
