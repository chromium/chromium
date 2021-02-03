// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/embedder/features.h"

namespace mojo {
namespace core {

#if defined(OS_POSIX) && !defined(OS_NACL) && !defined(OS_MAC)
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
const base::Feature kMojoLinuxChannelSharedMem{
    "MojoLinuxChannelSharedMem", base::FEATURE_DISABLED_BY_DEFAULT};
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
const base::FeatureParam<int> kMojoLinuxChannelSharedMemPages{
    &kMojoLinuxChannelSharedMem, "MojoLinuxChannelSharedMemPages", 4};
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)

COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
const base::Feature kMojoPosixUseWritev{"MojoPosixUseWritev",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_POSIX) && !defined(OS_NACL) && !defined(OS_MAC)

}  // namespace core
}  // namespace mojo
