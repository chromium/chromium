// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_brush.h"

#include <optional>

#include "base/check_op.h"

namespace chrome_pdf {

// static
std::optional<PdfInkBrush::Type> PdfInkBrush::StringToType(
    const std::string& brush_type) {
  if (brush_type == "highlighter") {
    return Type::kHighlighter;
  }
  if (brush_type == "pen") {
    return Type::kPen;
  }
  return std::nullopt;
}

PdfInkBrush::PdfInkBrush(Type brush_type, Params brush_params)
    : type_(brush_type), params_(brush_params) {
  CHECK_GT(brush_params.size, 0);
}

PdfInkBrush::~PdfInkBrush() = default;

std::unique_ptr<InkBrush> PdfInkBrush::CreateInkBrush() {
  // TODO(crbug.com/335524382): Implement this.
  return nullptr;
}

}  // namespace chrome_pdf
