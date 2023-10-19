// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_FEATURES_H_

#include "base/feature_list.h"

// Feature for the Privacy Guide.
BASE_DECLARE_FEATURE(kPrivacyGuideIos);

// Whether the Privacy Guide feature is enabled.
bool IsPrivacyGuideIosEnabled();

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_FEATURES_H_
