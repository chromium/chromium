// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/features.h"

#import "ios/chrome/browser/ui/infobars/infobar_constants.h"

BASE_FEATURE(kPasswordInfobarDisplayLength,
             "PasswordInfobarDisplayLength",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The default value is the same as the
// kInfobarBannerDefaultPresentationDuration constant.
constexpr base::FeatureParam<int> kPasswordInfobarDisplayLengthParam{
    &kPasswordInfobarDisplayLength,
    /*name=*/"duration-seconds", /*default_value=*/12};

BASE_FEATURE(kCreditCardInfobarDisplayLength,
             "CreditCardInfobarDisplayLength",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The default value is the same as the
// kInfobarBannerDefaultPresentationDuration constant.
constexpr base::FeatureParam<int> kCreditCardInfobarDisplayLengthParam{
    &kCreditCardInfobarDisplayLength,
    /*name=*/"duration-seconds", /*default_value=*/12};

BASE_FEATURE(kAddressInfobarDisplayLength,
             "AddressInfobarDisplayLength",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The default value is the same as the
// kInfobarBannerDefaultPresentationDuration constant.
constexpr base::FeatureParam<int> kAddressInfobarDisplayLengthParam{
    &kAddressInfobarDisplayLength,
    /*name=*/"duration-seconds", /*default_value=*/12};
