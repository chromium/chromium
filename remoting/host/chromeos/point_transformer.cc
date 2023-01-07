// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/point_transformer.h"

#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace remoting {

namespace {

gfx::PointF ConvertScreenDIPToWindowDIP(const aura::Window* window,
                                        gfx::PointF location_in_screen_in_dip) {
  gfx::PointF result(location_in_screen_in_dip);
  wm::ConvertPointFromScreen(window, &result);
  return result;
}

gfx::PointF ConvertWindowDIPToScreenDIP(const aura::Window* window,
                                        gfx::PointF location_in_window_in_dip) {
  gfx::PointF result(location_in_window_in_dip);
  wm::ConvertPointToScreen(window, &result);
  return result;
}

gfx::PointF ConvertWindowDIPtoWindowPixels(
    const aura::Window* window,
    gfx::PointF location_in_window_in_dip) {
  gfx::PointF result(location_in_window_in_dip);
  window->GetHost()->ConvertDIPToPixels(&result);
  return result;
}

gfx::PointF ConvertWindowPixelsToWindowDIP(
    const aura::Window* window,
    gfx::PointF location_in_window_in_pixels) {
  gfx::PointF result(location_in_window_in_pixels);
  window->GetHost()->ConvertPixelsToDIP(&result);
  return result;
}

gfx::PointF ConvertWindowPixelsToScreenPixels(
    const aura::Window* window,
    gfx::PointF location_in_window_in_pixels) {
  return location_in_window_in_pixels +
         window->GetHost()->GetBoundsInPixels().OffsetFromOrigin();
}

}  // namespace

// static
gfx::PointF PointTransformer::ConvertScreenInDipToScreenInPixel(
    gfx::PointF location_in_screen_in_dip) {
  const aura::Window* window = ash::window_util::GetRootWindowAt(
      gfx::ToRoundedPoint(location_in_screen_in_dip));

  gfx::PointF location_in_window_in_dip =
      ConvertScreenDIPToWindowDIP(window, location_in_screen_in_dip);

  gfx::PointF location_in_window_in_pixels =
      ConvertWindowDIPtoWindowPixels(window, location_in_window_in_dip);

  gfx::PointF location_in_screen_in_pixel =
      ConvertWindowPixelsToScreenPixels(window, location_in_window_in_pixels);

  return location_in_screen_in_pixel;
}

// static
gfx::PointF PointTransformer::ConvertWindowInPixelToScreenInDip(
    const aura::Window* window,
    gfx::PointF location_in_window_in_pixels) {
  gfx::PointF location_in_window_in_dip =
      ConvertWindowPixelsToWindowDIP(window, location_in_window_in_pixels);

  gfx::PointF location_in_screen_in_dip =
      ConvertWindowDIPToScreenDIP(window, location_in_window_in_dip);

  return location_in_screen_in_dip;
}

}  // namespace remoting
