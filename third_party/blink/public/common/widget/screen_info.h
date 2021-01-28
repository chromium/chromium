/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_WIDGET_SCREEN_INFO_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_WIDGET_SCREEN_INFO_H_

#include "third_party/blink/public/mojom/widget/screen_orientation.mojom-shared.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

struct ScreenInfo {
  // Device scale factor. Specifies the ratio between physical and logical
  // pixels.
  float device_scale_factor = 1.f;

  // The color spaces used by output display for various content types.
  gfx::DisplayColorSpaces display_color_spaces;

  // The screen depth in bits per pixel.
  int depth = 0;

  // The bits per colour component. This assumes that the colours are balanced
  // equally.
  int depth_per_component = 0;

  // This can be true for black and white printers
  bool is_monochrome = false;

  // The display frequency in Hz of the monitor. Set to 0 if it fails in the
  // monitor frequency query.
  int display_frequency = 0;

  // This is set from the rcMonitor member of MONITORINFOEX, to whit:
  //   "A RECT structure that specifies the display monitor rectangle,
  //   expressed in virtual-screen coordinates. Note that if the monitor
  //   is not the primary display monitor, some of the rectangle's
  //   coordinates may be negative values."
  gfx::Rect rect;

  // This is set from the rcWork member of MONITORINFOEX, to whit:
  //   "A RECT structure that specifies the work area rectangle of the
  //   display monitor that can be used by applications, expressed in
  //   virtual-screen coordinates. Windows uses this rectangle to
  //   maximize an application on the monitor. The rest of the area in
  //   rcMonitor contains system windows such as the task bar and side
  //   bars. Note that if the monitor is not the primary display monitor,
  //   some of the rectangle's coordinates may be negative values".
  gfx::Rect available_rect;

  // This is the orientation 'type' or 'name', as in landscape-primary or
  // portrait-secondary for examples.
  // See public/mojom/screen_orientation.mojom for the full list.
  mojom::ScreenOrientation orientation_type =
      mojom::ScreenOrientation::kUndefined;

  // This is the orientation angle of the displayed content in degrees.
  // It is the opposite of the physical rotation.
  // TODO(crbug.com/840189): we should use an enum rather than a number here.
  uint16_t orientation_angle = 0;

  // Proposed: https://github.com/webscreens/window-placement
  // Whether this Screen is part of a multi-screen extended visual workspace.
  bool is_extended = false;

  ScreenInfo() = default;

  bool operator==(const ScreenInfo& other) const {
    return this->device_scale_factor == other.device_scale_factor &&
           this->display_color_spaces == other.display_color_spaces &&
           this->depth == other.depth &&
           this->depth_per_component == other.depth_per_component &&
           this->is_monochrome == other.is_monochrome &&
           this->display_frequency == other.display_frequency &&
           this->rect == other.rect &&
           this->available_rect == other.available_rect &&
           this->orientation_type == other.orientation_type &&
           this->orientation_angle == other.orientation_angle &&
           this->is_extended == other.is_extended;
  }

  bool operator!=(const ScreenInfo& other) const {
    return !this->operator==(other);
  }
};

}  // namespace blink

#endif
