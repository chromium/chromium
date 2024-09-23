// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_FEATURES_H_

#import "base/feature_list.h"

// Feature for the implementation of Quick Delete in iOS.
BASE_DECLARE_FEATURE(kIOSQuickDelete);

// Whether the iOS Quick Delete feature is enabled.
bool IsIosQuickDeleteEnabled();

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_FEATURES_H_
