// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_FEATURES_H_

#import "base/feature_list.h"

// Feature flag to allow users to import passwords from Safari.
BASE_DECLARE_FEATURE(kImportPasswordsFromSafari);

// Enable crowdsourcing uploads for the Password Manager. Used as a kill switch,
// enabled by default.
BASE_DECLARE_FEATURE(kPasswordManagerEnableCrowdsourcingUploads);

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_FEATURES_H_
