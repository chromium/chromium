// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/tracing_support.h"

#include "base/hash/hash.h"
#include "base/trace_event/trace_event.h"

namespace blink {

perfetto::NamedTrack BLINK_COMMON_EXPORT
GetLocalFrameTracingTrack(const LocalFrameToken& frame_token,
                          bool is_main_frame,
                          perfetto::Track parent) {
  auto track =
      perfetto::NamedTrack(
          perfetto::StaticString(is_main_frame ? "MainFrame" : "SubFrame"),
          base::PersistentHash(frame_token->AsBytes()), parent)
          .disable_sibling_merge();
  return track;
}

}  // namespace blink
