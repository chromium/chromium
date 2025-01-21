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

#include "third_party/blink/renderer/core/layout/natural_sizing_info.h"

namespace blink {

namespace {

template <typename SizingInfo>
struct SizingTraits {};

template <>
struct SizingTraits<NaturalSizingInfo> {
  using SizeType = gfx::SizeF;

  static float GetWidth(const gfx::SizeF& size) { return size.width(); }
  static float GetHeight(const gfx::SizeF& size) { return size.height(); }
};

template <>
struct SizingTraits<PhysicalNaturalSizingInfo> {
  using SizeType = PhysicalSize;

  static LayoutUnit GetWidth(const PhysicalSize& size) { return size.width; }
  static LayoutUnit GetHeight(const PhysicalSize& size) { return size.height; }
};

// https://www.w3.org/TR/css3-images/#default-sizing
template <typename SizingInfo,
          typename SizeType = SizingTraits<SizingInfo>::SizeType>
SizeType ConcreteObjectSizeImpl(const SizingInfo& sizing_info,
                                const SizeType& default_object_size) {
  if (sizing_info.has_width && sizing_info.has_height) {
    return sizing_info.size;
  }

  using Traits = SizingTraits<SizingInfo>;
  if (sizing_info.has_width) {
    auto width = Traits::GetWidth(sizing_info.size);
    if (sizing_info.aspect_ratio.IsEmpty()) {
      return SizeType(width, Traits::GetHeight(default_object_size));
    }
    return SizeType(width,
                    ResolveHeightForRatio(width, sizing_info.aspect_ratio));
  }

  if (sizing_info.has_height) {
    auto height = Traits::GetHeight(sizing_info.size);
    if (sizing_info.aspect_ratio.IsEmpty()) {
      return SizeType(Traits::GetWidth(default_object_size), height);
    }
    return SizeType(ResolveWidthForRatio(height, sizing_info.aspect_ratio),
                    height);
  }

  if (!sizing_info.aspect_ratio.IsEmpty()) {
    // "A contain constraint is resolved by setting the concrete object size to
    //  the largest rectangle that has the object's intrinsic aspect ratio and
    //  additionally has neither width nor height larger than the constraint
    //  rectangle's width and height, respectively."
    auto default_width = Traits::GetWidth(default_object_size);
    auto default_height = Traits::GetHeight(default_object_size);
    auto solution_width =
        ResolveWidthForRatio(default_height, sizing_info.aspect_ratio);
    if (solution_width <= default_width) {
      return SizeType(solution_width, default_height);
    }

    auto solution_height =
        ResolveHeightForRatio(default_width, sizing_info.aspect_ratio);
    return SizeType(default_width, solution_height);
  }
  return default_object_size;
}

}  // namespace

// static
PhysicalNaturalSizingInfo PhysicalNaturalSizingInfo::FromSizingInfo(
    const NaturalSizingInfo& sizing_info) {
  return {PhysicalSize::FromSizeFRound(sizing_info.size),
          LayoutRatioFromSizeF(sizing_info.aspect_ratio), sizing_info.has_width,
          sizing_info.has_height};
}

gfx::SizeF ConcreteObjectSize(const NaturalSizingInfo& sizing_info,
                              const gfx::SizeF& default_object_size) {
  return ConcreteObjectSizeImpl(sizing_info, default_object_size);
}

PhysicalSize ConcreteObjectSize(const PhysicalNaturalSizingInfo& sizing_info,
                                const PhysicalSize& default_object_size) {
  return ConcreteObjectSizeImpl(sizing_info, default_object_size);
}

}  // namespace blink
