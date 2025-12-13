// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CAPTURE_VERSION_H_
#define MEDIA_BASE_CAPTURE_VERSION_H_

#include <stdint.h>

#include <string>

#include "media/base/media_export.h"

namespace media {

// Marks the "version" of a capture.
//
// After screen-share is initiated, the capture-session can undergo some
// types of transformations:
// 1. The user can dynamically choose to share something else. In that case,
//    we do NOT stop the capture and restart it; rather, we change the source,
//    delivering frames from the new source to the same MediaStreamTrack.
// 2. The Web application can "mutate" the capture by invoking cropTo() or
//    restrictTo() on the MediaStreamTrack.
//
// These changes are initiated from different processes, and may happen
// concurrently. In order to have a robust ordering, we define the
// CaptureVersion as follows:
// * The "major" part of the version is the `source` version.
//   It is incremented whenever the source changes in response to user
//   interaction with the browser's UX.
// * The "minor" part of the version is the `sub_capture` version.
//   It is incremented whenever the Web app *mutates* the track.
//
// The relative ordering of two pairs `(v1, sv1)` and `(v2, sv2)` is defined
// as for any pair-comparison. That is, the comparison between `v1` and `v2`
// takes precedence; if they are equal, the comparison proceeds
// to `sv1` and `sv2`.
//
// The rest of the system is built such that a concurrent (i) source-change
// and (ii) mutation is handled gracefully; the mutation can either
// (a) take effect before the source-change, (b) after it, or (c) be gracefully
// rejected as referring to a pre-change source. Which of these occurs depends
// on the exact ordering of events.
struct MEDIA_EXPORT CaptureVersion {
  CaptureVersion() = default;
  CaptureVersion(uint32_t source, uint32_t sub_capture);

  auto operator<=>(const CaptureVersion& other) const = default;

  std::string ToString() const;

  uint32_t source = 0;
  uint32_t sub_capture = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_CAPTURE_VERSION_H_
