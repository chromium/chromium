// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_TEST_UTIL_H_
#define REMOTING_HOST_LINUX_TEST_UTIL_H_

#include <vector>

#include "remoting/host/linux/gnome_display_config.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

GnomeDisplayConfig::MonitorInfo CreateMonitorInfo(
    int x,
    int y,
    int width,
    int height,
    double scale,
    std::vector<double> supported_scales = {1.0, 1.5, 2.0, 2.5, 3.0});

std::ostream& operator<<(std::ostream& os,
                         const GnomeDisplayConfig::MonitorInfo& monitor);

// Simple wrapper around webrtc::DesktopSize to allow ASSERT_EQ/EXPECT_EQ to
// work with desktop sizes.
struct TestDesktopSize {
  // NOLINTNEXTLINE(google-explicit-constructor)
  TestDesktopSize(const webrtc::DesktopSize& size);
  TestDesktopSize(int width, int height);
  ~TestDesktopSize();

  bool operator==(const TestDesktopSize&) const;
  webrtc::DesktopSize size;
};

std::ostream& operator<<(std::ostream& os, const TestDesktopSize& size);

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_TEST_UTIL_H_
