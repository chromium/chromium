// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BASE_FEATURE(kCalendarKillSwitch,
             "DownloadKillSwitchCalendar",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kARKillSwitch,
             "DownloadKillSwitchAR",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVCardKillSwitch,
             "DownloadKillSwitchVcard",
             base::FEATURE_DISABLED_BY_DEFAULT);
