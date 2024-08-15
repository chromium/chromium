// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/embedder/features.h"

#include "build/build_config.h"

namespace mojo {
namespace core {

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) && \
    !BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kMojoLinuxChannelSharedMem,
             "MojoLinuxChannelSharedMem",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kMojoLinuxChannelSharedMemPages{
    &kMojoLinuxChannelSharedMem, "MojoLinuxChannelSharedMemPages", 4};
const base::FeatureParam<bool> kMojoLinuxChannelSharedMemEfdZeroOnWake{
    &kMojoLinuxChannelSharedMem, "MojoLinuxChannelSharedMemEfdZeroOnWake",
    false};
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kMojoPosixUseWritev,
             "MojoPosixUseWritev",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) &&
        // !BUILDFLAG(MOJO_USE_APPLE_CHANNEL)

BASE_FEATURE(kMojoInlineMessagePayloads,
             "MojoInlineMessagePayloads",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)
#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kMojoIpcz, "MojoIpcz", base::FEATURE_DISABLED_BY_DEFAULT);
#else
BASE_FEATURE(kMojoIpcz, "MojoIpcz", base::FEATURE_ENABLED_BY_DEFAULT);
#endif
#endif  // BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)

BASE_FEATURE(kMojoIpczMemV2,
             "MojoIpczMemV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kMojoUseBinder,
             "MojoUseBinder",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace core
}  // namespace mojo
