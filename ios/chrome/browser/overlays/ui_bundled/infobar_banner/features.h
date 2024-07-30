// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_FEATURES_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

// Feature to experiment with longer password infobar display duration.
BASE_DECLARE_FEATURE(kPasswordInfobarDisplayLength);

// The param value for the password save infobar display duration.
extern const base::FeatureParam<int> kPasswordInfobarDisplayLengthParam;

// Feature to experiment with longer card save infobar display duration.
BASE_DECLARE_FEATURE(kCreditCardInfobarDisplayLength);

// The param value for the card save infobar display duration.
extern const base::FeatureParam<int> kCreditCardInfobarDisplayLengthParam;

// Feature to experiment with longer address save infobar display duration.
BASE_DECLARE_FEATURE(kAddressInfobarDisplayLength);

// The param value for the address save infobar display duration.
extern const base::FeatureParam<int> kAddressInfobarDisplayLengthParam;

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_FEATURES_H_
