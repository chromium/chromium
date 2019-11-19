// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const base::Feature kSettingsAddPaymentMethod{
    "SettingsAddPaymentMethod", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCreditCardScanner{"CreditCardScanner",
                                       base::FEATURE_DISABLED_BY_DEFAULT};
