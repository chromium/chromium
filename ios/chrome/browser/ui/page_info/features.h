// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_FEATURES_H_

#include "base/feature_list.h"

// Feature for the revamp of Page Info in iOS.
BASE_DECLARE_FEATURE(kRevampPageInfoIos);

// Whether the Revamp Page Info feature is enabled.
bool IsRevampPageInfoIosEnabled();

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_FEATURES_H_
