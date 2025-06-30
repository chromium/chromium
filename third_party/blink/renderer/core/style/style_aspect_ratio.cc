// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_aspect_ratio.h"

#include "third_party/blink/renderer/platform/geometry/physical_size.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

StyleAspectRatio::StyleAspectRatio(EAspectRatioType type, gfx::SizeF ratio)
    : type_(static_cast<unsigned>(type)),
      ratio_(ratio),
      layout_ratio_(LayoutRatioFromSizeF(ratio)) {}

}  // namespace blink
