// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_SELECTION_UTIL_H_
#define IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_SELECTION_UTIL_H_

#include "base/feature_list.h"

// Feature parameters for long-press and one-tap for
// `kEnableExpKitTextClassifierAddress` feature.
extern const char kTextClassifierAddressParameterName[];

// Feature parameters for long-press and one-tap for
// `kEnableExpKitTextClassifierPhoneNumber` feature.
extern const char kTextClassifierPhoneNumberParameterName[];

// Feature parameters for long-press and one-tap for
// `kEnableExpKitTextClassifierEmail` feature.
extern const char kTextClassifierEmailParameterName[];

// Feature flag to enable Text Classifier for specific entity detection.
BASE_DECLARE_FEATURE(kEnableExpKitTextClassifierDate);
BASE_DECLARE_FEATURE(kEnableExpKitTextClassifierAddress);
BASE_DECLARE_FEATURE(kEnableExpKitTextClassifierPhoneNumber);
BASE_DECLARE_FEATURE(kEnableExpKitTextClassifierEmail);

// Returns true if at least one of the entities above need Text Classifier. This
// is independent of the `kEnableExpKitTextClassifier` feature below.
bool IsExpKitTextClassifierEntityEnabled();

// Flag used only for confidence threshold (confidence_score_threshold)
BASE_DECLARE_FEATURE(kEnableExpKitTextClassifier);

#endif  // IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_SELECTION_UTIL_H_
