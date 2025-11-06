// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_COORDINATE_CONVERSION_H_
#define REMOTING_PROTOCOL_COORDINATE_CONVERSION_H_

#include "remoting/proto/coordinates.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting::protocol {

// Converts a local absolute coordinate to a fractional coordinate. Coordinate
// values will be clamped between [0, 1], if they are out of bounds.
FractionalCoordinate ToFractionalCoordinate(
    webrtc::ScreenId screen_id,
    const webrtc::DesktopSize& screen_size,
    const webrtc::DesktopVector& coordinate);

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_COORDINATE_CONVERSION_H_
