// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_trace_utils.h"

namespace media {

perfetto::Track GetTraceTrack(CameraTraceEvent event,
                              int primary_id,
                              int secondary_id) {
  auto uuid = (static_cast<uint64_t>(primary_id) << 32) +
              (static_cast<uint64_t>(secondary_id & 0xFFFF) << 16) +
              static_cast<uint64_t>(event);
  return perfetto::Track(uuid);
}

}  // namespace media
