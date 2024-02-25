// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_SELECTION_UTIL_H_
#define IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_SELECTION_UTIL_H_

#include "base/feature_list.h"

// Feature flag to enable Text Classifier entity detection in experience kit
// The addresses entity type is enabled by default. Feature params will control
// enabling the other entity types, and enabling for the one tap mode.
BASE_DECLARE_FEATURE(kEnableExpKitTextClassifier);

#endif  // IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_SELECTION_UTIL_H_
