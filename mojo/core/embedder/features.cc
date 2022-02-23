// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/embedder/features.h"

#include "build/build_config.h"

namespace mojo {
namespace core {

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
const base::Feature kMojoLinuxChannelSharedMem{
    "MojoLinuxChannelSharedMem", base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<int> kMojoLinuxChannelSharedMemPages{
    &kMojoLinuxChannelSharedMem, "MojoLinuxChannelSharedMemPages", 4};
const base::FeatureParam<bool> kMojoLinuxChannelSharedMemEfdZeroOnWake{
    &kMojoLinuxChannelSharedMem, "MojoLinuxChannelSharedMemEfdZeroOnWake",
    false};
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

const base::Feature kMojoPosixUseWritev{"MojoPosixUseWritev",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_MAC)

const base::Feature kMojoInlineMessagePayloads{
    "MojoInlineMessagePayloads", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace core
}  // namespace mojo
