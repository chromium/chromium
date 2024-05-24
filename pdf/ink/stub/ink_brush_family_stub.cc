// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/stub/ink_brush_family_stub.h"

#include <memory>
#include <utility>

#include "pdf/ink/ink_brush_paint.h"
#include "pdf/ink/ink_brush_tip.h"

namespace chrome_pdf {

// static
std::unique_ptr<InkBrushFamily> InkBrushFamily::Create(
    InkBrushTip tip,
    InkBrushPaint paint,
    std::string_view uri_string) {
  return std::make_unique<InkBrushFamilyStub>(std::move(tip));
}

InkBrushFamilyStub::InkBrushFamilyStub(InkBrushTip tip)
    : opacity_(tip.opacity_multiplier) {}

InkBrushFamilyStub::~InkBrushFamilyStub() = default;

float InkBrushFamilyStub::GetOpacityForTesting() const {
  return opacity_;
}

}  // namespace chrome_pdf
