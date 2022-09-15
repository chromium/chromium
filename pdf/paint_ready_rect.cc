// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/paint_ready_rect.h"

#include <utility>

#include "base/check.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

PaintReadyRect::PaintReadyRect(const gfx::Rect& rect,
                               sk_sp<SkImage> image,
                               bool flush_now)
    : rect_(rect), image_(std::move(image)), flush_now_(flush_now) {
  DCHECK(image_);
}

PaintReadyRect::PaintReadyRect(const PaintReadyRect& other) = default;

PaintReadyRect& PaintReadyRect::operator=(const PaintReadyRect& other) =
    default;

PaintReadyRect::~PaintReadyRect() = default;

}  // namespace chrome_pdf
