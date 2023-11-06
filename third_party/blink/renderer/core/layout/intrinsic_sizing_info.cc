/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"

namespace blink {

// https://www.w3.org/TR/css3-images/#default-sizing
gfx::SizeF ConcreteObjectSize(const IntrinsicSizingInfo& sizing_info,
                              const gfx::SizeF& default_object_size) {
  if (sizing_info.has_width && sizing_info.has_height) {
    return sizing_info.size;
  }

  if (sizing_info.has_width) {
    if (sizing_info.aspect_ratio.IsEmpty()) {
      return gfx::SizeF(sizing_info.size.width(), default_object_size.height());
    }
    return gfx::SizeF(sizing_info.size.width(),
                      ResolveHeightForRatio(sizing_info.size.width(),
                                            sizing_info.aspect_ratio));
  }

  if (sizing_info.has_height) {
    if (sizing_info.aspect_ratio.IsEmpty()) {
      return gfx::SizeF(default_object_size.width(), sizing_info.size.height());
    }
    return gfx::SizeF(ResolveWidthForRatio(sizing_info.size.height(),
                                           sizing_info.aspect_ratio),
                      sizing_info.size.height());
  }

  if (!sizing_info.aspect_ratio.IsEmpty()) {
    // "A contain constraint is resolved by setting the concrete object size to
    //  the largest rectangle that has the object's intrinsic aspect ratio and
    //  additionally has neither width nor height larger than the constraint
    //  rectangle's width and height, respectively."
    float solution_width = ResolveWidthForRatio(default_object_size.height(),
                                                sizing_info.aspect_ratio);
    if (solution_width <= default_object_size.width()) {
      return gfx::SizeF(solution_width, default_object_size.height());
    }

    float solution_height = ResolveHeightForRatio(default_object_size.width(),
                                                  sizing_info.aspect_ratio);
    return gfx::SizeF(default_object_size.width(), solution_height);
  }

  return default_object_size;
}

}  // namespace blink
