// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the
// services/resource_coordinator module.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_RESOURCE_COORDINATOR_FEATURES_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_RESOURCE_COORDINATOR_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace features {

// The features should be documented alongside the definition of their values
// in the .cc file.
extern const COMPONENT_EXPORT(SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_FEATURES)
    base::Feature kGlobalResourceCoordinator;
extern const COMPONENT_EXPORT(SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_FEATURES)
    base::Feature kPageAlmostIdle;
extern const COMPONENT_EXPORT(SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_FEATURES)
    base::Feature kPerformanceMeasurement;

}  // namespace features

namespace resource_coordinator {

bool COMPONENT_EXPORT(SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_FEATURES)
    IsPageAlmostIdleSignalEnabled();

int COMPONENT_EXPORT(SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_FEATURES)
    GetMainThreadTaskLoadLowThreshold();

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_RESOURCE_COORDINATOR_FEATURES_H_
