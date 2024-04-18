// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_BRUSH_FAMILY_H_
#define PDF_INK_INK_BRUSH_FAMILY_H_

#include <memory>
#include <string_view>

namespace chrome_pdf {

struct InkBrushPaint;
struct InkBrushTip;

class InkBrushFamily {
 public:
  static std::unique_ptr<InkBrushFamily> Create(const InkBrushTip& tip,
                                                const InkBrushPaint& paint,
                                                std::string_view uri_string);

  ~InkBrushFamily();

 protected:
  InkBrushFamily();
};

}  // namespace chrome_pdf

#endif  // PDF_INK_INK_BRUSH_FAMILY_H_
