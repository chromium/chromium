// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/geometry/dom_matrix_read_only.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_dom_matrix_2d_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_matrix_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_point_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_unrestricteddoublesequence.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/transform_builder.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/core/geometry/dom_point.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace {

void SetDictionaryMembers(DOMMatrix2DInit* other) {
  if (!other->hasM11())
    other->setM11(other->hasA() ? other->a() : 1);

  if (!other->hasM12())
    other->setM12(other->hasB() ? other->b() : 0);

  if (!other->hasM21())
    other->setM21(other->hasC() ? other->c() : 0);

  if (!other->hasM22())
    other->setM22(other->hasD() ? other->d() : 1);

  if (!other->hasM41())
    other->setM41(other->hasE() ? other->e() : 0);

  if (!other->hasM42())
    other->setM42(other->hasF() ? other->f() : 0);
}

}  // namespace

bool DOMMatrixReadOnly::ValidateAndFixup2D(DOMMatrix2DInit* other) {
  if (other->hasA() && other->hasM11() && other->a() != other->m11() &&
      !(std::isnan(other->a()) && std::isnan(other->m11()))) {
    return false;
  }
  if (other->hasB() && other->hasM12() && other->b() != other->m12() &&
      !(std::isnan(other->b()) && std::isnan(other->m12()))) {
    return false;
  }
  if (other->hasC() && other->hasM21() && other->c() != other->m21() &&
      !(std::isnan(other->c()) && std::isnan(other->m21()))) {
    return false;
  }
  if (other->hasD() && other->hasM22() && other->d() != other->m22() &&
      !(std::isnan(other->d()) && std::isnan(other->m22()))) {
    return false;
  }
  if (other->hasE() && other->hasM41() && other->e() != other->m41() &&
      !(std::isnan(other->e()) && std::isnan(other->m41()))) {
    return false;
  }
  if (other->hasF() && other->hasM42() && other->f() != other->m42() &&
      !(std::isnan(other->f()) && std::isnan(other->m42()))) {
    return false;
  }

  SetDictionaryMembers(other);
  return true;
}

bool DOMMatrixReadOnly::ValidateAndFixup(DOMMatrixInit* other,
                                         ExceptionState& exception_state) {
  if (!ValidateAndFixup2D(other)) {
    exception_state.ThrowTypeError(
        "Property mismatch on matrix initialization.");
    return false;
  }

  if (other->hasIs2D() && other->is2D() &&
      (other->m31() || other->m32() || other->m13() || other->m23() ||
       other->m43() || other->m14() || other->m24() || other->m34() ||
       other->m33() != 1 || other->m44() != 1)) {
    exception_state.ThrowTypeError(
        "The is2D member is set to true but the input matrix is a 3d matrix.");
    return false;
  }

  if (!other->hasIs2D()) {
    bool is2d =
        !(other->m31() || other->m32() || other->m13() || other->m23() ||
          other->m43() || other->m14() || other->m24() || other->m34() ||
          other->m33() != 1 || other->m44() != 1);
    other->setIs2D(is2d);
  }
  return true;
}

DOMMatrixReadOnly* DOMMatrixReadOnly::Create(
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<DOMMatrixReadOnly>(gfx::Transform());
}

