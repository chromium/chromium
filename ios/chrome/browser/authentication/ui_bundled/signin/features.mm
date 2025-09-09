// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/features.h"

BASE_FEATURE(kFullscreenSigninPromoManagerMigration,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsFullscreenSigninPromoManagerMigrationEnabled() {
  return base::FeatureList::IsEnabled(kFullscreenSigninPromoManagerMigration);
}
