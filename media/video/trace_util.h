// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_TRACE_UTIL_H_
#define MEDIA_VIDEO_TRACE_UTIL_H_

#include <stdint.h>

namespace base {
namespace trace_event {
class MemoryAllocatorDumpGuid;
}
}  // namespace base

namespace media {

base::trace_event::MemoryAllocatorDumpGuid GetGLTextureClientGUIDForTracing(
    uint64_t context_group_tracing_id,
    uint32_t texture_id);

}  // namespace media

#endif  // MEDIA_VIDEO_TRACE_UTIL_H_
