// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_brush.h"

#include <numbers>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "pdf/ink/ink_brush.h"
#include "pdf/ink/ink_brush_family.h"
#include "pdf/ink/ink_brush_paint.h"
#include "pdf/ink/ink_brush_tip.h"

namespace chrome_pdf {

namespace {

std::string CreateBrushUri() {
  // TODO(crbug.com/335524380): Use real value here.
  return "ink://ink/texture:test-texture";
}

float GetOpacity(PdfInkBrush::Type type) {
  switch (type) {
    case PdfInkBrush::Type::kHighlighter:
      return 0.4f;
    case PdfInkBrush::Type::kPen:
      return 1.0f;
  }
}

std::unique_ptr<InkBrush> CreateInkBrush(PdfInkBrush::Type type,
                                         PdfInkBrush::Params params) {
  CHECK_GT(params.size, 0);

  // TODO(crbug.com/335524380): Use real values here.
  InkBrushTip tip;
  tip.corner_rounding = 0;
  tip.opacity_multiplier = GetOpacity(type);

  InkBrushPaint::TextureLayer layer;
  layer.color_texture_uri = CreateBrushUri();
  layer.mapping = InkBrushPaint::TextureMapping::kWinding;
  layer.size_unit = InkBrushPaint::TextureSizeUnit::kBrushSize;
  layer.size_x = 3;
  layer.size_y = 5;
  layer.size_jitter_x = 0.1;
  layer.size_jitter_y = 2;
  layer.keyframes = {
      {.progress = 0.1, .rotation_in_radians = std::numbers::pi_v<float> / 4}};
  layer.blend_mode = InkBrushPaint::BlendMode::kSrcIn;

  InkBrushPaint paint;
  paint.texture_layers.push_back(layer);
  auto family = InkBrushFamily::Create(std::move(tip), std::move(paint), "");
  CHECK(family);

  return InkBrush::Create(std::move(family),
                          /*color=*/params.color,
                          /*size=*/params.size,
                          /*epsilon=*/0.1f);
}

}  // namespace

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
    : ink_brush_(CreateInkBrush(brush_type, brush_params)) {
  CHECK(ink_brush_);
}

PdfInkBrush::~PdfInkBrush() = default;

const InkBrush& PdfInkBrush::GetInkBrush() const {
  return *ink_brush_;
}

}  // namespace chrome_pdf
