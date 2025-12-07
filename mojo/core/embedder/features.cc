// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/embedder/features.h"

#include "build/build_config.h"

namespace mojo {
namespace core {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kMojoUseEventFd, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kMojoUseEventFdPages{&kMojoUseEventFd,
                                                   "MojoUseEventFdPages", 4};
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kMojoInlineMessagePayloads, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)
#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kMojoIpcz, base::FEATURE_DISABLED_BY_DEFAULT);
#else
BASE_FEATURE(kMojoIpcz, base::FEATURE_ENABLED_BY_DEFAULT);
#endif
#endif  // BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)

BASE_FEATURE(kMojoIpczMemV2, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// If enabled, then only handles of types Section, File, Directory and
// DxgkSharedResource are allowed to traverse a process boundary to an untrusted
// process via mojo.
BASE_FEATURE(kMojoHandleTypeProtections, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace core
}  // namespace mojo