DOMMatrixReadOnly* DOMMatrixReadOnly::Create(
    ExecutionContext* execution_context,
    const V8UnionStringOrUnrestrictedDoubleSequence* init,
    ExceptionState& exception_state) {
  DCHECK(init);

  switch (init->GetContentType()) {
    case V8UnionStringOrUnrestrictedDoubleSequence::ContentType::kString: {
      if (!execution_context->IsWindow()) {
        exception_state.ThrowTypeError(
            "DOMMatrix can't be constructed with strings on workers.");
        return nullptr;
      }

      DOMMatrixReadOnly* matrix =
          MakeGarbageCollected<DOMMatrixReadOnly>(gfx::Transform());
      matrix->SetMatrixValueFromString(execution_context, init->GetAsString(),
                                       exception_state);
      return matrix;
    }
    case V8UnionStringOrUnrestrictedDoubleSequence::ContentType::
        kUnrestrictedDoubleSequence: {
      const Vector<double>& sequence = init->GetAsUnrestrictedDoubleSequence();
      if (sequence.size() != 6 && sequence.size() != 16) {
        exception_state.ThrowTypeError(
            "The sequence must contain 6 elements for a 2D matrix or 16 "
            "elements "
            "for a 3D matrix.");
        return nullptr;
      }
      return MakeGarbageCollected<DOMMatrixReadOnly>(sequence, sequence.size());
    }
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

DOMMatrixReadOnly* DOMMatrixReadOnly::CreateForSerialization(double sequence[],
                                                             int size) {
  return MakeGarbageCollected<DOMMatrixReadOnly>(sequence, size);
}

DOMMatrixReadOnly* DOMMatrixReadOnly::fromFloat32Array(
    NotShared<DOMFloat32Array> float32_array,
    ExceptionState& exception_state) {
  if (float32_array->length() != 6 && float32_array->length() != 16) {
    exception_state.ThrowTypeError(
        "The sequence must contain 6 elements for a 2D matrix or 16 elements a "
        "for 3D matrix.");
    return nullptr;
  }
  return MakeGarbageCollected<DOMMatrixReadOnly>(
      float32_array->Data(), static_cast<int>(float32_array->length()));
}

DOMMatrixReadOnly* DOMMatrixReadOnly::fromFloat64Array(
    NotShared<DOMFloat64Array> float64_array,
    ExceptionState& exception_state) {
  if (float64_array->length() != 6 && float64_array->length() != 16) {
    exception_state.ThrowTypeError(
        "The sequence must contain 6 elements for a 2D matrix or 16 elements "
        "for a 3D matrix.");
    return nullptr;
  }
  return MakeGarbageCollected<DOMMatrixReadOnly>(
      float64_array->Data(), static_cast<int>(float64_array->length()));
}

DOMMatrixReadOnly* DOMMatrixReadOnly::fromMatrix2D(
    DOMMatrix2DInit* other,
    ExceptionState& exception_state) {
  if (!ValidateAndFixup2D(other)) {
    exception_state.ThrowTypeError(
        "Property mismatch on matrix initialization.");
    return nullptr;
  }
  double args[] = {other->m11(), other->m12(), other->m21(),
                   other->m22(), other->m41(), other->m42()};
  return MakeGarbageCollected<DOMMatrixReadOnly>(args, 6);
}

DOMMatrixReadOnly* DOMMatrixReadOnly::fromMatrix(
    DOMMatrixInit* other,
    ExceptionState& exception_state) {
  if (!ValidateAndFixup(other, exception_state)) {
    DCHECK(exception_state.HadException());
    return nullptr;
  }
  if (other->is2D()) {
    double args[] = {other->m11(), other->m12(), other->m21(),
                     other->m22(), other->m41(), other->m42()};
    return MakeGarbageCollected<DOMMatrixReadOnly>(args, 6);
  }

  double args[] = {other->m11(), other->m12(), other->m13(), other->m14(),
                   other->m21(), other->m22(), other->m23(), other->m24(),
                   other->m31(), other->m32(), other->m33(), other->m34(),
                   other->m41(), other->m42(), other->m43(), other->m44()};
  return MakeGarbageCollected<DOMMatrixReadOnly>(args, 16);
}

DOMMatrixReadOnly::~DOMMatrixReadOnly() = default;

bool DOMMatrixReadOnly::is2D() const {
  return is2d_;
}

bool DOMMatrixReadOnly::isIdentity() const {
  return matrix_.IsIdentity();
}

DOMMatrix* DOMMatrixReadOnly::multiply(DOMMatrixInit* other,
                                       ExceptionState& exception_state) {
  return DOMMatrix::Create(this)->multiplySelf(other, exception_state);
}

DOMMatrix* DOMMatrixReadOnly::translate(double tx, double ty, double tz) {
  return DOMMatrix::Create(this)->translateSelf(tx, ty, tz);
}

DOMMatrix* DOMMatrixReadOnly::scale(double sx) {
  return scale(sx, sx);
}

DOMMatrix* DOMMatrixReadOnly::scale(double sx,
                                    double sy,
                                    double sz,
                                    double ox,
                                    double oy,
                                    double oz) {
  return DOMMatrix::Create(this)->scaleSelf(sx, sy, sz, ox, oy, oz);
}

DOMMatrix* DOMMatrixReadOnly::scale3d(double scale,
                                      double ox,
                                      double oy,
                                      double oz) {
  return DOMMatrix::Create(this)->scale3dSelf(scale, ox, oy, oz);
}

DOMMatrix* DOMMatrixReadOnly::scaleNonUniform(double sx, double sy) {
  return DOMMatrix::Create(this)->scaleSelf(sx, sy, 1, 0, 0, 0);
}

DOMMatrix* DOMMatrixReadOnly::rotate(double rot_x) {
  return DOMMatrix::Create(this)->rotateSelf(rot_x);
}

DOMMatrix* DOMMatrixReadOnly::rotate(double rot_x, double rot_y) {
  return DOMMatrix::Create(this)->rotateSelf(rot_x, rot_y);
}

DOMMatrix* DOMMatrixReadOnly::rotate(double rot_x, double rot_y, double rot_z) {
  return DOMMatrix::Create(this)->rotateSelf(rot_x, rot_y, rot_z);
}

DOMMatrix* DOMMatrixReadOnly::rotateFromVector(double x, double y) {
  return DOMMatrix::Create(this)->rotateFromVectorSelf(x, y);
}

DOMMatrix* DOMMatrixReadOnly::rotateAxisAngle(double x,
                                              double y,
                                              double z,
                                              double angle) {
  return DOMMatrix::Create(this)->rotateAxisAngleSelf(x, y, z, angle);
}

DOMMatrix* DOMMatrixReadOnly::skewX(double sx) {
  return DOMMatrix::Create(this)->skewXSelf(sx);
}

DOMMatrix* DOMMatrixReadOnly::skewY(double sy) {
  return DOMMatrix::Create(this)->skewYSelf(sy);
}

DOMMatrix* DOMMatrixReadOnly::flipX() {
  DOMMatrix* flip_x = DOMMatrix::Create(this);
  flip_x->setM11(-m11());
  flip_x->setM12(-m12());
  flip_x->setM13(-m13());
  flip_x->setM14(-m14());
  return flip_x;
}

DOMMatrix* DOMMatrixReadOnly::flipY() {
  DOMMatrix* flip_y = DOMMatrix::Create(this);
  flip_y->setM21(-m21());
  flip_y->setM22(-m22());
  flip_y->setM23(-m23());
  flip_y->setM24(-m24());
  return flip_y;
}

DOMMatrix* DOMMatrixReadOnly::inverse() {
  return DOMMatrix::Create(this)->invertSelf();
}

DOMPoint* DOMMatrixReadOnly::transformPoint(const DOMPointInit* point) {
  if (is2D() && point->z() == 0 && point->w() == 1) {
    double x = point->x() * m11() + point->y() * m21() + m41();
    double y = point->x() * m12() + point->y() * m22() + m42();
    return DOMPoint::Create(x, y, 0, 1);
  }

  double x = point->x() * m11() + point->y() * m21() + point->z() * m31() +
             point->w() * m41();
  double y = point->x() * m12() + point->y() * m22() + point->z() * m32() +
             point->w() * m42();
  double z = point->x() * m13() + point->y() * m23() + point->z() * m33() +
             point->w() * m43();
  double w = point->x() * m14() + point->y() * m24() + point->z() * m34() +
             point->w() * m44();
  return DOMPoint::Create(x, y, z, w);
}

DOMMatrixReadOnly::DOMMatrixReadOnly(const gfx::Transform& matrix, bool is2d)
    : matrix_(matrix), is2d_(is2d) {}

NotShared<DOMFloat32Array> DOMMatrixReadOnly::toFloat32Array() const {
  float array[16];
  matrix_.GetColMajorF(array);
  return NotShared<DOMFloat32Array>(DOMFloat32Array::Create(array));
}

NotShared<DOMFloat64Array> DOMMatrixReadOnly::toFloat64Array() const {
  double array[16];
  matrix_.GetColMajor(array);
  return NotShared<DOMFloat64Array>(DOMFloat64Array::Create(array));
}

const String DOMMatrixReadOnly::toString(
    ExceptionState& exception_state) const {
  constexpr const char* kComma = ", ";
  StringBuilder result;

  if (is2D()) {
    if (!std::isfinite(a()) || !std::isfinite(b()) || !std::isfinite(c()) ||
        !std::isfinite(d()) || !std::isfinite(e()) || !std::isfinite(f())) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "DOMMatrix cannot be serialized with NaN or Infinity values.");
      return String();
    }

    result.Append("matrix(");
    result.Append(String::NumberToStringECMAScript(a()));
    result.Append(kComma);
    result.Append(String::NumberToStringECMAScript(b()));
    result.Append(kComma);
    result.Append(String::NumberToStringECMAScript(c()));
    result.Append(kComma);
    result.Append(String::NumberToStringECMAScript(d()));
    result.Append(kComma);
    result.Append(String::NumberToStringECMAScript(e()));
    result.Append(kComma);
    result.Append(String::NumberToStringECMAScript(f()));
    result.Append(")");
    return result.ToString();
  }

  if (!std::isfinite(m11()) || !std::isfinite(m12()) || !std::isfinite(m13()) ||
      !std::isfinite(m14()) || !std::isfinite(m21()) || !std::isfinite(m22()) ||
      !std::isfinite(m23()) || !std::isfinite(m24()) || !std::isfinite(m31()) ||
      !std::isfinite(m32()) || !std::isfinite(m33()) || !std::isfinite(m34()) ||
      !std::isfinite(m41()) || !std::isfinite(m42()) || !std::isfinite(m43()) ||
      !std::isfinite(m44())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "DOMMatrix cannot be serialized with NaN or Infinity values.");
    return String();
  }

  result.Append("matrix3d(");
  result.Append(String::NumberToStringECMAScript(m11()));
  result.Append(kComma);
  result.Append(String::NumberToStringECMAScript(m12()));
  result.Append(kComma);
  result.Append(String::NumberToStringECMAScript(m13()));
  result.Append(kComma);
  result.Append(String::NumberToStringECMAScript(m14()));
  result.Append(kComma);
  result.Append(String::NumberToStringECMAScript(m21()));
  result.Append(kComma);
  result.Append(String::NumberToStringECMAScript(m22()));
  result.Append(kComma);
  result.Append(String::NumberToStringECMAScript(m23()));
  result.Append(kComma);
  result.Append(String::NumberToStringECMAScript(m24()));
  result.Append(kComma);
  result.Append(String::NumberToStringECMAScript(m31()));
  result.Append(kComma);
  result.Append(String::NumberToStringECMAScript(m32()));
  result.Append(kComma);
  result.Append(String::NumberToStringECMAScript(m33()));
  result.Append(kComma);
  result.Append(String::NumberToStringECMAScript(m34()));
  result.Append(kComma);
  result.Append(String::NumberToStringECMAScript(m41()));
  result.Append(kComma);
  result.Append(String::NumberToStringECMAScript(m42()));
  result.Append(kComma);
  result.Append(String::NumberToStringECMAScript(m43()));
  result.Append(kComma);
  result.Append(String::NumberToStringECMAScript(m44()));
  result.Append(")");

  return result.ToString();
}

