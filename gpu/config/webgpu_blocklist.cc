// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/webgpu_blocklist.h"

#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gl/buildflags.h"

#if BUILDFLAG(USE_DAWN)
#include "gpu/config/webgpu_blocklist_impl.h"  // nogncheck
#endif

namespace gpu {

namespace {

#if BUILDFLAG(USE_DAWN)
// List of patterns, delimited by |. Each pattern is of the form:
// `vendorId(:deviceIdOrArchitecture(:driverDescription)?)?`
// Vendor and device ids should be in hexadecimal representation, without a
// leading `0x`.
// The FeatureParam may be overriden via Finch config, or via the command line
// with
// --force-fieldtrial-params=WebGPU.Enabled:AdapterBlockList/params
// where `params` is URL-encoded.
const base::FeatureParam<std::string> kAdapterBlockList{
    &features::kWebGPUService, "AdapterBlockList", ""};
#endif

}  // namespace

bool IsWebGPUAdapterBlocklisted(const WGPUAdapterProperties& properties) {
#if BUILDFLAG(USE_DAWN)
  return IsWebGPUAdapterBlocklisted(properties, kAdapterBlockList.Get());
#else
  return true;
#endif
}

}  // namespace gpu
