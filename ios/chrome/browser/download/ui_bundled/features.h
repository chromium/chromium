// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_FEATURES_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_FEATURES_H_

#include "base/feature_list.h"

// Kill switch for Calendar support.
BASE_DECLARE_FEATURE(kCalendarKillSwitch);

// Kill switch for Vcard support.
BASE_DECLARE_FEATURE(kVCardKillSwitch);

// Kill switch for AR support.
BASE_DECLARE_FEATURE(kARKillSwitch);

// Kill switch for the PassKit support.
BASE_DECLARE_FEATURE(kPassKitKillSwitch);

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_FEATURES_H_
