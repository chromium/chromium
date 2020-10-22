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
#include "mojo/core/entrypoints.h"
#include "mojo/core/node_controller.h"
#include "mojo/public/c/system/thunks.h"

namespace mojo {
namespace core {

namespace {
#if defined(OS_POSIX) && !defined(OS_NACL) && !defined(OS_MAC)
const base::Feature kMojoPosixUseWritev{"MojoPosixUseWritev",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
#endif
}  // namespace

// InitFeatures will be called as soon as the base::FeatureList is initialized.
void InitFeatures() {
#if defined(OS_POSIX) && !defined(OS_NACL) && !defined(OS_MAC)
  Channel::set_posix_use_writev(
      base::FeatureList::IsEnabled(kMojoPosixUseWritev));
#endif
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
