// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/ink_brush_family.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "pdf/ink/ink_brush_paint.h"
#include "pdf/ink/ink_brush_tip.h"

namespace chrome_pdf {

// static
std::unique_ptr<InkBrushFamily> InkBrushFamily::Create(
    InkBrushTip tip,
    InkBrushPaint paint,
    std::string_view uri_string) {
  // Protected ctor.
  return base::WrapUnique(new InkBrushFamily());
}

}  // namespace chrome_pdf
