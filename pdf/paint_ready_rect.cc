// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/paint_ready_rect.h"

#include "pdf/ppapi_migration/image.h"
#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

PaintReadyRect::PaintReadyRect(const gfx::Rect& rect,
                               const Image& image,
                               bool flush_now)
    : rect_(rect), image_(image), flush_now_(flush_now) {}

PaintReadyRect::PaintReadyRect(const PaintReadyRect& other) = default;

PaintReadyRect& PaintReadyRect::operator=(const PaintReadyRect& other) =
    default;

PaintReadyRect::~PaintReadyRect() = default;

}  // namespace chrome_pdf
