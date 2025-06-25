// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_FEATURES_H_

#include "base/feature_list.h"

// Feature controlling when to create TabHelpers.
BASE_DECLARE_FEATURE(kCreateTabHelperOnlyForRealizedWebStates);

// Returns whether the TabHelpers should only be created for realized WebStates.
bool CreateTabHelperOnlyForRealizedWebStates();

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_FEATURES_H_
