// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_WAYLAND_DISPLAY_INFO_H_
#define REMOTING_HOST_LINUX_WAYLAND_DISPLAY_INFO_H_

#include <string>

#include <xdg-output-unstable-v1-client-protocol.h>

#include "base/memory/raw_ptr.h"

namespace remoting {

// Collection of parameters for each Wayland Display object available on the
// system.
struct DisplayInfo {
  DisplayInfo();
  DisplayInfo(int x,
              int y,
              int transform,
              int physical_width,
              int physical_height,
              int subpixel);
  DisplayInfo(int width, int height, int refresh);
  DisplayInfo(int scale_factor);
  DisplayInfo(const DisplayInfo&);
  DisplayInfo(DisplayInfo&&);
  DisplayInfo& operator=(const DisplayInfo&);
  DisplayInfo& operator=(DisplayInfo&);
  ~DisplayInfo();

  uint32_t id;
  std::string name;
  std::string description;
  int scale_factor = 1;
  int x = -1;
  int y = -1;
  int width = -1;
  int height = -1;
  int refresh = -1;
  int transform = -1;
  int physical_width = -1;
  int physical_height = -1;
  int subpixel = -1;

  raw_ptr<struct wl_output> output = nullptr;
  raw_ptr<struct zxdg_output_v1> xdg_output = nullptr;

  // Whether last write to x, y was done by an event emitted from the xdg_output
  // interface. If yes, we would to preserve these writes over the ones from
  // wl_output interface.
  bool last_write_from_xdg = false;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_WAYLAND_DISPLAY_INFO_H_
