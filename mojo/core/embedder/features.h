// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_EMBEDDER_FEATURES_H_
#define MOJO_CORE_EMBEDDER_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace mojo {
namespace core {

#if defined(OS_POSIX) && !defined(OS_NACL) && !defined(OS_MAC)
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
extern const base::Feature kMojoLinuxChannelSharedMem;

COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
extern const base::FeatureParam<int> kMojoLinuxChannelSharedMemPages;

COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
extern const base::FeatureParam<bool> kMojoLinuxChannelSharedMemEfdZeroOnWake;
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)

COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
extern const base::Feature kMojoPosixUseWritev;

#endif  // defined(OS_POSIX) && !defined(OS_NACL) && !defined(OS_MAC)

COMPONENT_EXPORT(MOJO_CORE_EMBEDDER_FEATURES)
extern const base::Feature kMojoInlineMessagePayloads;

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_EMBEDDER_FEATURES_H_
