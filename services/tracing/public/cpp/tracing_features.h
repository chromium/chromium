// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the
// services/tracing module.

#ifndef SERVICES_TRACING_PUBLIC_CPP_TRACING_FEATURES_H_
#define SERVICES_TRACING_PUBLIC_CPP_TRACING_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace features {

// The features should be documented alongside the definition of their values
// in the .cc file.
extern const COMPONENT_EXPORT(TRACING_CPP) base::Feature
    kTracingServiceInProcess;

extern const COMPONENT_EXPORT(TRACING_CPP) base::Feature
    kEnablePerfettoSystemTracing;

extern const COMPONENT_EXPORT(TRACING_CPP) base::Feature
    kEnablePerfettoSystemBackgroundTracing;

}  // namespace features

namespace tracing {

// Returns true if the system tracing Perfetto producer should be setup. This
// can be influenced by the feature above or other situations (like debug
// android builds).
bool COMPONENT_EXPORT(TRACING_CPP) ShouldSetupSystemTracing();

// Returns true if the system tracing backend is available for background
// tracing scenarios.
bool COMPONENT_EXPORT(TRACING_CPP) SystemBackgroundTracingEnabled();

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_TRACING_FEATURES_H_
