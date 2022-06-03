// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_matrix_component.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_css_matrix_component_options.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

DOMMatrix* To2DMatrix(DOMMatrixReadOnly* matrix) {
  DOMMatrix* twoDimensionalMatrix = DOMMatrix::Create();
  twoDimensionalMatrix->setA(matrix->m11());
  twoDimensionalMatrix->setB(matrix->m12());
  twoDimensionalMatrix->setC(matrix->m21());
  twoDimensionalMatrix->setD(matrix->m22());
  twoDimensionalMatrix->setE(matrix->m41());
  twoDimensionalMatrix->setF(matrix->m42());
  return twoDimensionalMatrix;
}

}  // namespace

CSSMatrixComponent* CSSMatrixComponent::Create(
    DOMMatrixReadOnly* matrix,
    const CSSMatrixComponentOptions* options) {
  return MakeGarbageCollected<CSSMatrixComponent>(
      matrix, options->hasIs2D() ? options->is2D() : matrix->is2D());
}

DOMMatrix* CSSMatrixComponent::toMatrix(ExceptionState&) const {
  if (is2D() && !matrix_->is2D())
    return To2DMatrix(matrix_);
  return DOMMatrix::Create(matrix_.Get());
}

CSSMatrixComponent* CSSMatrixComponent::FromCSSValue(
    const CSSFunctionValue& value) {
  WTF::Vector<double> entries;
  for (const auto& item : value)
    entries.push_back(To<CSSPrimitiveValue>(*item).GetDoubleValue());

  return CSSMatrixComponent::Create(
      DOMMatrixReadOnly::CreateForSerialization(entries.data(), entries.size()),
      CSSMatrixComponentOptions::Create());
}

const CSSFunctionValue* CSSMatrixComponent::ToCSSValue() const {
  CSSFunctionValue* result = MakeGarbageCollected<CSSFunctionValue>(
      is2D() ? CSSValueID::kMatrix : CSSValueID::kMatrix3d);

  if (is2D()) {
    double values[6] = {matrix_->a(), matrix_->b(), matrix_->c(),
                        matrix_->d(), matrix_->e(), matrix_->f()};
    for (double value : values) {
      result->Append(*CSSNumericLiteralValue::Create(
          value, CSSPrimitiveValue::UnitType::kNumber));
    }
  } else {
    double values[16] = {
        matrix_->m11(), matrix_->m12(), matrix_->m13(), matrix_->m14(),
        matrix_->m21(), matrix_->m22(), matrix_->m23(), matrix_->m24(),
        matrix_->m31(), matrix_->m32(), matrix_->m33(), matrix_->m34(),
        matrix_->m41(), matrix_->m42(), matrix_->m43(), matrix_->m44()};
    for (double value : values) {
      result->Append(*CSSNumericLiteralValue::Create(
          value, CSSPrimitiveValue::UnitType::kNumber));
    }
  }

  return result;
}

}  // namespace blink
