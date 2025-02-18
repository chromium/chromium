// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_recording_context_2d.h"

#include <cmath>

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_canvasfilter_string.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d_state.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/identifiability_study_helper.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"

namespace blink {

CanvasRecordingContext2D::CanvasRecordingContext2D() {
  state_stack_.push_back(MakeGarbageCollected<CanvasRenderingContext2DState>());
}

void CanvasRecordingContext2D::scale(double sx, double sy) {
  // TODO(crbug.com/40153853): Investigate the performance impact of simply
  // calling the 3d version of this function
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c) {
    return;
  }

  if (!std::isfinite(sx) || !std::isfinite(sy)) {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kScale, sx, sy);
  }

  const CanvasRenderingContext2DState& state = GetState();
  AffineTransform new_transform = state.GetTransform();
  float fsx = ClampTo<float>(sx);
  float fsy = ClampTo<float>(sy);
  new_transform.ScaleNonUniform(fsx, fsy);
  if (state.GetTransform() == new_transform) {
    return;
  }

  SetTransform(new_transform);
  c->scale(fsx, fsy);

  if (IsTransformInvertible()) [[likely]] {
    GetModifiablePath().Transform(
        AffineTransform().ScaleNonUniform(1.0 / fsx, 1.0 / fsy));
  }
}

void CanvasRecordingContext2D::rotate(double angle_in_radians) {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c) {
    return;
  }

  if (!std::isfinite(angle_in_radians)) {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kRotate,
                                                angle_in_radians);
  }

  const CanvasRenderingContext2DState& state = GetState();
  AffineTransform new_transform = state.GetTransform();
  new_transform.RotateRadians(angle_in_radians);
  if (state.GetTransform() == new_transform) {
    return;
  }

  SetTransform(new_transform);
  c->rotate(ClampTo<float>(angle_in_radians * (180.0 / kPiFloat)));

  if (IsTransformInvertible()) [[likely]] {
    GetModifiablePath().Transform(
        AffineTransform().RotateRadians(-angle_in_radians));
  }
}

void CanvasRecordingContext2D::translate(double tx, double ty) {
  // TODO(crbug.com/40153853): Investigate the performance impact of simply
  // calling the 3d version of this function
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c) {
    return;
  }

  if (!IsTransformInvertible()) [[unlikely]] {
    return;
  }

  if (!std::isfinite(tx) || !std::isfinite(ty)) {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kTranslate, tx, ty);
  }

  const CanvasRenderingContext2DState& state = GetState();
  AffineTransform new_transform = state.GetTransform();
  // clamp to float to avoid float cast overflow when used as SkScalar
  float ftx = ClampTo<float>(tx);
  float fty = ClampTo<float>(ty);
  new_transform.Translate(ftx, fty);
  if (state.GetTransform() == new_transform) {
    return;
  }

  SetTransform(new_transform);
  c->translate(ftx, fty);

  if (IsTransformInvertible()) [[likely]] {
    GetModifiablePath().Transform(AffineTransform().Translate(-ftx, -fty));
  }
}

void CanvasRecordingContext2D::transform(double m11,
                                         double m12,
                                         double m21,
                                         double m22,
                                         double dx,
                                         double dy) {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c) {
    return;
  }

  if (!std::isfinite(m11) || !std::isfinite(m21) || !std::isfinite(dx) ||
      !std::isfinite(m12) || !std::isfinite(m22) || !std::isfinite(dy)) {
    return;
  }

  // clamp to float to avoid float cast overflow when used as SkScalar
  float fm11 = ClampTo<float>(m11);
  float fm12 = ClampTo<float>(m12);
  float fm21 = ClampTo<float>(m21);
  float fm22 = ClampTo<float>(m22);
  float fdx = ClampTo<float>(dx);
  float fdy = ClampTo<float>(dy);
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kTransform, fm11,
                                                fm12, fm21, fm22, fdx, fdy);
  }

  AffineTransform transform(fm11, fm12, fm21, fm22, fdx, fdy);
  const CanvasRenderingContext2DState& state = GetState();
  AffineTransform new_transform = state.GetTransform() * transform;
  if (state.GetTransform() == new_transform) {
    return;
  }

  SetTransform(new_transform);
  c->concat(AffineTransformToSkM44(transform));

  if (IsTransformInvertible()) [[likely]] {
    GetModifiablePath().Transform(transform.Inverse());
  }
}

