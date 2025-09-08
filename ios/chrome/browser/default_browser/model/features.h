// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_FEATURES_H_

#import "base/feature_list.h"

// Feature to enable different text for the non-modal DB promo.
BASE_DECLARE_FEATURE(kTailoredNonModalDBPromo);

// Returns whether `kTailoredNonModalDBPromo` is enabled.
bool IsTailoredNonModalDBPromoEnabled();

// Feature to enable sharing default browser status with 1P apps.
BASE_DECLARE_FEATURE(kShareDefaultBrowserStatus);

// Returns whether `kShareDefaultBrowserStatus` is enabled.
bool IsShareDefaultBrowserStatusEnabled();

BASE_DECLARE_FEATURE(kPersistentDefaultBrowserPromo);

// Returns whether `kPersistentDefaultBrowserPromo` is enabled.
bool IsPersistentDefaultBrowserPromoEnabled();

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_FEATURES_H_
