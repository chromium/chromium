// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_info.h"

#include "base/check.h"
#include "build/build_config.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "remoting/proto/control.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

DesktopDisplayInfo::DesktopDisplayInfo() = default;
DesktopDisplayInfo::DesktopDisplayInfo(DesktopDisplayInfo&&) = default;
DesktopDisplayInfo& DesktopDisplayInfo::operator=(DesktopDisplayInfo&&) =
    default;
DesktopDisplayInfo::~DesktopDisplayInfo() = default;

bool DesktopDisplayInfo::operator==(const DesktopDisplayInfo& other) const {
  if (other.displays_.size() == displays_.size()) {
    for (size_t display = 0; display < displays_.size(); display++) {
      const DisplayGeometry& this_display = displays_[display];
      const DisplayGeometry& other_display = other.displays_[display];
      if (this_display.id != other_display.id ||
          this_display.x != other_display.x ||
          this_display.y != other_display.y ||
          this_display.width != other_display.width ||
          this_display.height != other_display.height ||
          this_display.dpi != other_display.dpi ||
          this_display.bpp != other_display.bpp ||
          this_display.is_default != other_display.is_default) {
        return false;
      }
    }
    return true;
  }
  return false;
}

bool DesktopDisplayInfo::operator!=(const DesktopDisplayInfo& other) const {
  return !(*this == other);
}

/* static */
webrtc::DesktopSize DesktopDisplayInfo::CalcSizeDips(webrtc::DesktopSize size,
                                                     int dpi_x,
                                                     int dpi_y) {
  // Guard against invalid input.
  // TODO: Replace with a DCHECK, once crbug.com/938648 is fixed.
  if (dpi_x == 0) {
    dpi_x = kDefaultDpi;
  }
  if (dpi_y == 0) {
    dpi_y = kDefaultDpi;
  }

  webrtc::DesktopSize size_dips(size.width() * kDefaultDpi / dpi_x,
                                size.height() * kDefaultDpi / dpi_y);
  return size_dips;
}

void DesktopDisplayInfo::Reset() {
  displays_.clear();
}

int DesktopDisplayInfo::NumDisplays() const {
  return displays_.size();
}

const DisplayGeometry* DesktopDisplayInfo::GetDisplayInfo(
    unsigned int id) const {
  if (id < 0 || id >= displays_.size()) {
    return nullptr;
  }
  return &displays_[id];
}

// Calculate the offset from the origin of the desktop to the origin of the
// specified display.
//
// For Mac and ChromeOS, the origin of the desktop is the origin of the default
// display.
//
// For Windows/Linux, the origin of the desktop is the upper-left of the
// entire desktop region.
//
// x         b-----------+            ---
//           |           |             |  y-offset to c
// a---------+           |             |
// |         +-------c---+-------+    ---
// |         |       |           |
// +---------+       |           |
//                   +-----------+
//
// |-----------------|
//    x-offset to c
//
// x = upper left of desktop
// a,b,c = origin of display A,B,C
webrtc::DesktopVector DesktopDisplayInfo::CalcDisplayOffset(
    webrtc::ScreenId disp_id) const {
  bool full_desktop = (disp_id == webrtc::kFullDesktopScreenId);
  unsigned int disp_index = disp_id;

  if (full_desktop) {
#if BUILDFLAG(IS_APPLE)
    // For Mac, we need to calculate the offset relative to the default
    // display.
    disp_index = 0;
#else
    // For other platforms, the origin for full desktop is 0,0.
    return webrtc::DesktopVector();
#endif  // !BUILDFLAG(IS_APPLE)
  }

  if (displays_.size() == 0) {
    LOG(INFO) << "No display info available";
    return webrtc::DesktopVector();
  }
  if (disp_index >= displays_.size()) {
    LOG(INFO) << "Invalid display id for CalcDisplayOffset: " << disp_index;
    return webrtc::DesktopVector();
  }

  const DisplayGeometry& disp_info = displays_[disp_index];
  webrtc::DesktopVector origin(disp_info.x, disp_info.y);

  // Find topleft-most display coordinate. This is the topleft of the desktop.
  int dx = 0;
  int dy = 0;
  for (const auto& display : displays_) {
    if (display.x < dx) {
      dx = display.x;
    }
    if (display.y < dy) {
      dy = display.y;
    }
  }
  webrtc::DesktopVector topleft(dx, dy);

#if BUILDFLAG(IS_APPLE)
  // Mac display offsets need to be relative to the main display's origin.
  if (full_desktop) {
    // For full desktop, this is the offset to the topleft display coord.
    return topleft;
  } else {
    // For single displays, this offset is stored in the DisplayGeometry
    // x,y values.
    return origin;
  }
#elif BUILDFLAG(IS_CHROMEOS)
  // ChromeOS display offsets need to be relative to the main display's origin,
  // which is stored in the DisplayGeometry x,y values.
  return origin;
#else
  // Return offset to this screen, relative to topleft.
  return origin.subtract(topleft);
#endif  // BUILDFLAG(IS_APPLE)
}

void DesktopDisplayInfo::AddDisplay(const DisplayGeometry& display) {
  displays_.push_back(display);
}

void DesktopDisplayInfo::AddDisplayFrom(
    const protocol::VideoTrackLayout& track) {
  DisplayGeometry display;
  display.id = track.screen_id();
  display.x = track.position_x();
  display.y = track.position_y();
  display.width = track.width();
  display.height = track.height();
  display.dpi = track.x_dpi();
  display.bpp = 24;
  display.is_default = false;
  displays_.push_back(display);
}

std::unique_ptr<protocol::VideoLayout> DesktopDisplayInfo::GetVideoLayoutProto()
    const {
  auto layout = std::make_unique<protocol::VideoLayout>();
  HOST_LOG << "Displays loaded:";
  for (const auto& display : displays()) {
    protocol::VideoTrackLayout* track = layout->add_video_track();
    track->set_position_x(display.x);
    track->set_position_y(display.y);
    track->set_width(display.width);
    track->set_height(display.height);
    track->set_x_dpi(display.dpi);
    track->set_y_dpi(display.dpi);
    track->set_screen_id(display.id);
    HOST_LOG << "   Display: " << display.x << "," << display.y << " "
             << display.width << "x" << display.height << " @ " << display.dpi
             << ", id=" << display.id << ", bpp=" << display.bpp
             << ", primary=" << display.is_default;
    if (display.is_default) {
      if (layout->has_primary_screen_id()) {
        LOG(WARNING) << "Multiple primary displays found";
      }
      layout->set_primary_screen_id(display.id);
    }
  }
  return layout;
}

std::ostream& operator<<(std::ostream& out, const DisplayGeometry& geo) {
  out << "Display " << geo.id << (geo.is_default ? " (primary)" : "") << ": "
      << geo.x << "+" << geo.y << "-" << geo.width << "x" << geo.height << " @ "
      << geo.dpi;
  return out;
}

}  // namespace remoting
