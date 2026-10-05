// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_2d_bitmap_provider.h"

#include <utility>

#include "base/check.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

Canvas2DBitmapProvider::Canvas2DBitmapProvider(sk_sp<SkSurface> surface)
    : surface_(std::move(surface)) {
  CHECK(surface_);
}

Canvas2DBitmapProvider::~Canvas2DBitmapProvider() = default;

}  // namespace blink
