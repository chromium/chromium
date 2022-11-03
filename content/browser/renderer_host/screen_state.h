// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SCREEN_STATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_SCREEN_STATE_H_

#include "components/viz/common/surfaces/local_surface_id.h"
#include "ui/display/mojom/screen_orientation.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace content {

// Contains a subset of blink::VisualProperties that each contribute to define
// the state of the screen. Each is updated through different paths/timings.
// This class is used to determine once all of them are in sync so that we can
// begin SurfaceSync with the Renderer.
class ScreenState {
 public:
  ScreenState();
  ~ScreenState() = default;

  // Visual properties of the screen.
  gfx::Size visible_viewport_size;
  gfx::Size physical_backing_size;
  gfx::Size screen_info_size;
  display::mojom::ScreenOrientation orientation_type =
      display::mojom::ScreenOrientation::kUndefined;
  bool is_fullscreen = false;

  // True when we have unlocked the orientation, which may occur in the middle
  // of a rotation.
  bool has_unlocked_orientation_lock = false;
  // True when we have been locked to an orientation that requires a rotation.
  bool is_expecting_fullscreen_rotation = false;
  // True when we are visual properties for Picture-in-Picture. Not used in
  // comparisons as we want to identify before/after equality of the visual
  // properties.
  bool is_picture_in_picture = false;
  // Once we've processed the first update we do not look at these propertiies
  // on subsequent updates. As rapid changes to `visible_viewport_size` can
  // cause re-processing.
  bool on_physical_backing_changed_received = false;
  bool on_sync_display_properties_changed_received = false;
  // When entering fullscreen we throttle until there is any non-rotation
  // update.
  bool any_non_rotation_size_changed = false;
  // The id allocated after we have synced the above visual properties.
  viz::LocalSurfaceId local_surface_id;

  // Copies all values that are `valid` as defined by their class.
  void CopyDefinedAttributes(const ScreenState& other);

  // Returns true if each of `visible_viewport_size`, `physical_backing_size`,
  // and `orientation_type` have matching orientations to `other`.
  bool EqualOrientations(const ScreenState& other);

  // Returns true if each of `visible_viewport_size`, `physical_backing_size`,
  // and `orientation_type` have are rotations to `other`.
  bool IsRotated(const ScreenState& other);

  // Returns true if each of `visible_viewport_size`, `physical_backing_size`,
  // and `orientation_type` are valid.
  bool IsValid();

  // We only want to compare the equality of the visual properties, not the
  // transitional book keeping.
  bool EqualVisualProperties(const ScreenState& other) const;

  // Determines the orientation of the two different sizes, then returns true if
  // they are a rotation of each other.
  static bool IsRotation(const gfx::Size& old_size, const gfx::Size& new_size);

  // Only for use with Sizes with the same scale factor. Such as comparing
  // changes between two Physical Backings. Returns true if there was a resize
  // along one of the axis.
  static bool IsSingleAxisResize(const gfx::Size& old_size,
                                 const gfx::Size& new_size);

  // Compares the orientations to determine if a resize if expected. For example
  // going from Landscape to Portrait. Whereas a Portrait-Primary to
  // Portrait-Secondary would not expect any resize for the orientation change.
  static bool ExpectsResizeForOrientationChange(
      display::mojom::ScreenOrientation current,
      display::mojom::ScreenOrientation pending);
};
}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SCREEN_STATE_H_
