// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/messaging/port_context.h"

namespace extensions {

PortContext::PortContext() = default;
PortContext::~PortContext() = default;
PortContext::PortContext(const PortContext& other) = default;

PortContext::FrameContext::FrameContext(int routing_id)
    : routing_id(routing_id) {}
PortContext::FrameContext::FrameContext() = default;

PortContext::WorkerContext::WorkerContext(int thread_id,
                                          int64_t version_id,
                                          const std::string& extension_id)
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
                                   const std::string& extension_id) {
  PortContext context;
  context.worker = WorkerContext(thread_id, version_id, extension_id);
  return context;
}

PortContext PortContext::ForNativeHost() {
  return PortContext();
}

}  // namespace extensions
