// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UPGRADE_UTILS_FEATURES_H_
#define IOS_CHROME_BROWSER_UPGRADE_UTILS_FEATURES_H_

#include "base/feature_list.h"

// Feature flag to enable UpgradeCenter refactored code.
extern const base::Feature kUpgradeCenterRefactor;

// Returns true if UpgradeCenter refactored code is enabled.
bool IsUpgradeCenterRefactorEnabled();

#endif  // IOS_CHROME_BROWSER_UPGRADE_UTILS_FEATURES_H_
