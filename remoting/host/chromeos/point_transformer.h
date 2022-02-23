// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_POINT_TRANSFORMER_H_
#define REMOTING_HOST_CHROMEOS_POINT_TRANSFORMER_H_

#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"

namespace aura {
class Window;
}  // namespace aura

namespace remoting {

// A class that performs coordinate transformations between the root window
// coordinates and the native screen coordinates, according to the current
// display rotation settings.
//
// Root window coordinates are expressed relative to the top left corner of the
// root window whereas native screen coordinates are expressed relative to the
// top left corner of the frame buffer, which may not match the root window
// either in origin nor orientation.  Both coordinate systems are always in
// device pixels.
//
// For example, when the display is rotated by 90 deg, the pixel at root window
// coordinates (x, y) will have native screen coordinates (height - y, x).
class PointTransformer : public aura::WindowObserver {
 public:
  PointTransformer();

  PointTransformer(const PointTransformer&) = delete;
  PointTransformer& operator=(const PointTransformer&) = delete;

  ~PointTransformer() override;

  // Converts from root window coordinates to native screen coordinates.
  gfx::PointF ToScreenCoordinates(const gfx::PointF& window_location);

  // Converts from native screen coordinates to root window coordinates.
  gfx::PointF FromScreenCoordinates(const gfx::PointF& screen_location);

 private:
  // aura::WindowObserver interface.
  void OnWindowTargetTransformChanging(
      aura::Window* window,
      const gfx::Transform& new_transform) override;

  aura::Window* root_window_;
  gfx::Transform root_to_screen_;
  gfx::Transform screen_to_root_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_POINT_TRANSFORMER_H_
