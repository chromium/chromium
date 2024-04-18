// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/ink_brush.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "pdf/ink/ink_brush_family.h"
#include "third_party/skia/include/core/SkColor.h"

namespace chrome_pdf {

// static
std::unique_ptr<InkBrush> InkBrush::Create(
    std::unique_ptr<InkBrushFamily> family,
    SkColor color,
    float size,
    float epsilon) {
  if (!family) {
    return nullptr;
  }

  // Protected ctor.
  return base::WrapUnique(new InkBrush());
}

}  // namespace chrome_pdf
