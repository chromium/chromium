// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/display_util.h"

#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/icc_profile.h"

namespace content {

// static
void DisplayUtil::DisplayToScreenInfo(blink::ScreenInfo* screen_info,
                                      const display::Display& display) {
  screen_info->rect = display.bounds();
  // TODO(husky): Remove any Android system controls from availableRect.
  screen_info->available_rect = display.work_area();
  screen_info->device_scale_factor = display.device_scale_factor();
  screen_info->display_color_spaces = display.color_spaces();
  screen_info->depth = display.color_depth();
  screen_info->depth_per_component = display.depth_per_component();
  screen_info->is_monochrome = display.is_monochrome();
  screen_info->display_frequency = display.display_frequency();

  // TODO(https://crbug.com/998131): Expose panel orientation via a proper web
  // API instead of window.screen.orientation.angle.
  screen_info->orientation_angle = display.PanelRotationAsDegree();
#if defined(USE_AURA)
  // The Display rotation and the ScreenInfo orientation are not the same
  // angle. The former is the physical display rotation while the later is the
  // rotation required by the content to be shown properly on the screen, in
  // other words, relative to the physical display.
  // Spec: https://w3c.github.io/screen-orientation/#dom-screenorientation-angle
  // TODO(ccameron): Should this apply to macOS? Should this be reconciled at a
  // higher level (say, in conversion to ScreenInfo)?
  if (screen_info->orientation_angle == 90)
    screen_info->orientation_angle = 270;
  else if (screen_info->orientation_angle == 270)
    screen_info->orientation_angle = 90;
#endif

#if defined(OS_ANDROID)
  screen_info->orientation_type = GetOrientationTypeForMobile(display);
#else
  screen_info->orientation_type = GetOrientationTypeForDesktop(display);
#endif

  auto* screen = display::Screen::GetScreen();
  // Some tests are run with no Screen initialized.
  screen_info->is_extended = screen && screen->GetNumDisplays() > 1;
}

// static
void DisplayUtil::GetDefaultScreenInfo(blink::ScreenInfo* screen_info) {
  return GetNativeViewScreenInfo(screen_info, nullptr);
}

// static
void DisplayUtil::GetNativeViewScreenInfo(blink::ScreenInfo* screen_info,
                                          gfx::NativeView native_view) {
  // Some tests are run with no Screen initialized.
  display::Screen* screen = display::Screen::GetScreen();
  if (!screen) {
    *screen_info = blink::ScreenInfo();
    return;
  }
  display::Display display = native_view
                                 ? screen->GetDisplayNearestView(native_view)
                                 : screen->GetPrimaryDisplay();
  DisplayToScreenInfo(screen_info, display);
}

// static
blink::mojom::ScreenOrientation DisplayUtil::GetOrientationTypeForMobile(
    const display::Display& display) {
  int angle = display.PanelRotationAsDegree();
  const gfx::Rect& bounds = display.bounds();

  // Whether the device's natural orientation is portrait.
  bool natural_portrait = false;
  if (angle == 0 || angle == 180)  // The device is in its natural orientation.
    natural_portrait = bounds.height() >= bounds.width();
  else
    natural_portrait = bounds.height() <= bounds.width();

  switch (angle) {
    case 0:
      return natural_portrait
                 ? blink::mojom::ScreenOrientation::kPortraitPrimary
                 : blink::mojom::ScreenOrientation::kLandscapePrimary;
    case 90:
      return natural_portrait
                 ? blink::mojom::ScreenOrientation::kLandscapePrimary
                 : blink::mojom::ScreenOrientation::kPortraitSecondary;
    case 180:
      return natural_portrait
                 ? blink::mojom::ScreenOrientation::kPortraitSecondary
                 : blink::mojom::ScreenOrientation::kLandscapeSecondary;
    case 270:
      return natural_portrait
                 ? blink::mojom::ScreenOrientation::kLandscapeSecondary
                 : blink::mojom::ScreenOrientation::kPortraitPrimary;
    default:
      NOTREACHED();
      return blink::mojom::ScreenOrientation::kPortraitPrimary;
  }
}

// static
blink::mojom::ScreenOrientation DisplayUtil::GetOrientationTypeForDesktop(
    const display::Display& display) {
  static int primary_landscape_angle = -1;
  static int primary_portrait_angle = -1;

  int angle = display.PanelRotationAsDegree();
  const gfx::Rect& bounds = display.bounds();
  bool is_portrait = bounds.height() >= bounds.width();

  if (is_portrait && primary_portrait_angle == -1)
    primary_portrait_angle = angle;

  if (!is_portrait && primary_landscape_angle == -1)
    primary_landscape_angle = angle;

  if (is_portrait) {
    return primary_portrait_angle == angle
               ? blink::mojom::ScreenOrientation::kPortraitPrimary
               : blink::mojom::ScreenOrientation::kPortraitSecondary;
  }

  return primary_landscape_angle == angle
             ? blink::mojom::ScreenOrientation::kLandscapePrimary
             : blink::mojom::ScreenOrientation::kLandscapeSecondary;
}

}  // namespace content
