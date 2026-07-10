// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_COORDINATE_CONVERTER_H_
#define REMOTING_PROTOCOL_COORDINATE_CONVERTER_H_

#include <optional>

#include "remoting/proto/control.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting::protocol {

// Class for converting fractional coordinates between [0, 1] to absolute
// coordinates. The converter is pixel-type-agnostic and whether the absolute
// coordinates are in DIPs or physical pixels depends on the pixel type of the
// provided VideoLayout.
// TODO: yuweih - move logic in this class into coordinate_conversion.h.
class CoordinateConverter {
 public:
  CoordinateConverter();
  ~CoordinateConverter();

  CoordinateConverter(const CoordinateConverter&) = delete;
  CoordinateConverter& operator=(const CoordinateConverter&) = delete;

  // Sets the video layout to be used to convert fractional coordinates for
  // injection.
  void set_video_layout(const VideoLayout& layout);

  // Converts the fractional coordinate to a global absolute coordinate. Returns
  // nullopt if conversion fails, usually because the screen ID is not found.
  std::optional<webrtc::DesktopVector> ToGlobalAbsoluteCoordinate(
      const FractionalCoordinate& fractional) const;

 private:
  VideoLayout video_layout_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_COORDINATE_CONVERTER_H_