void CanvasRecordingContext2D::setTransform(double m11,
                                            double m12,
                                            double m21,
                                            double m22,
                                            double dx,
                                            double dy) {
  if (!std::isfinite(m11) || !std::isfinite(m21) || !std::isfinite(dx) ||
      !std::isfinite(m12) || !std::isfinite(m22) || !std::isfinite(dy)) {
    return;
  }

  resetTransform();
  transform(m11, m12, m21, m22, dx, dy);
}

void CanvasRecordingContext2D::setTransform(DOMMatrixInit* transform,
                                            ExceptionState& exception_state) {
  DOMMatrixReadOnly* m =
      DOMMatrixReadOnly::fromMatrix(transform, exception_state);

  if (!m) {
    return;
  }

  setTransform(m->m11(), m->m12(), m->m21(), m->m22(), m->m41(), m->m42());
}

DOMMatrix* CanvasRecordingContext2D::getTransform() {
  const AffineTransform& t = GetState().GetTransform();
  DOMMatrix* m = DOMMatrix::Create();
  m->setA(t.A());
  m->setB(t.B());
  m->setC(t.C());
  m->setD(t.D());
  m->setE(t.E());
  m->setF(t.F());
  return m;
}

void CanvasRecordingContext2D::resetTransform() {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c) {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kResetTransform);
  }

  CanvasRenderingContext2DState& state = GetState();
  AffineTransform ctm = state.GetTransform();
  bool invertible_ctm = IsTransformInvertible();
  // It is possible that CTM is identity while CTM is not invertible.
  // When CTM becomes non-invertible, realizeSaves() can make CTM identity.
  if (ctm.IsIdentity() && invertible_ctm) {
    return;
  }

  // resetTransform() resolves the non-invertible CTM state.
  state.ResetTransform();
  SetIsTransformInvertible(true);
  // Set the SkCanvas' matrix to identity.
  c->setMatrix(SkM44());

  if (invertible_ctm) {
    GetModifiablePath().Transform(ctm);
  }
  // When else, do nothing because all transform methods didn't update m_path
  // when CTM became non-invertible.
  // It means that resetTransform() restores m_path just before CTM became
  // non-invertible.
}

double CanvasRecordingContext2D::globalAlpha() const {
  return GetState().GlobalAlpha();
}

void CanvasRecordingContext2D::setGlobalAlpha(double alpha) {
  if (!(alpha >= 0 && alpha <= 1)) {
    return;
  }
  CanvasRenderingContext2DState& state = GetState();
  if (state.GlobalAlpha() == alpha) {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetGlobalAlpha,
                                                alpha);
  }
  state.SetGlobalAlpha(alpha);
}

String CanvasRecordingContext2D::globalCompositeOperation() const {
  auto [composite_op, blend_mode] =
      CompositeAndBlendOpsFromSkBlendMode(GetState().GlobalComposite());
  return CanvasCompositeOperatorName(composite_op, blend_mode);
}

void CanvasRecordingContext2D::setGlobalCompositeOperation(
    const String& operation) {
  CompositeOperator op = kCompositeSourceOver;
  BlendMode blend_mode = BlendMode::kNormal;
  if (!ParseCanvasCompositeAndBlendMode(operation, op, blend_mode)) {
    return;
  }
  SkBlendMode sk_blend_mode = WebCoreCompositeToSkiaComposite(op, blend_mode);
  CanvasRenderingContext2DState& state = GetState();
  if (state.GlobalComposite() == sk_blend_mode) {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kSetGlobalCompositeOpertion, sk_blend_mode);
  }
  state.SetGlobalComposite(sk_blend_mode);
}

