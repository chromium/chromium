// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TRACING_SUPPORT_H_
#define CONTENT_PUBLIC_BROWSER_TRACING_SUPPORT_H_

#include "content/common/content_export.h"
#include "content/public/browser/child_process_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace content {

// Returns a perfetto Track that represents a child process, given `process_id`.
// Do not emit events directly to this track, because it's not a real process
// track. It can only be used to create new perfetto tracks nested under the
// child process track:
//
// auto track = perfetto::NamedTrack("Name", id,
//     GetChildProcessTracingTrack(process_id));
CONTENT_EXPORT perfetto::Track GetChildProcessTracingTrack(
    ChildProcessId process_id);

// Returns a perfetto NamedTrack nested under a child process identified by
// `process_id`. This may be used to emit events relating to a child process.
CONTENT_EXPORT perfetto::NamedTrack CreateTracingTrackUnderChildProcess(
    ChildProcessId process_id,
    perfetto::StaticString name,
    uint64_t id = 0);

// Returns a perfetto Track that represents a local frame identified by
// `frame_token` within `process_id`. This may be used to emit events relating
// to a specific frame. It can be used to create new perfetto tracks nested
// under the frame track:
//
// auto track = perfetto::NamedTrack("Name", id,
//     GetLocalFrameTracingTrack(frame_token, true));
CONTENT_EXPORT perfetto::NamedTrack GetLocalFrameTracingTrack(
    const blink::LocalFrameToken& frame_token,
    bool is_main_frame,
    ChildProcessId process_id);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TRACING_SUPPORT_H_
