// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the content
// module.

#ifndef SERVICES_SERVICE_MANAGER_SANDBOX_FEATURES_H_
#define SERVICES_SERVICE_MANAGER_SANDBOX_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "services/service_manager/sandbox/export.h"

namespace service_manager {
namespace features {

SERVICE_MANAGER_SANDBOX_EXPORT extern const base::Feature kAudioServiceSandbox;

SERVICE_MANAGER_SANDBOX_EXPORT extern const base::Feature
    kNetworkServiceSandbox;

#if defined(OS_WIN)
SERVICE_MANAGER_SANDBOX_EXPORT extern const base::Feature
    kWinSboxDisableExtensionPoints;
#endif  // defined(OS_WIN)

#if !defined(OS_ANDROID)
SERVICE_MANAGER_SANDBOX_EXPORT extern const base::Feature kXRSandbox;
#endif  // !defined(OS_ANDROID)

}  // namespace features
}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SANDBOX_FEATURES_H_
