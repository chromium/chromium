// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_FEATURES_H_

#import "base/feature_list.h"

// Feature flag to enable non-modal promo migration.
BASE_DECLARE_FEATURE(kNonModalPromoMigration);

// Returns true if the non-modal promo migration is enabled.
bool IsNonModalPromoMigrationEnabled();

// Feature to enable different text for the non-modal DB promo.
BASE_DECLARE_FEATURE(kTailoredNonModalDBPromo);

// Returns whether `kTailoredNonModalDBPromo` is enabled.
bool IsTailoredNonModalDBPromoEnabled();

// Feature to enable sharing default browser status with 1P apps.
BASE_DECLARE_FEATURE(kShareDefaultBrowserStatus);

// Returns whether `kShareDefaultBrowserStatus` is enabled.
bool IsShareDefaultBrowserStatusEnabled();

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_FEATURES_H_
