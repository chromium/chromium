// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INK_TEXT_H_
#define PDF_PDF_INK_TEXT_H_

#include <stdint.h>

#include <vector>

#include "pdf/mojom/pdf.mojom.h"
#include "pdf/pdf_ink_ids.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace chrome_pdf {

struct InkTextInfo {
  InkTextInfo(FontId,
              std::vector<uint32_t>,
              std::vector<gfx::Vector2dF>,
              gfx::RectF,
              bool);
  InkTextInfo(InkTextInfo&&) noexcept;
  InkTextInfo& operator=(InkTextInfo&&) noexcept;
  ~InkTextInfo();

  static std::vector<InkTextInfo> SplitTypefaceRuns(
      const std::vector<pdf::mojom::InkTextRunPtr>& text_runs,
      float effective_zoom);

  FontId font_id;
  std::vector<uint32_t> glyphs;
  std::vector<gfx::Vector2dF> glyph_positions;
  gfx::RectF location;
  bool is_horizontal;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_TEXT_H_
