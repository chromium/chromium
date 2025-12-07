// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/tracing_support.h"

#include "base/hash/hash.h"
#include "third_party/blink/public/common/tracing_support.h"

namespace content {
namespace {
// A random value mixed into child process track uuids to avoid collisions with
// other types of tracks.
static constexpr uint64_t kProcessTrackMagic = 0x12E91CFDD3E3509Bul;
}  // namespace

perfetto::Track GetChildProcessTracingTrack(ChildProcessId process_id) {
  return perfetto::Track::Global(base::HashInts(
      kProcessTrackMagic, std::hash<ChildProcessId>()(process_id)));
}

perfetto::NamedTrack CreateTracingTrackUnderChildProcess(
    ChildProcessId process_id,
    perfetto::StaticString name,
    uint64_t id) {
  return perfetto::NamedTrack(name, id,
                              GetChildProcessTracingTrack(process_id));
}

perfetto::NamedTrack GetLocalFrameTracingTrack(
    const blink::LocalFrameToken& frame_token,
    bool is_main_frame,
    ChildProcessId process_id) {
  return blink::GetLocalFrameTracingTrack(
      frame_token, is_main_frame, GetChildProcessTracingTrack(process_id));
}

}  // namespace content
