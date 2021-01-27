// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/embedder/embedder.h"

#include <stdint.h>
#include <atomic>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "build/build_config.h"
#include "mojo/core/channel.h"
#include "mojo/core/configuration.h"
#include "mojo/core/core.h"
#include "mojo/core/embedder/features.h"
#include "mojo/core/entrypoints.h"
#include "mojo/core/node_controller.h"
#include "mojo/public/c/system/thunks.h"

#if !defined(OS_NACL)
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
#include "mojo/core/channel_linux.h"
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
#endif  // !defined(OS_NACL)

namespace mojo {
namespace core {

// InitFeatures will be called as soon as the base::FeatureList is initialized.
void InitFeatures() {
#if defined(OS_POSIX) && !defined(OS_NACL) && !defined(OS_MAC)
  Channel::set_posix_use_writev(
      base::FeatureList::IsEnabled(kMojoPosixUseWritev));

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
  bool shared_mem_enabled =
      base::FeatureList::IsEnabled(kMojoLinuxChannelSharedMem);
  int num_pages = kMojoLinuxChannelSharedMemPages.Get();
  if (num_pages < 0) {
    num_pages = 4;
  } else if (num_pages > 128) {
    num_pages = 128;
  }

  ChannelLinux::SetSharedMemParameters(shared_mem_enabled,
                                       static_cast<unsigned int>(num_pages));
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
#endif  // defined(OS_POSIX) && !defined(OS_NACL) && !defined(OS_MAC)
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
