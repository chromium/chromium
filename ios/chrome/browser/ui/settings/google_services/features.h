// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_FEATURES_H_

#include "base/feature_list.h"

// Feature for the Linked Services Setting.
BASE_DECLARE_FEATURE(kLinkedServicesSettingIos);

// Whether the Linked Services Setting feature is enabled.
bool IsLinkedServicesSettingIosEnabled();

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_FEATURES_H_
