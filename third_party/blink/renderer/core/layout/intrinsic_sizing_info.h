// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INTRINSIC_SIZING_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INTRINSIC_SIZING_INFO_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

struct IntrinsicSizingInfo {
  DISALLOW_NEW();

  static IntrinsicSizingInfo None() {
    return {gfx::SizeF(), gfx::SizeF(), false, false};
  }

  // Because they are using float instead of LayoutUnit, we can't use
  // PhysicalSize here.
  gfx::SizeF size;
  gfx::SizeF aspect_ratio;
  bool has_width = true;
  bool has_height = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INTRINSIC_SIZING_INFO_H_
