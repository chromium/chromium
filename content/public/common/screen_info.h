// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SCREEN_INFO_H_
#define CONTENT_PUBLIC_COMMON_SCREEN_INFO_H_

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/screen_orientation_values.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/icc_profile.h"

namespace content {

// Information about the screen on which a RenderWidget is being displayed. This
// is the content counterpart to WebScreenInfo in blink.
struct CONTENT_EXPORT ScreenInfo {
    ScreenInfo();
    ScreenInfo(const ScreenInfo& other);
    ~ScreenInfo();

    // Device scale factor. Specifies the ratio between physical and logical
    // pixels.
    float device_scale_factor = 1.f;

    // The color space of the output display.
    gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();

    // The screen depth in bits per pixel
    uint32_t depth = 0;

    // The bits per colour component. This assumes that the colours are balanced
    // equally.
    uint32_t depth_per_component = 0;

    // This can be true for black and white printers
    bool is_monochrome = false;

    // The display frequency in Hz of the monitor. Set to 0 if it fails in the
    // monitor frequency query.
    int display_frequency = 0;

    // The display monitor rectangle in virtual-screen coordinates. Note that
    // this may be negative.
    gfx::Rect rect;

    // The portion of the monitor's rectangle that can be used by applications.
    gfx::Rect available_rect;

    // The monitor's orientation.
    ScreenOrientationValues orientation_type =
        SCREEN_ORIENTATION_VALUES_DEFAULT;

    // This is the orientation angle of the displayed content in degrees.
    // It is the opposite of the physical rotation.
    uint16_t orientation_angle = 0;

    bool operator==(const ScreenInfo& other) const;
    bool operator!=(const ScreenInfo& other) const;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SCREEN_INFO_H_
