// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_MESSAGING_PORT_CONTEXT_H_
#define EXTENSIONS_COMMON_API_MESSAGING_PORT_CONTEXT_H_

#include <stddef.h>

#include <optional>
#include <string>

#include "base/debug/crash_logging.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// A port can refer to a RenderFrame (FrameContext) or a Service Worker
// (WorkerContext) or a native messaging host.
struct PortContext {
 public:
  // This constructor is needed by our IPC code and tests. Clients should use
  // factory functions instead.
  PortContext();

  ~PortContext();
  PortContext(const PortContext& other);

  struct FrameContext {
    FrameContext();
    explicit FrameContext(int routing_id);

    // The routing id of the frame context.
    // This may be MSG_ROUTING_NONE if the context is process-wide and isn't
    // tied to a specific RenderFrame.
    int routing_id;
  };

  struct WorkerContext {
    WorkerContext();
    WorkerContext(int thread_id,
                  int64_t version_id,
                  const ExtensionId& extension_id);

    int thread_id;
    int64_t version_id;
    ExtensionId extension_id;
  };

  static PortContext ForFrame(int routing_id);
  static PortContext ForWorker(int thread_id,
                               int64_t version_id,
                               const ExtensionId& extension_id);
  static PortContext ForNativeHost();

  bool is_for_render_frame() const { return frame.has_value(); }
  bool is_for_service_worker() const { return worker.has_value(); }
  bool is_for_native_host() const { return !frame && !worker; }

  std::optional<FrameContext> frame;
  std::optional<WorkerContext> worker;
};

namespace debug {

class ScopedPortContextCrashKeys {
 public:
  explicit ScopedPortContextCrashKeys(const PortContext& port_context);
  ~ScopedPortContextCrashKeys();

 private:
  std::optional<base::debug::ScopedCrashKeyString> extension_id_;
};

}  // namespace debug

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_MESSAGING_PORT_CONTEXT_H_
