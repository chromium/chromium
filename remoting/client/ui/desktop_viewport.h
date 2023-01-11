// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_UI_DESKTOP_VIEWPORT_H_
#define REMOTING_CLIENT_UI_DESKTOP_VIEWPORT_H_

#include "base/functional/callback.h"
#include "remoting/client/ui/view_matrix.h"

namespace remoting {

// The viewport is a sliding window on the desktop image. This class defines
// the viewport's position and size, provides methods to manipulate it, and
// outputs a desktop space -> surface space transformation matrix for drawing
// the desktop onto the client's view surface on the screen.
//
// Desktop space: Coordinates to locate a point on the host's desktop image in
//     pixels, similar to the mouse's cursor position.
// Surface space: Coordinates to locate a point on the client device's view
//     surface (e.g. GLKView/SurfaceView) for rendering the desktop, this is
//     often the coordinate system to locate a point on the user's phone
//     screen (if the surface expands to the whole screen).
//
// Viewport: Projection of the user surface view on the desktop space.
// Desktop: Projection of the host's desktop on the surface space.
//
// You may either manipulate the desktop on the surface coordinate or manipulate
// the viewport on the desktop coordinate, depending on your choice of the
// reference frame.
class DesktopViewport {
 public:
  using TransformationCallback =
      base::RepeatingCallback<void(const ViewMatrix&)>;

  DesktopViewport();
  ~DesktopViewport();

  // Sets the |desktop_size_| and (re)initializes the viewport.
  void SetDesktopSize(int desktop_width, int desktop_height);

  // Sets the |surface_size_| and (re)initializes the viewport.
  void SetSurfaceSize(int surface_width, int surface_height);

  // Sets insets on the surface area to allow viewport to be panned out of them.
  // Should be used to adjust for system UI like soft keyboard and screen
  // notches/cutouts.
  // This method effectively shrinks the size of the viewport on the surface.
  // You may want to call this before SetSurfaceSize() so that safe insets are
  // taken into account when initializing viewport.
  void SetSafeInsets(int left, int top, int right, int bottom);

  // Translates the desktop on the surface's reference frame by <dx, dy>.
  void MoveDesktop(float dx, float dy);

  // Scales the desktop on the surface's reference frame at pivot point (px, py)
  // by |scale|.
  void ScaleDesktop(float px, float py, float scale);

  // Moves the viewport center by <x, y> on the desktop's coordinate.
  void MoveViewport(float dx, float dy);

  // Sets the viewport center to (x, y) on the desktop's coordinate.
  void SetViewportCenter(float x, float y);

  // Returns the current center of the viewport on the desktop's coordinate.
  ViewMatrix::Point GetViewportCenter() const;

  // Returns true if |point| is within the bounds of the desktop.
  bool IsPointWithinDesktopBounds(const ViewMatrix::Point& point) const;

  // True if desktop size and surface size are set.
  bool IsViewportReady() const;

  // Constrains |point| within the bounds of the desktop. Do nothing if the
  // desktop size is not set.
  ViewMatrix::Point ConstrainPointToDesktop(
      const ViewMatrix::Point& point) const;

  // Registers the callback to be called once the transformation has changed.
  // run_immediately: If true and the viewport is ready to be used, the callback
  // will be called immediately with the transformation matrix.
  void RegisterOnTransformationChangedCallback(
      const TransformationCallback& callback,
      bool run_immediately);

  // Returns the reference to the desktop-to-surface transformation.
  const ViewMatrix& GetTransformation() const;

 private:
  struct Bounds {
    float left;
    float top;
    float right;
    float bottom;
  };

  // Resizes the desktop such that the image is displayed without borders in
  // minimum possible zoom-level. This will be called once both the desktop
  // and the surface size are set.
  void ResizeToFit();

  // Adjusts the size and position of the viewport so that the constrains always
  // hold, then feed the matrix to |on_transformation_changed_|.
  void UpdateViewport();

  // Gets a rectangle of all possible positions where the viewport's center can
  // locate.
  Bounds GetViewportCenterBounds() const;

  // Gets the size of |surface_size_| inset by |safe_insets_|.
  ViewMatrix::Vector2D GetSurfaceSafeAreaSize() const;

  // Translates the viewport on the desktop's reference frame by <dx, dy>,
  // without calling UpdateViewport().
  void MoveViewportWithoutUpdate(float dx, float dy);

  // Moves the point inside the bounds with minimum displacement if it is out of
  // the bounds.
  static ViewMatrix::Point ConstrainPointToBounds(
      const Bounds& bounds,
      const ViewMatrix::Point& point);

  ViewMatrix::Vector2D desktop_size_{0.f, 0.f};
  ViewMatrix::Vector2D surface_size_{0.f, 0.f};
  Bounds safe_insets_{0, 0, 0, 0};

  ViewMatrix desktop_to_surface_transform_;

  TransformationCallback on_transformation_changed_;

  // DesktopViewport is neither copyable nor movable.
  DesktopViewport(const DesktopViewport&) = delete;
  DesktopViewport& operator=(const DesktopViewport&) = delete;
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_UI_DESKTOP_VIEWPORT_H_
