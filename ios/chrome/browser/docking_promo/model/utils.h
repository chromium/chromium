// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOCKING_PROMO_MODEL_UTILS_H_
#define IOS_CHROME_BROWSER_DOCKING_PROMO_MODEL_UTILS_H_

#import "base/time/time.h"

// For testing only.
// Returns YES if the Docking Promo is forced for display via Chrome
// Experimental Settings.
BOOL IsDockingPromoForcedForDisplay();

// Returns whether the user eligibility criteria to show the Docking Promo have
// been met, namely:
//
// 1. Chrome is likely not the default browser,
//
// AND
//
// 2. For users no older than 2 days, whether they're active on their first day,
// but not their second day, (and/or) for users no older than 14 days, whether
// they've been inactive for 3 consecutive (or more) days.
BOOL CanShowDockingPromo(base::TimeDelta time_since_last_foreground);

#endif  // IOS_CHROME_BROWSER_DOCKING_PROMO_MODEL_UTILS_H_
