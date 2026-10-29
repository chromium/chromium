// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_TRACE_UTILS_H_
#define MEDIA_AUDIO_AUDIO_TRACE_UTILS_H_

#include <cstdint>

#include "media/base/media_export.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_args.h"

namespace base {
class UnguessableToken;
}  // namespace base

// Tracing helpers shared by the audio service and the renderer.

namespace media {

// Perfetto flows that link the audio service's and the renderer's trace events
// for each audio buffer they exchange through shared memory.

// Returns the base of the flow IDs for the shared memory region with
// `shared_memory_guid`. Both processes compute the same value. It's a hash of
// the GUID, so traces don't reveal the GUID itself.
MEDIA_EXPORT uint64_t
GetAudioTraceFlowBaseId(const base::UnguessableToken& shared_memory_guid);

// Returns the flow ID for buffer `buffer_index`. The audio service counts
// buffers with 32-bit indices that wrap, so callers with wider counters must
// truncate them.
constexpr uint64_t GetAudioTraceFlowId(uint64_t base_id,
                                       uint32_t buffer_index) {
  return base_id ^ buffer_index;
}

// Returns the flow to attach to the event that starts work on buffer
// `buffer_index`, e.g. the audio service requesting or writing it.
inline perfetto::Flow AudioBufferFlow(uint64_t base_id, uint32_t buffer_index) {
  return perfetto::Flow::Global(GetAudioTraceFlowId(base_id, buffer_index));
}

// Returns the flow to attach to the event that finishes work on buffer
// `buffer_index`, e.g. the other process consuming it.
inline perfetto::TerminatingFlow AudioBufferTerminatingFlow(
    uint64_t base_id,
    uint32_t buffer_index) {
  return perfetto::TerminatingFlow::Global(
      GetAudioTraceFlowId(base_id, buffer_index));
}

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_TRACE_UTILS_H_
