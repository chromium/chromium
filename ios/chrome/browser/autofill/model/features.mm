// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/features.h"

#import "base/feature_list.h"

BASE_FEATURE(kAutofillBottomSheetNewBlur, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kStatelessFormSuggestionController,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kStatelessFormSuggestionControllerWithRequestDeduping,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kThrottleFormInputAccessorySuggestionRefresh,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFormInputAccessorySkipInputViewReloadInBackground,
             base::FEATURE_ENABLED_BY_DEFAULT);
