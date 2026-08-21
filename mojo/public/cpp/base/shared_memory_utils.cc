// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/shared_memory_utils.h"

#include "mojo/core/embedder/embedder.h"

namespace mojo {

void SharedMemoryUtils::InstallBaseHooks() {
  mojo::core::InstallMojoIpczBaseSharedMemoryHooks();
}

}  // namespace mojo
