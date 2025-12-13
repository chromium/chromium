// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/features.h"

#import "ios/chrome/browser/infobars/ui_bundled/infobar_constants.h"

BASE_FEATURE(kPasswordInfobarDisplayLength, base::FEATURE_DISABLED_BY_DEFAULT);

// The default value is the same as the
// kInfobarBannerDefaultPresentationDuration constant.
constexpr base::FeatureParam<int> kPasswordInfobarDisplayLengthParam{
    &kPasswordInfobarDisplayLength,
    /*name=*/"duration-seconds", /*default_value=*/12};

BASE_FEATURE(kCreditCardInfobarDisplayLength,
             base::FEATURE_DISABLED_BY_DEFAULT);

// The default value is the same as the
// kInfobarBannerDefaultPresentationDuration constant.
constexpr base::FeatureParam<int> kCreditCardInfobarDisplayLengthParam{
    &kCreditCardInfobarDisplayLength,
    /*name=*/"duration-seconds", /*default_value=*/12};

BASE_FEATURE(kAddressInfobarDisplayLength, base::FEATURE_DISABLED_BY_DEFAULT);

// The default value is the same as the
// kInfobarBannerDefaultPresentationDuration constant.
constexpr base::FeatureParam<int> kAddressInfobarDisplayLengthParam{
    &kAddressInfobarDisplayLength,
    /*name=*/"duration-seconds", /*default_value=*/12};
