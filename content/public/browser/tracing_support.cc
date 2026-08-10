// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/tracing_support.h"

#include <optional>

#include "base/hash/hash.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/common/tracing_support.h"

namespace content {
namespace {
// A random value mixed into child process track uuids to avoid collisions with
// other types of tracks.
static constexpr uint64_t kProcessTrackMagic = 0x12E91CFDD3E3509Bul;

std::optional<base::trace_event::TrackRegistration<perfetto::NamedTrack>>&
GetWebContentsListTrackRegistrationStorage() {
  static base::NoDestructor<
      std::optional<base::trace_event::TrackRegistration<perfetto::NamedTrack>>>
      page_group_track;
  return *page_group_track;
}

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

perfetto::StateTrack GetLocalFrameTracingTrack(
    const blink::LocalFrameToken& frame_token,
    bool is_main_frame,
    ChildProcessId process_id) {
  return blink::GetLocalFrameTracingTrack(
      frame_token, is_main_frame, GetChildProcessTracingTrack(process_id));
}

perfetto::StateTrack GetWebContentsTracingTrack(
    const WebContents::UniqueToken& web_contents_token,
    perfetto::StaticString name) {
  auto& page_group_track = GetWebContentsListTrackRegistrationStorage();
  if (!page_group_track.has_value()) {
    page_group_track.emplace(
        perfetto::NamedTrack::Global("WebContentsList", 0));
  }

  auto track = perfetto::StateTrack(
      name, base::UnguessableTokenHash()(web_contents_token.value()),
      page_group_track->track());
  return track;
}

CONTENT_EXPORT perfetto::Flow GetNavigationTracingFlow(int64_t navigation_id) {
  return perfetto::Flow::Global(navigation_id, "navigation_id");
}

void ResetWebContentsListTrackRegistrationForTesting() {
  GetWebContentsListTrackRegistrationStorage().reset();
}

}  // namespace content
