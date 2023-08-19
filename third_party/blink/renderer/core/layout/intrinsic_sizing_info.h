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

  IntrinsicSizingInfo() : has_width(true), has_height(true) {}

  // Because they are using float instead of LayoutUnit, we can't use
  // PhysicalSize here.
  gfx::SizeF size;
  gfx::SizeF aspect_ratio;
  bool has_width;
  bool has_height;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INTRINSIC_SIZING_INFO_H_
