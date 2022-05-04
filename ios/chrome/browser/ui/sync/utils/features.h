// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SYNC_UTILS_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_SYNC_UTILS_FEATURES_H_

#include "base/feature_list.h"

// Feature flag to enable DisplaySyncErrors refactored code.
extern const base::Feature kDisplaySyncErrorsRefactor;

// Returns true if DisplaySyncErrors integration is enabled.
bool IsDisplaySyncErrorsRefactorEnabled();

#endif  // IOS_CHROME_BROWSER_UI_SYNC_UTILS_FEATURES_H_
