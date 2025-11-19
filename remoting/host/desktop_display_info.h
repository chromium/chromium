// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_DISPLAY_INFO_H_
#define REMOTING_HOST_DESKTOP_DISPLAY_INFO_H_

#include <stddef.h>

#include <iosfwd>
#include <memory>
#include <optional>

#include "remoting/proto/control.pb.h"
#include "remoting/proto/coordinates.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

namespace protocol {
class VideoLayout;
}  // namespace protocol

struct DisplayGeometry {
  DisplayGeometry();
  DisplayGeometry(webrtc::ScreenId id,
                  int32_t x,
                  int32_t y,
                  uint32_t width,
                  uint32_t height,
                  uint32_t dpi,
                  uint32_t bpp,
                  bool is_default,
                  const std::string& display_name);
  DisplayGeometry(const DisplayGeometry&);
  DisplayGeometry& operator=(const DisplayGeometry&);
  ~DisplayGeometry();

  // Returns whether `global_absolute_coordinate` is within the boundaries of
  // the desktop geometry.
  bool Contains(const webrtc::DesktopVector& global_absolute_coordinate) const;

  webrtc::ScreenId id;
  int32_t x, y;
  uint32_t width, height;
  uint32_t dpi;     // Number of pixels per logical inch.
  uint32_t bpp;     // Number of bits per pixel.
  bool is_default;  // True if this is the default display.
  std::string display_name;  // Sent to the client, informational only.
};

class DesktopDisplayInfo {
 public:
  enum class PixelType {
    // Measurements are in physical screen pixels.
    PHYSICAL,

    // Measurements are in logical pixels, aka. density-independent pixels
    // (DIPs).
    LOGICAL,
  };

  DesktopDisplayInfo();
  DesktopDisplayInfo(DesktopDisplayInfo&&);
  DesktopDisplayInfo& operator=(DesktopDisplayInfo&&);
  ~DesktopDisplayInfo();

  static webrtc::DesktopSize CalcSizeDips(webrtc::DesktopSize size,
                                          int dpi_x,
                                          int dpi_y);

  // Clear out the display info.
  void Reset();
  int NumDisplays() const;
  const DisplayGeometry* GetDisplayInfo(unsigned int id) const;

  // Calculate the offset to the origin (upper left) of the specific display.
  webrtc::DesktopVector CalcDisplayOffset(webrtc::ScreenId id) const;

  // Add a new display with the given info to the display list.
  void AddDisplay(const DisplayGeometry& display);

  void AddDisplayFrom(const protocol::VideoTrackLayout& track);

  bool operator==(const DesktopDisplayInfo& other) const;
  bool operator!=(const DesktopDisplayInfo& other) const;

  const std::vector<DisplayGeometry>& displays() const { return displays_; }

  void set_pixel_type(std::optional<PixelType> pixel_type) {
    pixel_type_ = pixel_type;
  }

  const std::optional<PixelType>& pixel_type() const { return pixel_type_; }

  // Converts a global absolute coordinate to a fractional coordinate. Returns
  // nullopt if the absolute coordinate is not within any display.
  std::optional<protocol::FractionalCoordinate> ToFractionalCoordinate(
      const webrtc::DesktopVector& global_absolute_coordinate) const;

  std::unique_ptr<protocol::VideoLayout> GetVideoLayoutProto() const;

 private:
  std::vector<DisplayGeometry> displays_;
  std::optional<PixelType> pixel_type_;
};

// The output format is:
//      "Display <id>: <x>+<y>-<width>x<height>@<dpi>"
std::ostream& operator<<(std::ostream& out, const DisplayGeometry& geo);
}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_DISPLAY_INFO_H_
