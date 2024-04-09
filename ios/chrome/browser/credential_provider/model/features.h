// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_FEATURES_H_

#include "base/feature_list.h"

// Feature flag to enable the performance improvements for the credential
// provider.
BASE_DECLARE_FEATURE(kCredentialProviderPerformanceImprovements);

// Returns whether the CPE Performance Improvement Feature is enabled.
bool IsCPEPerformanceImprovementsEnabled();

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_FEATURES_H_
