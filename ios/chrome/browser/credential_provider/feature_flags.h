// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_FEATURE_FLAGS_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_FEATURE_FLAGS_H_

#include "base/feature_list.h"

// Feature for enabling password creation in the extension.
// This feature is used in extensions. If it is modified significantly, consider
// updating the version in |app_group_field_trial_version|.
extern const base::Feature kPasswordCreationEnabled;

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_FEATURE_FLAGS_H_
