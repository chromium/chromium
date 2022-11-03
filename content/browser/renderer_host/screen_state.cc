// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/screen_state.h"

namespace content {

ScreenState::ScreenState() = default;

void ScreenState::CopyDefinedAttributes(const ScreenState& other) {
  if (!other.visible_viewport_size.IsEmpty())
    visible_viewport_size = other.visible_viewport_size;
  if (!other.physical_backing_size.IsEmpty())
    physical_backing_size = other.physical_backing_size;
  if (!other.screen_info_size.IsEmpty())
    screen_info_size = other.screen_info_size;
  if (other.orientation_type != display::mojom::ScreenOrientation::kUndefined)
    orientation_type = other.orientation_type;
  has_unlocked_orientation_lock = other.has_unlocked_orientation_lock;
  is_expecting_fullscreen_rotation = other.is_expecting_fullscreen_rotation;
  is_fullscreen = other.is_fullscreen;
  is_picture_in_picture = other.is_picture_in_picture;
  on_physical_backing_changed_received =
      other.on_physical_backing_changed_received;
  on_sync_display_properties_changed_received =
      other.on_sync_display_properties_changed_received;
  any_non_rotation_size_changed = other.any_non_rotation_size_changed;
  if (other.local_surface_id.is_valid())
    local_surface_id = other.local_surface_id;
}

bool ScreenState::EqualOrientations(const ScreenState& other) {
  return !IsRotation(visible_viewport_size, other.visible_viewport_size) &&
         !IsRotation(physical_backing_size, other.physical_backing_size) &&
         !ExpectsResizeForOrientationChange(orientation_type,
                                            other.orientation_type);
}

bool ScreenState::IsRotated(const ScreenState& other) {
  return IsRotation(visible_viewport_size, other.visible_viewport_size) &&
         IsRotation(physical_backing_size, other.physical_backing_size) &&
         ExpectsResizeForOrientationChange(orientation_type,
                                           other.orientation_type);
}

bool ScreenState::IsValid() {
  return !visible_viewport_size.IsEmpty() && !physical_backing_size.IsEmpty() &&
         !screen_info_size.IsEmpty() &&
         orientation_type != display::mojom::ScreenOrientation::kUndefined;
}

bool ScreenState::EqualVisualProperties(const ScreenState& other) const {
  return visible_viewport_size == other.visible_viewport_size &&
         physical_backing_size == other.physical_backing_size &&
         screen_info_size == other.screen_info_size &&
         orientation_type == other.orientation_type &&
         is_fullscreen == other.is_fullscreen;
}

// static
bool ScreenState::IsRotation(const gfx::Size& old_size,
                             const gfx::Size& new_size) {
  // The size change can sometimes include both the rotation and top-controls
  // adjustments at the same time. So we can't rely on it being a direct swap.
  if (old_size.width() > old_size.height() &&
      new_size.width() < new_size.height()) {
    return true;
  } else if (old_size.width() < old_size.height() &&
             new_size.width() > new_size.height()) {
    return true;
  }
  return false;
}

// static
bool ScreenState::IsSingleAxisResize(const gfx::Size& old_size,
                                     const gfx::Size& new_size) {
  if (old_size.width() == new_size.width() &&
      old_size.height() != new_size.height()) {
    return true;
  }
  if (old_size.height() == new_size.height() &&
      old_size.width() != new_size.width()) {
    return true;
  }
  return false;
}

// static
bool ScreenState::ExpectsResizeForOrientationChange(
    display::mojom::ScreenOrientation current,
    display::mojom::ScreenOrientation pending) {
  switch (current) {
    case display::mojom::ScreenOrientation::kUndefined:
      return false;
    case display::mojom::ScreenOrientation::kPortraitPrimary:
    case display::mojom::ScreenOrientation::kPortraitSecondary:
      return pending == display::mojom::ScreenOrientation::kLandscapePrimary ||
             pending == display::mojom::ScreenOrientation::kLandscapeSecondary;
    case display::mojom::ScreenOrientation::kLandscapePrimary:
    case display::mojom::ScreenOrientation::kLandscapeSecondary:
      return pending == display::mojom::ScreenOrientation::kPortraitPrimary ||
             pending == display::mojom::ScreenOrientation::kPortraitSecondary;
  }
  return false;
}

}  // namespace content
