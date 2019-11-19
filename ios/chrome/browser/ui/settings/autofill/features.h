// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_FEATURES_H_

#import <Foundation/Foundation.h>

#include "base/feature_list.h"

// Add new credit card feature.
extern const base::Feature kSettingsAddPaymentMethod;

// Scan a credit card using the camera scanner feature.
extern const base::Feature kCreditCardScanner;

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_FEATURES_H_
