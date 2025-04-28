// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/features.h"

#import "base/feature_list.h"

BASE_FEATURE(kImportPasswordsFromSafari,
             "ImportPasswordsFromSafari",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPasswordManagerEnableCrowdsourcingUploads,
             "PasswordManagerEnableCrowdsourcingUploads",
             base::FEATURE_DISABLED_BY_DEFAULT);
