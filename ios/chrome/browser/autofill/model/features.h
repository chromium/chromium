// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_FEATURES_H_

#import "base/feature_list.h"

BASE_DECLARE_FEATURE(kStatelessFormSuggestionController);

BASE_DECLARE_FEATURE(kThrottleFormInputAccessorySuggestionRefresh);

BASE_DECLARE_FEATURE(kFormInputAccessorySkipInputViewReloadInBackground);

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_FEATURES_H_
