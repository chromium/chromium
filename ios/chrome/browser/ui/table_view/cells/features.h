// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_FEATURES_H_

#include "base/feature_list.h"

// Feature flag enabling the wrapping of table view cell title.
BASE_DECLARE_FEATURE(kTruncateTableViewCellTitle);

// Returns true if Truncate Table View Cell Title feature is enabled.
bool IsTruncateTableViewCellTitleEnabled();

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_FEATURES_H_
