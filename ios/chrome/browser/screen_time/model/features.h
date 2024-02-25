// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCREEN_TIME_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_SCREEN_TIME_MODEL_FEATURES_H_

#include "base/feature_list.h"

// Feature flag to enable ScreenTime integration.
BASE_DECLARE_FEATURE(kScreenTimeIntegration);

// Returns true if ScreenTime integration is enabled.
bool IsScreenTimeIntegrationEnabled();

#endif  // IOS_CHROME_BROWSER_SCREEN_TIME_MODEL_FEATURES_H_
