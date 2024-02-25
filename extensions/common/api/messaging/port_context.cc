// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/messaging/port_context.h"
#include "extensions/common/extension_id.h"

namespace extensions {

PortContext::PortContext() = default;
PortContext::~PortContext() = default;
PortContext::PortContext(const PortContext& other) = default;

PortContext::FrameContext::FrameContext(int routing_id)
    : routing_id(routing_id) {}
PortContext::FrameContext::FrameContext() = default;

PortContext::WorkerContext::WorkerContext(int thread_id,
                                          int64_t version_id,
                                          const ExtensionId& extension_id)
    : thread_id(thread_id),
      version_id(version_id),
      extension_id(extension_id) {}
PortContext::WorkerContext::WorkerContext() = default;

PortContext PortContext::ForFrame(int routing_id) {
  PortContext context;
  context.frame = FrameContext(routing_id);
  return context;
}

PortContext PortContext::ForWorker(int thread_id,
                                   int64_t version_id,
                                   const ExtensionId& extension_id) {
  PortContext context;
  context.worker = WorkerContext(thread_id, version_id, extension_id);
  return context;
}

PortContext PortContext::ForNativeHost() {
  return PortContext();
}

namespace debug {

namespace {

base::debug::CrashKeyString* GetServiceWorkerExtensionIdCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "PortContext-worker-extension_id", base::debug::CrashKeySize::Size64);
  return crash_key;
}

}  // namespace

ScopedPortContextCrashKeys::ScopedPortContextCrashKeys(
    const PortContext& port_context) {
  if (port_context.is_for_service_worker()) {
    extension_id_.emplace(GetServiceWorkerExtensionIdCrashKey(),
                          port_context.worker->extension_id);
  }
}

ScopedPortContextCrashKeys::~ScopedPortContextCrashKeys() = default;

}  // namespace debug
}  // namespace extensions
