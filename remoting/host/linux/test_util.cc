// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/test_util.h"

namespace remoting {

GnomeDisplayConfig::MonitorInfo CreateMonitorInfo(
    int x,
    int y,
    int width,
    int height,
    double scale,
    std::vector<double> supported_scales) {
  GnomeDisplayConfig::MonitorInfo info;
  info.x = x;
  info.y = y;
  info.scale = scale;
  GnomeDisplayConfig::MonitorMode mode;
  mode.width = width;
  mode.height = height;
  mode.is_current = true;
  mode.supported_scales = std::move(supported_scales);
  info.modes.push_back(mode);
  return info;
}

std::ostream& operator<<(std::ostream& os,
                         const GnomeDisplayConfig::MonitorInfo& monitor) {
  os << monitor.x << "," << monitor.y << ":" << monitor.GetCurrentMode()->width
     << "x" << monitor.GetCurrentMode()->height << "@" << monitor.scale
     << "x (supported: ";
  for (auto it = monitor.GetCurrentMode()->supported_scales.begin();
       it != monitor.GetCurrentMode()->supported_scales.end(); ++it) {
    os << *it;
    if (it + 1 != monitor.GetCurrentMode()->supported_scales.end()) {
      os << ", ";
    }
  }
  os << ')';
  if (monitor.is_primary) {
    os << " (primary)";
  }
  return os;
}

TestDesktopSize::TestDesktopSize(const webrtc::DesktopSize& size)
    : size(size) {}

TestDesktopSize::TestDesktopSize(int width, int height) : size(width, height) {}

TestDesktopSize::~TestDesktopSize() = default;

bool TestDesktopSize::operator==(const TestDesktopSize& other) const {
  return size.equals(other.size);
}

std::ostream& operator<<(std::ostream& os, const TestDesktopSize& size) {
  return os << size.size.width() << "x" << size.size.height();
}

}  // namespace remoting
