// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_utils.h"

#include "base/compiler_specific.h"
#include "base/no_destructor.h"
#include "remoting/base/logging.h"

namespace remoting {

RemotingPipewireLoader& GetPipewireLoader() {
  static base::NoDestructor<RemotingPipewireLoader> pipewire_loader;
  return *pipewire_loader;
}

DISABLE_CFI_DLSYM
bool EnsurePipewireInitialized() {
  RemotingPipewireLoader& loader = GetPipewireLoader();
  if (loader.loaded()) {
    return true;
  }

  // Try to load the library with the default name. This will succeed if
  // PipeWire is installed on the system and the library is in the standard
  // search path.
  if (loader.Load("libpipewire-0.3.so.0")) {
    loader.pw_init(nullptr, nullptr);
    return true;
  }

  HOST_LOG << "Cannot load PipeWire library.";
  return false;
}

DISABLE_CFI_DLSYM
ScopedThreadLoopLock::ScopedThreadLoopLock(struct pw_thread_loop* loop)
    : loop_(loop) {
  if (loop_) {
    GetPipewireLoader().pw_thread_loop_lock(loop_);
  }
}

DISABLE_CFI_DLSYM
ScopedThreadLoopLock::~ScopedThreadLoopLock() {
  if (loop_) {
    GetPipewireLoader().pw_thread_loop_unlock(loop_);
  }
}

}  // namespace remoting
