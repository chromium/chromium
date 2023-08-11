// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_UI_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_UI_FEATURES_H_

#import "base/feature_list.h"

// Password manager features that only touch UI. Other features should go in
// components.
namespace password_manager::features {

BASE_DECLARE_FEATURE(kIOSPasswordAuthOnEntry);

// Helper function returning the status of `kIOSPasswordAuthOnEntry`.
bool IsAuthOnEntryEnabled();

BASE_DECLARE_FEATURE(kIOSPasswordAuthOnEntryV2);

// Helper function returning the status of `kIOSPasswordAuthOnEntry2`.
bool IsAuthOnEntryV2Enabled();

}  // namespace password_manager::features

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_UI_FEATURES_H_
