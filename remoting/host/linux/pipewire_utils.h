// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PIPEWIRE_UTILS_H_
#define REMOTING_HOST_LINUX_PIPEWIRE_UTILS_H_

#include "base/memory/raw_ptr.h"
#include "library_loaders/remoting_libpipewire.h"

namespace remoting {

// Returns the singleton instance of the PipeWire library loader. Make sure you
// only use it after calling EnsurePipewireInitialized().
RemotingPipewireLoader& GetPipewireLoader();

// Ensures the PipeWire library is loaded and initialized.
// Returns true if PipeWire is ready to use.
bool EnsurePipewireInitialized();

// RAII helper to lock the PipeWire thread loop and unlock it when it goes out
// of scope.
class ScopedThreadLoopLock {
 public:
  ScopedThreadLoopLock(const ScopedThreadLoopLock&) = delete;
  ScopedThreadLoopLock& operator=(const ScopedThreadLoopLock&) = delete;

  explicit ScopedThreadLoopLock(struct pw_thread_loop* loop);

  ~ScopedThreadLoopLock();

 private:
  raw_ptr<struct pw_thread_loop> loop_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_UTILS_H_
