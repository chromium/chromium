// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/features.h"

#import "base/feature_list.h"

// Enables the throttling of form suggestion refresh in the form input keyboard
// accessory.
BASE_FEATURE(kThrottleFormInputAccessorySuggestionRefresh,
             "ThrottleFormInputAccessorySuggestionRefresh",
             base::FEATURE_ENABLED_BY_DEFAULT);
