// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_FEATURES_H_

#import "base/feature_list.h"

// Enables the stateless form suggestion controller.
BASE_DECLARE_FEATURE(kStatelessFormSuggestionController);

// Enables deduping concurrent requests even when using the stateless
// FormSuggestionController. This feature is only effective if
// `kStatelessFormSuggestionController` is enabled.
BASE_DECLARE_FEATURE(kStatelessFormSuggestionControllerWithRequestDeduping);

// Enables the throttling of form suggestion refresh in the form input keyboard
// accessory.
BASE_DECLARE_FEATURE(kThrottleFormInputAccessorySuggestionRefresh);

// Enables skipping reloading input views in the form input keyboard accessory
// when the app is in the background.
BASE_DECLARE_FEATURE(kFormInputAccessorySkipInputViewReloadInBackground);

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_FEATURES_H_
