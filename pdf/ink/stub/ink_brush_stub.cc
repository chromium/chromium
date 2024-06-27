// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/stub/ink_brush_stub.h"

#include <memory>
#include <utility>

#include "pdf/ink/ink_brush_family.h"
#include "pdf/ink/ink_brush_tip.h"
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

  return std::make_unique<InkBrushStub>(std::move(family), color, size,
                                        epsilon);
}

InkBrushStub::InkBrushStub(std::unique_ptr<InkBrushFamily> family,
                           SkColor color,
                           float size,
                           float epsilon)
    : family_(std::move(family)), color_(color), size_(size) {}

InkBrushStub::~InkBrushStub() = default;

float InkBrushStub::GetSize() const {
  return size_;
}

SkColor InkBrushStub::GetColor() const {
  return color_;
}

float InkBrushStub::GetOpacityForTesting() const {
  return family_->GetOpacityForTesting();  // IN-TEST
}

}  // namespace chrome_pdf
