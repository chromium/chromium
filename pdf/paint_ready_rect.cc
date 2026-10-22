// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/paint_ready_rect.h"

#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

PaintReadyRect::PaintReadyRect(const gfx::Rect& rect)
    : PaintReadyRect(rect, /*flush_now=*/false) {}

PaintReadyRect::PaintReadyRect(const gfx::Rect& rect, bool flush_now)
    : rect_(rect), flush_now_(flush_now) {}

PaintReadyRect::PaintReadyRect(PaintReadyRect&&) noexcept = default;

PaintReadyRect& PaintReadyRect::operator=(PaintReadyRect&&) noexcept = default;

PaintReadyRect::PaintReadyRect(const PaintReadyRect& other) = default;

PaintReadyRect& PaintReadyRect::operator=(const PaintReadyRect& other) =
    default;

PaintReadyRect::~PaintReadyRect() = default;

}  // namespace chrome_pdf
