// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_FEATURES_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_FEATURES_H_

#import "base/feature_list.h"

// Feature flag to enable the sign-in promo manager migration.
BASE_DECLARE_FEATURE(kFullscreenSigninPromoManagerMigration);

// Returns true if the full screen sign-in promo manager migration is enabled.
bool IsFullscreenSigninPromoManagerMigrationEnabled();

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_FEATURES_H_
