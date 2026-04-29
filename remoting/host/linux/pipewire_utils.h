// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PIPEWIRE_UTILS_H_
#define REMOTING_HOST_LINUX_PIPEWIRE_UTILS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "library_loaders/remoting_libpipewire.h"

namespace remoting {

// RAII helpers to manage the lifetime of PipeWire objects.
struct PipewireThreadLoopDeleter {
  void operator()(struct pw_thread_loop* loop);
};
using ScopedPipewireMainLoop =
    std::unique_ptr<struct pw_thread_loop, PipewireThreadLoopDeleter>;

struct PipewireContextDeleter {
  void operator()(struct pw_context* context);
};
using ScopedPipewireContext =
    std::unique_ptr<struct pw_context, PipewireContextDeleter>;

struct PipewireCoreDeleter {
  void operator()(struct pw_core* core);
};
using ScopedPipewireCore = std::unique_ptr<struct pw_core, PipewireCoreDeleter>;

struct PipewireStreamDeleter {
  void operator()(struct pw_stream* stream);
};
using ScopedPipewireStream =
    std::unique_ptr<struct pw_stream, PipewireStreamDeleter>;

struct PipewireProxyDeleter {
  void operator()(struct pw_proxy* proxy);
};
using ScopedPipewireProxy =
    std::unique_ptr<struct pw_proxy, PipewireProxyDeleter>;

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

// Returns the singleton instance of the PipeWire library loader. Make sure you
// only use it after calling EnsurePipewireInitialized().
// Note: Any function that calls functions in the pipewire loader must be
// annotated with DISABLE_CFI_DLSYM. Otherwise the release build will crash with
// SIGILL.
RemotingPipewireLoader& GetPipewireLoader();

// Ensures the PipeWire library is loaded and initialized.
// Returns true if PipeWire is ready to use.
bool EnsurePipewireInitialized();

// Creates a PipeWire stream and connects to it.
// `props`: This method will take the ownership of `props`.
// `data`: Data to be passed to the functions in `stream_events`.
ScopedPipewireStream CreatePipewireStream(
    ScopedPipewireCore& core,
    struct pw_properties* props,
    const struct pw_stream_events* stream_events,
    void* data,
    struct spa_hook* listener,
    pw_direction direction,
    uint32_t rate,
    uint32_t channels);

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_UTILS_H_
