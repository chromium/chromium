// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/features.h"

#import "base/feature_list.h"

// Enables the stateless form suggestion controller.
BASE_FEATURE(kStatelessFormSuggestionController,
             "StatelessFormSuggestionController",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the throttling of form suggestion refresh in the form input keyboard
// accessory.
BASE_FEATURE(kThrottleFormInputAccessorySuggestionRefresh,
             "ThrottleFormInputAccessorySuggestionRefresh",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables skipping reloading input views in the form input keyboard accessory
// when the app is in the background.
BASE_FEATURE(kFormInputAccessorySkipInputViewReloadInBackground,
             "FormInputAccessorySkipInputViewReloadInBackground",
             base::FEATURE_ENABLED_BY_DEFAULT);
