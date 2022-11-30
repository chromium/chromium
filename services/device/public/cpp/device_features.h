// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the
// services/device module.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_DEVICE_FEATURES_H_
#define SERVICES_DEVICE_PUBLIC_CPP_DEVICE_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "services/device/public/cpp/device_features_export.h"

namespace features {

// The features should be documented alongside the definition of their values
// in the .cc file.
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kGenericSensorExtraClasses);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kWinrtGeolocationImplementation);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kMacCoreLocationBackend);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kAsyncSensorCalls);

}  // namespace features

#endif  // SERVICES_DEVICE_PUBLIC_CPP_DEVICE_FEATURES_H_
