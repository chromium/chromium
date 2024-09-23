// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/text_selection/model/text_selection_util.h"

const char kTextClassifierAddressParameterName[] = "TCAddressOneTap";
const char kTextClassifierPhoneNumberParameterName[] = "TCPhoneNumberOneTap";
const char kTextClassifierEmailParameterName[] = "TCEmailOneTap";

BASE_FEATURE(kEnableExpKitTextClassifier,
             "EnableExpKitTextClassifier",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableExpKitTextClassifierDate,
             "EnableExpKitTextClassifierDate",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableExpKitTextClassifierAddress,
             "EnableExpKitTextClassifierAddress",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableExpKitTextClassifierPhoneNumber,
             "EnableExpKitTextClassifierPhoneNumber",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableExpKitTextClassifierEmail,
             "EnableExpKitTextClassifierEmail",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsExpKitTextClassifierEntityEnabled() {
  return base::FeatureList::IsEnabled(kEnableExpKitTextClassifierDate) ||
         base::FeatureList::IsEnabled(kEnableExpKitTextClassifierAddress) ||
         base::FeatureList::IsEnabled(kEnableExpKitTextClassifierPhoneNumber) ||
         base::FeatureList::IsEnabled(kEnableExpKitTextClassifierEmail);
}
