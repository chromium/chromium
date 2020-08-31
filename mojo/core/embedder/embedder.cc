// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/embedder/embedder.h"

#include <stdint.h>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "mojo/core/configuration.h"
#include "mojo/core/core.h"
#include "mojo/core/entrypoints.h"
#include "mojo/core/node_controller.h"
#include "mojo/public/c/system/thunks.h"

namespace mojo {
namespace core {

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
