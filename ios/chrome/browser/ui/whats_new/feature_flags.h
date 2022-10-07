// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_FEATURE_FLAGS_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_FEATURE_FLAGS_H_

#include "base/feature_list.h"

// Feature to enable What's New feature.
BASE_DECLARE_FEATURE(kWhatsNewIOS);

extern const char kWhatsNewModuleBasedLayoutParam[];

bool IsWhatsNewModuleBasedLayout();

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_FEATURE_FLAGS_H_
