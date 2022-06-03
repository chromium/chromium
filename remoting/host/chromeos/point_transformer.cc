// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/point_transformer.h"

#include "ash/shell.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"

namespace remoting {

PointTransformer::PointTransformer() {
  root_window_ = ash::Shell::GetPrimaryRootWindow();
  root_window_->AddObserver(this);
  // Set the initial display rotation.
  OnWindowTargetTransformChanging(root_window_,
                                  root_window_->layer()->GetTargetTransform());
}

PointTransformer::~PointTransformer() {
  root_window_->RemoveObserver(this);
}

void PointTransformer::OnWindowTargetTransformChanging(
    aura::Window* window,
    const gfx::Transform& new_transform) {
  CHECK_EQ(window, root_window_);

  ui::Layer* layer = root_window_->layer();
  float scale = layer->device_scale_factor();

  gfx::Transform to_device_pixels;
  gfx::Transform to_dip;

  to_device_pixels.Scale(scale, scale);
  to_dip.Scale(1 / scale, 1 / scale);

  // Use WindowTreeHost::GetRootTransform instead of |new_transform| because
  // |new_transform| no longer contains rotation. The root transform contains
  // a transform comprising a rotation and a translation, but it expects DIPs as
  // input in DIPs and convert to device pixels. So we need to switch device
  // pixels to DIPs then apply it.
  gfx::Transform rotation = root_window_->GetHost()->GetRootTransform();

  // Use GetInverseRootTransform instead of using root transform inverse. The
  // two normally should be the same but GetInverseRootTransform() is
  // constructed at the same time of root transform and has less rounding
  // errors. It expect device pixels as input and convert to DIPs. So we need to
  // switch to device pixels after applying it.
  gfx::Transform inverse_rotation =
      root_window_->GetHost()->GetInverseRootTransform();

  // Matrix transformations are applied from right to left.  See annotations.
  //                (2)        (1)
  root_to_screen_ = rotation * to_dip;
  screen_to_root_ = to_device_pixels * inverse_rotation;
}

gfx::PointF PointTransformer::ToScreenCoordinates(
    const gfx::PointF& root_location) {
  gfx::Point3F screen_location(root_location);
  root_to_screen_.TransformPoint(&screen_location);
  return screen_location.AsPointF();
}

gfx::PointF PointTransformer::FromScreenCoordinates(
    const gfx::PointF& screen_location) {
  gfx::Point3F root_location(screen_location);
  screen_to_root_.TransformPoint(&root_location);
  return root_location.AsPointF();
}

}  // namespace remoting