const V8UnionCanvasFilterOrString* CanvasRecordingContext2D::filter() const {
  const CanvasRenderingContext2DState& state = GetState();
  if (CanvasFilter* filter = state.GetCanvasFilter()) {
    return MakeGarbageCollected<V8UnionCanvasFilterOrString>(filter);
  }
  return MakeGarbageCollected<V8UnionCanvasFilterOrString>(
      state.UnparsedCSSFilter());
}

void CanvasRecordingContext2D::setFilter(
    ScriptState* script_state,
    const V8UnionCanvasFilterOrString* input) {
  if (!input) {
    return;
  }

  CanvasRenderingContext2DState& state = GetState();
  switch (input->GetContentType()) {
    case V8UnionCanvasFilterOrString::ContentType::kCanvasFilter:
      UseCounter::Count(GetTopExecutionContext(),
                        WebFeature::kCanvasRenderingContext2DCanvasFilter);
      state.SetCanvasFilter(input->GetAsCanvasFilter());
      SnapshotStateForFilter();
      // TODO(crbug.com/40191831): Instrument new canvas APIs.
      identifiability_study_helper_.set_encountered_skipped_ops();
      break;
    case V8UnionCanvasFilterOrString::ContentType::kString: {
      const String& filter_string = input->GetAsString();
      if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
        identifiability_study_helper_.UpdateBuilder(
            CanvasOps::kSetFilter,
            IdentifiabilitySensitiveStringToken(filter_string));
      }
      if (!state.GetCanvasFilter() && !state.IsFontDirtyForFilter() &&
          filter_string == state.UnparsedCSSFilter()) {
        return;
      }
      const CSSValue* css_value = CSSParser::ParseSingleValue(
          CSSPropertyID::kFilter, filter_string,
          MakeGarbageCollected<CSSParserContext>(
              kHTMLStandardMode,
              ExecutionContext::From(script_state)->GetSecureContextMode()));
      if (!css_value || css_value->IsCSSWideKeyword()) {
        return;
      }
      state.SetUnparsedCSSFilter(filter_string);
      state.SetCSSFilter(css_value);
      SnapshotStateForFilter();
      break;
    }
  }
}

double CanvasRecordingContext2D::shadowOffsetX() const {
  return GetState().ShadowOffset().x();
}

void CanvasRecordingContext2D::setShadowOffsetX(double x) {
  if (!std::isfinite(x)) {
    return;
  }
  CanvasRenderingContext2DState& state = GetState();
  if (state.ShadowOffset().x() == x) {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetShadowOffsetX,
                                                x);
  }
  state.SetShadowOffsetX(ClampTo<float>(x));
}

double CanvasRecordingContext2D::shadowOffsetY() const {
  return GetState().ShadowOffset().y();
}

void CanvasRecordingContext2D::setShadowOffsetY(double y) {
  if (!std::isfinite(y)) {
    return;
  }
  CanvasRenderingContext2DState& state = GetState();
  if (state.ShadowOffset().y() == y) {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetShadowOffsetY,
                                                y);
  }
  state.SetShadowOffsetY(ClampTo<float>(y));
}

double CanvasRecordingContext2D::shadowBlur() const {
  return GetState().ShadowBlur();
}

void CanvasRecordingContext2D::setShadowBlur(double blur) {
  if (!std::isfinite(blur) || blur < 0) {
    return;
  }
  CanvasRenderingContext2DState& state = GetState();
  if (state.ShadowBlur() == blur) {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetShadowBlur,
                                                blur);
  }
  state.SetShadowBlur(ClampTo<float>(blur));
}

void CanvasRecordingContext2D::Trace(Visitor* visitor) const {
  visitor->Trace(state_stack_);
  CanvasPath::Trace(visitor);
}

HTMLCanvasElement* CanvasRecordingContext2D::HostAsHTMLCanvasElement() const {
  return nullptr;
}

}  // namespace blink
