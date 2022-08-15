// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/embedder/embedder.h"

#include <stdint.h>
#include <atomic>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_runner.h"
#include "build/build_config.h"
#include "mojo/core/channel.h"
#include "mojo/core/configuration.h"
#include "mojo/core/core.h"
#include "mojo/core/embedder/features.h"
#include "mojo/core/entrypoints.h"
#include "mojo/core/node_controller.h"
#include "mojo/public/c/system/thunks.h"

#if !BUILDFLAG(IS_NACL)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include "mojo/core/channel_linux.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)
#endif  // !BUILDFLAG(IS_NACL)

namespace mojo {
namespace core {

// InitFeatures will be called as soon as the base::FeatureList is initialized.
void InitFeatures() {
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_MAC)
  Channel::set_posix_use_writev(
      base::FeatureList::IsEnabled(kMojoPosixUseWritev));

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  bool shared_mem_enabled =
      base::FeatureList::IsEnabled(kMojoLinuxChannelSharedMem);
  bool use_zero_on_wake = kMojoLinuxChannelSharedMemEfdZeroOnWake.Get();
  int num_pages = kMojoLinuxChannelSharedMemPages.Get();
  if (num_pages < 0) {
    num_pages = 4;
  } else if (num_pages > 128) {
    num_pages = 128;
  }

  ChannelLinux::SetSharedMemParameters(shared_mem_enabled,
                                       static_cast<unsigned int>(num_pages),
                                       use_zero_on_wake);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_MAC)

  Channel::set_use_trivial_messages(
      base::FeatureList::IsEnabled(kMojoInlineMessagePayloads));

  Core::set_avoid_random_pipe_id(
      base::FeatureList::IsEnabled(kMojoAvoidRandomPipeId));
}

void Init(const Configuration& configuration) {
  internal::g_configuration = configuration;
  InitializeCore();
  MojoEmbedderSetSystemThunks(&GetSystemThunks());
}

void Init() {
  Init(Configuration());
}

scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() {
  return Core::Get()->GetNodeController()->io_task_runner();
}

}  // namespace core
}  // namespace mojo
