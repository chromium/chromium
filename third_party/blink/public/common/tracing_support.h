// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_TRACING_SUPPORT_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_TRACING_SUPPORT_H_

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace blink {

// Returns a perfetto Track that represents a local frame identified by
// `frame_token`. This may be used to emit events relating to a specific frame.
// It can be used to create new perfetto tracks nested under the frame track:
//
// auto track = perfetto::NamedTrack("Name", id,
//     GetLocalFrameTracingTrack(frame_token, true));
perfetto::NamedTrack BLINK_COMMON_EXPORT GetLocalFrameTracingTrack(
    const LocalFrameToken& frame_token,
    bool is_main_frame,
    perfetto::Track parent_process = perfetto::ProcessTrack::Current());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_TRACING_SUPPORT_H_
