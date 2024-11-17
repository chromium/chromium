// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INK_CURSOR_H_
#define PDF_PDF_INK_CURSOR_H_

#include "pdf/buildflags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

namespace chrome_pdf {

// Converts brush size into cursor diameter, for use with GenerateToolCursor().
// `brush_size` must be in the range that passes the validation performed with
// PdfInkBrush::IsToolSizeInRange().
int CursorDiameterFromBrushSizeAndZoom(float brush_size, float zoom);

// Draws a custom circular cursor to represent the brush/highlighter/eraser
// tool. `diameter` must be in the range [6, 32]. If it is too small, then it
// becomes too hard to see. If it is too big, then it may not display due to OS
// limitations.
SkBitmap GenerateToolCursor(SkColor color, int diameter);

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_CURSOR_H_
