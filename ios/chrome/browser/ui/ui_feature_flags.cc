// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/ui_feature_flags.h"

const base::Feature kFirstResponderKeyWindow{"FirstResponderKeyWindow",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// TODO(crbug.com/893314) : Remove this flag.
const base::Feature kClosingLastIncognitoTab{"ClosingLastIncognitoTab",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kBrowserContainerContainsNTP{
    "BrowserContainerContainsNTP", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCopyImage{"CopyImage", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kOmniboxPopupShortcutIconsInZeroState{
    "OmniboxPopupShortcutIconsInZeroState", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kWKWebViewSnapshots{"WKWebViewSnapshots",
                                        base::FEATURE_ENABLED_BY_DEFAULT};
