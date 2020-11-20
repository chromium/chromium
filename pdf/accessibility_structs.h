// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_ACCESSIBILITY_STRUCTS_H_
#define PDF_ACCESSIBILITY_STRUCTS_H_

#include <stdint.h>

#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

struct AccessibilityPageInfo {
  uint32_t page_index = 0;
  gfx::Rect bounds;
  uint32_t text_run_count = 0;
  uint32_t char_count = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_ACCESSIBILITY_STRUCTS_H_