ScriptValue DOMMatrixReadOnly::toJSONForBinding(
    ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  result.AddNumber("a", a());
  result.AddNumber("b", b());
  result.AddNumber("c", c());
  result.AddNumber("d", d());
  result.AddNumber("e", e());
  result.AddNumber("f", f());
  result.AddNumber("m11", m11());
  result.AddNumber("m12", m12());
  result.AddNumber("m13", m13());
  result.AddNumber("m14", m14());
  result.AddNumber("m21", m21());
  result.AddNumber("m22", m22());
  result.AddNumber("m23", m23());
  result.AddNumber("m24", m24());
  result.AddNumber("m31", m31());
  result.AddNumber("m32", m32());
  result.AddNumber("m33", m33());
  result.AddNumber("m34", m34());
  result.AddNumber("m41", m41());
  result.AddNumber("m42", m42());
  result.AddNumber("m43", m43());
  result.AddNumber("m44", m44());
  result.AddBoolean("is2D", is2D());
  result.AddBoolean("isIdentity", isIdentity());
  return result.GetScriptValue();
}

AffineTransform DOMMatrixReadOnly::GetAffineTransform() const {
  return AffineTransform(a(), b(), c(), d(), e(), f());
}

void DOMMatrixReadOnly::SetMatrixValueFromString(
    const ExecutionContext* execution_context,
    const String& input_string,
    ExceptionState& exception_state) {
  DEFINE_STATIC_LOCAL(String, identity_matrix2d, ("matrix(1, 0, 0, 1, 0, 0)"));
  String string = input_string;
  if (string.empty())
    string = identity_matrix2d;

  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kTransform, string,
      StrictCSSParserContext(execution_context->GetSecureContextMode()));

  if (!value || value->IsCSSWideKeyword()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Failed to parse '" + input_string + "'.");
    return;
  }

  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK(identifier_value->GetValueID() == CSSValueID::kNone);
    matrix_.MakeIdentity();
    is2d_ = true;
    return;
  }

  if (TransformBuilder::HasRelativeLengths(To<CSSValueList>(*value))) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Lengths must be absolute, not relative");
    return;
  }

  TransformOperations operations = TransformBuilder::CreateTransformOperations(
      *value, CSSToLengthConversionData());

  if (operations.BoxSizeDependencies()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Lengths must be absolute, not depend on the box size");
    return;
  }

  matrix_.MakeIdentity();
  operations.Apply(gfx::SizeF(0, 0), matrix_);

  is2d_ = !operations.Has3DOperation();

  return;
}

}  // namespace blink
