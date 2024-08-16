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

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) && \
    !BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
BASE_DECLARE_FEATURE(kMojoLinuxChannelSharedMem);

COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
extern const base::FeatureParam<int> kMojoLinuxChannelSharedMemPages;

COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
extern const base::FeatureParam<bool> kMojoLinuxChannelSharedMemEfdZeroOnWake;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
BASE_DECLARE_FEATURE(kMojoPosixUseWritev);

#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) &&
        // !BUILDFLAG(MOJO_USE_APPLE_CHANNEL)

COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
BASE_DECLARE_FEATURE(kMojoInlineMessagePayloads);

#if BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES) BASE_DECLARE_FEATURE(kMojoIpcz);
#endif

COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
BASE_DECLARE_FEATURE(kMojoIpczMemV2);

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
BASE_DECLARE_FEATURE(kMojoUseBinder);
#endif

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_EMBEDDER_FEATURES_H_
