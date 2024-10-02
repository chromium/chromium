// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_FEATURES_H_
#define IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_FEATURES_H_

#include "base/feature_list.h"

// Feature for the Incognito Soft Lock.
BASE_DECLARE_FEATURE(kIOSSoftLock);

// Whether the Soft Lock feature is enabled.
bool IsIOSSoftLockEnabled();

#endif  // IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_FEATURES_H_
