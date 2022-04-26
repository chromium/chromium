// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_FEATURES_H_
#define IOS_CHROME_BROWSER_NTP_FEATURES_H_

#include "base/feature_list.h"

// Feature flag to enable NTP UI pending loader blocker.
extern const base::Feature kBlockNewTabPagePendingLoad;

// Feature flag to enable the Following feed in the NTP.
// Use IsWebChannelsEnabled() instead of this constant directly.
extern const base::Feature kEnableWebChannels;

// Whether the Following Feed is enabled on NTP.
bool IsWebChannelsEnabled();

#endif  // IOS_CHROME_BROWSER_NTP_FEATURES_H_
