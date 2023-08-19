// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_details/screen_detailed.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_statics.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/display/screen_info.h"
#include "ui/display/screen_infos.h"

namespace blink {

ScreenDetailed::ScreenDetailed(LocalDOMWindow* window, int64_t display_id)
    : Screen(window, display_id) {}

// static
bool ScreenDetailed::AreWebExposedScreenDetailedPropertiesEqual(
    const display::ScreenInfo& prev,
    const display::ScreenInfo& current) {
  if (!Screen::AreWebExposedScreenPropertiesEqual(prev, current)) {
    return false;
  }

  // left() / top()
  if (prev.rect.origin() != current.rect.origin())
    return false;

  // isPrimary()
  if (prev.is_primary != current.is_primary)
    return false;

  // isInternal()
  if (prev.is_internal != current.is_internal)
    return false;

  // label()
  if (prev.label != current.label)
    return false;

  if (RuntimeEnabledFeatures::CanvasHDREnabled()) {
    // highDynamicRangeHeadroom()
    if (prev.display_color_spaces.GetHDRMaxLuminanceRelative() !=
        current.display_color_spaces.GetHDRMaxLuminanceRelative()) {
      return false;
    }

    const auto prev_primaries = prev.display_color_spaces.GetPrimaries();
    const auto curr_primaries = current.display_color_spaces.GetPrimaries();

    // redPrimaryX()
    if (prev_primaries.fRX != curr_primaries.fRX)
      return false;

    // redPrimaryY()
    if (prev_primaries.fRY != curr_primaries.fRY)
      return false;

    // greenPrimaryX()
    if (prev_primaries.fGX != curr_primaries.fGX)
      return false;

    // greenPrimaryY()
    if (prev_primaries.fGY != curr_primaries.fGY)
      return false;

    // bluePrimaryX()
    if (prev_primaries.fBX != curr_primaries.fBX)
      return false;

    // bluePrimaryY()
    if (prev_primaries.fBY != curr_primaries.fBY)
      return false;

    // whitePointX()
    if (prev_primaries.fWX != curr_primaries.fWX)
      return false;

    // whitePointY()
    if (prev_primaries.fWY != curr_primaries.fWY)
      return false;
  }

  // Note: devicePixelRatio() covered by Screen base function

  return true;
}

int ScreenDetailed::left() const {
  if (!DomWindow())
    return 0;
  return GetRect(/*available=*/false).x();
}

int ScreenDetailed::top() const {
  if (!DomWindow())
    return 0;
  return GetRect(/*available=*/false).y();
}

bool ScreenDetailed::isPrimary() const {
  if (!DomWindow())
    return false;
  return GetScreenInfo().is_primary;
}

bool ScreenDetailed::isInternal() const {
  if (!DomWindow())
    return false;
  return GetScreenInfo().is_internal;
}

float ScreenDetailed::devicePixelRatio() const {
  if (!DomWindow())
    return 0.f;
  return GetScreenInfo().device_scale_factor;
}

String ScreenDetailed::label() const {
  if (!DomWindow())
    return String();
  return String::FromUTF8(GetScreenInfo().label);
}

float ScreenDetailed::highDynamicRangeHeadroom() const {
  return GetScreenInfo().display_color_spaces.GetHDRMaxLuminanceRelative();
}

float ScreenDetailed::redPrimaryX() const {
  return GetScreenInfo().display_color_spaces.GetPrimaries().fRX;
}

float ScreenDetailed::redPrimaryY() const {
  return GetScreenInfo().display_color_spaces.GetPrimaries().fRY;
}

float ScreenDetailed::greenPrimaryX() const {
  return GetScreenInfo().display_color_spaces.GetPrimaries().fGX;
}

float ScreenDetailed::greenPrimaryY() const {
  return GetScreenInfo().display_color_spaces.GetPrimaries().fGY;
}

float ScreenDetailed::bluePrimaryX() const {
  return GetScreenInfo().display_color_spaces.GetPrimaries().fBX;
}

float ScreenDetailed::bluePrimaryY() const {
  return GetScreenInfo().display_color_spaces.GetPrimaries().fBY;
}

float ScreenDetailed::whitePointX() const {
  return GetScreenInfo().display_color_spaces.GetPrimaries().fWX;
}

float ScreenDetailed::whitePointY() const {
  return GetScreenInfo().display_color_spaces.GetPrimaries().fWY;
}

}  // namespace blink
