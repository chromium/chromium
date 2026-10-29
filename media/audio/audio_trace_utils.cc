// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_trace_utils.h"

#include "base/memory/shared_memory_tracker.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/unguessable_token.h"

namespace media {

uint64_t GetAudioTraceFlowBaseId(
    const base::UnguessableToken& shared_memory_guid) {
  // Reuse memory-infra's global ID for the region, which hashes its GUID.
  return base::SharedMemoryTracker::GetGlobalDumpIdForTracing(
             shared_memory_guid)
      .ToUint64();
}

}  // namespace media
