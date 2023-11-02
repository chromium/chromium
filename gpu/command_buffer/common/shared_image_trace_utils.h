// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_TRACE_UTILS_H_
#define GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_TRACE_UTILS_H_

#include "base/trace_event/memory_allocator_dump_guid.h"
#include "gpu/gpu_export.h"

namespace gpu {

struct Mailbox;

// Generates a memory tracing GUID for `mailbox` to be used for shared
// MemoryAllocatorDump. This allows GPU clients and service to link together
// memory dumps for the shared image.
GPU_EXPORT base::trace_event::MemoryAllocatorDumpGuid
GetSharedImageGUIDForTracing(const Mailbox& mailbox);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_TRACE_UTILS_H_
