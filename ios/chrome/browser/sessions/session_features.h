// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_FEATURES_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_FEATURES_H_

#include "base/feature_list.h"

namespace sessions {

// If enabled, save each tab content to a separate file.
BASE_DECLARE_FEATURE(kSaveSessionTabsToSeparateFiles);

// If enabled, save each tab content to a separate file.
bool ShouldSaveSessionTabsToSeparateFiles();

}  // namespace sessions

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_FEATURES_H_
