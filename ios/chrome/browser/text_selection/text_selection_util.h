// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TEXT_SELECTION_TEXT_SELECTION_UTIL_H_
#define IOS_CHROME_BROWSER_TEXT_SELECTION_TEXT_SELECTION_UTIL_H_

#include "base/feature_list.h"

// Feature flag to enable Text Classifier entity detection in experience kit
// calendar.
BASE_DECLARE_FEATURE(kEnableExpKitCalendarTextClassifier);

// Feature flag to enable Text Classifier entity detection in experience kit
// The feature params will control which entity types are enabled for
// detection.
BASE_DECLARE_FEATURE(kEnableExpKitTextClassifier);

#endif  // IOS_CHROME_BROWSER_TEXT_SELECTION_TEXT_SELECTION_UTIL_H_
