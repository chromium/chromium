/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COLOR_BLEND_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COLOR_BLEND_H_

#include "third_party/blink/renderer/platform/geometry/blend.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

// TODO(https://crbug.com/1351544): This function will need to operate on
// all possible color parameterizations.
inline Color Blend(const Color& from,
                   const Color& to,
                   double progress,
                   bool blend_premultiplied = true) {
  if (blend_premultiplied) {
    // Contrary to the name, RGBA32 actually stores ARGB, so we can initialize
    // Color directly from premultipliedARGBFromColor(). Also,
    // premultipliedARGBFromColor() bails on zero alpha, so special-case that.
    Color premult_from =
        Color::FromRGBA32(from.Alpha() ? PremultipliedARGBFromColor(from) : 0);
    Color premult_to =
        Color::FromRGBA32(to.Alpha() ? PremultipliedARGBFromColor(to) : 0);

    Color premult_blended(
        Blend(premult_from.Red(), premult_to.Red(), progress),
        Blend(premult_from.Green(), premult_to.Green(), progress),
        Blend(premult_from.Blue(), premult_to.Blue(), progress),
        Blend(premult_from.Alpha(), premult_to.Alpha(), progress));

    return Color(ColorFromPremultipliedARGB(premult_blended.Rgb()));
  }

  return Color(Blend(from.Red(), to.Red(), progress),
               Blend(from.Green(), to.Green(), progress),
               Blend(from.Blue(), to.Blue(), progress),
               Blend(from.Alpha(), to.Alpha(), progress));
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COLOR_BLEND_H_
