// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/ink_brush_paint.h"

namespace chrome_pdf {

InkBrushPaint::InkBrushPaint() = default;

InkBrushPaint::InkBrushPaint(InkBrushPaint&&) noexcept = default;

InkBrushPaint& InkBrushPaint::operator=(InkBrushPaint&&) noexcept = default;

InkBrushPaint::~InkBrushPaint() = default;

InkBrushPaint::TextureLayer::TextureLayer() = default;

InkBrushPaint::TextureLayer::TextureLayer(const InkBrushPaint::TextureLayer&) =
    default;

InkBrushPaint::TextureLayer& InkBrushPaint::TextureLayer::operator=(
    const InkBrushPaint::TextureLayer&) = default;

InkBrushPaint::TextureLayer::~TextureLayer() = default;

}  // namespace chrome_pdf
