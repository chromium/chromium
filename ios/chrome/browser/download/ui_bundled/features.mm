// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui_bundled/features.h"

BASE_FEATURE(kCalendarKillSwitch,
             "DownloadKillSwitchCalendar",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVCardKillSwitch,
             "DownloadKillSwitchVcard",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kARKillSwitch,
             "DownloadKillSwitchAR",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPassKitKillSwitch,
             "DownloadKillSwitchPassKit",
             base::FEATURE_DISABLED_BY_DEFAULT);
