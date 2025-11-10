// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_FEATURES_H_
#define IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_FEATURES_H_

#import "base/feature_list.h"

// Feature flag to enable default browser iPad specific instructions.
BASE_DECLARE_FEATURE(kDefaultBrowserPromoIpadInstructions);

// Returns true if the default browser iPad specific instructions are enabled.
bool IsDefaultBrowserPromoIpadInstructions();

#endif  // IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_FEATURES_H_
