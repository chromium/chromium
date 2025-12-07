// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_GEOMETRY_H_
#define REMOTING_HOST_DESKTOP_GEOMETRY_H_

#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace remoting {

class DesktopResolution {
 public:
  DesktopResolution(gfx::Size dimensions, gfx::Vector2d dpi)
      : dimensions_(dimensions), dpi_(dpi) {}
  const gfx::Size dimensions() const { return dimensions_; }
  const gfx::Vector2d dpi() const { return dpi_; }

 private:
  gfx::Size dimensions_;
  gfx::Vector2d dpi_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_GEOMETRY_H_
