// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_EMBEDDER_FEATURES_H_
#define MOJO_CORE_EMBEDDER_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "mojo/buildflags.h"

namespace mojo {
namespace core {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
BASE_DECLARE_FEATURE(kMojoUseEventFd);

COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
extern const base::FeatureParam<int> kMojoUseEventFdPages;

COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
extern const char kSuppressEventfdUpgradeForWebview[];
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
BASE_DECLARE_FEATURE(kMojoIpczMemV2);

// Selects the parcel capacity policy (IpczMemoryFlags) of Mojo's ipcz nodes, as
// read by InitFeatures(). If disabled, Mojo uses
// IPCZ_MEMORY_FIXED_PARCEL_CAPACITY.
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
BASE_DECLARE_FEATURE(kMojoIpczParcelCapacityPolicy);

// Parcel capacity policies selectable through kMojoIpczParcelCapacityPolicy.
// See IpczMemoryFlags in third_party/ipcz/include/ipcz/ipcz.h.
// LINT.IfChange(MojoIpczParcelCapacityPolicy)
enum class MojoIpczParcelCapacityPolicy {
  // IPCZ_MEMORY_FIXED_PARCEL_CAPACITY.
  kFixed,
  // IPCZ_MEMORY_NO_PARCEL_CAPACITY.
  kNoCapacity,
  // IPCZ_MEMORY_EXPAND_PARCEL_CAPACITY_ONCE.
  kExpandOnce,
  // IPCZ_MEMORY_RESTRICTED_DYNAMIC_PARCEL_CAPACITY.
  kRestrictedDynamic,
  // No policy flag, so ipcz expands parcel capacity on demand.
  kDynamic,
};
// LINT.ThenChange(//third_party/ipcz/include/ipcz/ipcz.h:ParcelCapacityPolicyFlags)

// The "policy" param of kMojoIpczParcelCapacityPolicy. Defaults to kFixed if
// missing or unknown.
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
extern const base::FeatureParam<MojoIpczParcelCapacityPolicy>
    kMojoIpczParcelCapacityPolicyParam;

COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
BASE_DECLARE_FEATURE(kMojoFixGeometricBufferGrowth);

COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
BASE_DECLARE_FEATURE(kMojoFixGeometricBufferGrowth2);

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
BASE_DECLARE_FEATURE(kMojoHandleTypeProtections);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_EMBEDDER_FEATURES_H_
