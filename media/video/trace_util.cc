// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/trace_util.h"

#include "base/trace_event/memory_allocator_dump_guid.h"
#include "ui/gl/trace_util.h"

namespace media {

base::trace_event::MemoryAllocatorDumpGuid GetGLTextureClientGUIDForTracing(
    uint64_t context_group_tracing_id,
    uint32_t texture_id) {
  return gl::GetGLTextureClientGUIDForTracing(context_group_tracing_id,
                                              texture_id);
}

}  // namespace media
