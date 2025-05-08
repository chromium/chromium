// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/features.h"

#import "base/feature_list.h"

BASE_FEATURE(kStatelessFormSuggestionController,
             "StatelessFormSuggestionController",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kStatelessFormSuggestionControllerWithRequestDeduping,
             "StatelessFormSuggestionControllerWithRequestDeduping",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kThrottleFormInputAccessorySuggestionRefresh,
             "ThrottleFormInputAccessorySuggestionRefresh",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFormInputAccessorySkipInputViewReloadInBackground,
             "FormInputAccessorySkipInputViewReloadInBackground",
             base::FEATURE_ENABLED_BY_DEFAULT);
