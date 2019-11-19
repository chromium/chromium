// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/display_util.h"

#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/common/screen_info.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/icc_profile.h"

namespace content {

// static
void DisplayUtil::DisplayToScreenInfo(ScreenInfo* screen_info,
                                      const display::Display& display) {
  screen_info->rect = display.bounds();
  // TODO(husky): Remove any Android system controls from availableRect.
  screen_info->available_rect = display.work_area();
  screen_info->device_scale_factor = display.device_scale_factor();
  screen_info->color_space = display.color_space();
  screen_info->depth = display.color_depth();
  screen_info->depth_per_component = display.depth_per_component();
  screen_info->is_monochrome = display.is_monochrome();
  screen_info->display_frequency = display.display_frequency();

  screen_info->orientation_angle = display.RotationAsDegree();
#if defined(USE_AURA)
  // The Display rotation and the ScreenInfo orientation are not the same
  // angle. The former is the physical display rotation while the later is the
  // rotation required by the content to be shown properly on the screen, in
  // other words, relative to the physical display.
  // TODO(ccameron): Should this apply to macOS? Should this be reconciled at a
  // higher level (say, in conversion to WebScreenInfo)?
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
}

// static
void DisplayUtil::GetDefaultScreenInfo(ScreenInfo* screen_info) {
  // Some tests are run with no Screen initialized.
  display::Screen* screen = display::Screen::GetScreen();
  if (!screen) {
    *screen_info = ScreenInfo();
    return;
  }
#if defined(USE_AURA)
  // This behavior difference between Aura and other platforms may or may not
  // be intentional, and may or may not have any effect.
  gfx::NativeView null_native_view = nullptr;
  display::Display display = screen->GetDisplayNearestView(null_native_view);
#else
  display::Display display = screen->GetPrimaryDisplay();
#endif
  DisplayToScreenInfo(screen_info, display);
}

// static
void DisplayUtil::GetNativeViewScreenInfo(ScreenInfo* screen_info,
                                          gfx::NativeView native_view) {
  // Some tests are run with no Screen initialized.
  display::Screen* screen = display::Screen::GetScreen();
  if (!screen) {
    *screen_info = ScreenInfo();
    return;
  }
  display::Display display = native_view
                                 ? screen->GetDisplayNearestView(native_view)
                                 : screen->GetPrimaryDisplay();
  DisplayToScreenInfo(screen_info, display);
}

// static
ScreenOrientationValues DisplayUtil::GetOrientationTypeForMobile(
    const display::Display& display) {
  int angle = display.RotationAsDegree();
  const gfx::Rect& bounds = display.bounds();

  // Whether the device's natural orientation is portrait.
  bool natural_portrait = false;
  if (angle == 0 || angle == 180)  // The device is in its natural orientation.
    natural_portrait = bounds.height() >= bounds.width();
  else
    natural_portrait = bounds.height() <= bounds.width();

  switch (angle) {
    case 0:
      return natural_portrait ? SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY
                              : SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY;
    case 90:
      return natural_portrait ? SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY
                              : SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY;
    case 180:
      return natural_portrait ? SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY
                              : SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY;
    case 270:
      return natural_portrait ? SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY
                              : SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY;
    default:
      NOTREACHED();
      return SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY;
  }
}

// static
ScreenOrientationValues DisplayUtil::GetOrientationTypeForDesktop(
    const display::Display& display) {
  static int primary_landscape_angle = -1;
  static int primary_portrait_angle = -1;

  int angle = display.RotationAsDegree();
  const gfx::Rect& bounds = display.bounds();
  bool is_portrait = bounds.height() >= bounds.width();

  if (is_portrait && primary_portrait_angle == -1)
    primary_portrait_angle = angle;

  if (!is_portrait && primary_landscape_angle == -1)
    primary_landscape_angle = angle;

  if (is_portrait) {
    return primary_portrait_angle == angle
               ? SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY
               : SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY;
  }

  return primary_landscape_angle == angle
             ? SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY
             : SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY;
}

}  // namespace content
