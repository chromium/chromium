// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/core/css/cssom/css_url_image_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_gradient.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_pattern.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/path_2d.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/graphics/canvas_heuristic_parameters.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

const char BaseRenderingContext2D::kDefaultFont[] = "10px sans-serif";
const char BaseRenderingContext2D::kInheritDirectionString[] = "inherit";
const char BaseRenderingContext2D::kRtlDirectionString[] = "rtl";
const char BaseRenderingContext2D::kLtrDirectionString[] = "ltr";
const double BaseRenderingContext2D::kCDeviceScaleFactor = 1.0;

BaseRenderingContext2D::BaseRenderingContext2D()
    : clip_antialiasing_(kNotAntiAliased), origin_tainted_by_content_(false) {
  state_stack_.push_back(CanvasRenderingContext2DState::Create());
}

BaseRenderingContext2D::~BaseRenderingContext2D() = default;

CanvasRenderingContext2DState& BaseRenderingContext2D::ModifiableState() {
  RealizeSaves();
  return *state_stack_.back();
}

void BaseRenderingContext2D::RealizeSaves() {
  ValidateStateStack();
  if (GetState().HasUnrealizedSaves()) {
    DCHECK_GE(state_stack_.size(), 1u);
    // Reduce the current state's unrealized count by one now,
    // to reflect the fact we are saving one state.
    state_stack_.back()->Restore();
    state_stack_.push_back(CanvasRenderingContext2DState::Create(
        GetState(), CanvasRenderingContext2DState::kDontCopyClipList));
    // Set the new state's unrealized count to 0, because it has no outstanding
    // saves.
    // We need to do this explicitly because the copy constructor and operator=
    // used by the Vector operations copy the unrealized count from the previous
    // state (in turn necessary to support correct resizing and unwinding of the
    // stack).
    state_stack_.back()->ResetUnrealizedSaveCount();
    cc::PaintCanvas* canvas = DrawingCanvas();
    if (canvas)
      canvas->save();
    ValidateStateStack();
  }
}

void BaseRenderingContext2D::save() {
  state_stack_.back()->Save();
}

void BaseRenderingContext2D::restore() {
  ValidateStateStack();
  if (GetState().HasUnrealizedSaves()) {
    // We never realized the save, so just record that it was unnecessary.
    state_stack_.back()->Restore();
    return;
  }
  DCHECK_GE(state_stack_.size(), 1u);
  if (state_stack_.size() <= 1)
    return;
  // Verify that the current state's transform is invertible.
  if (GetState().IsTransformInvertible())
    path_.Transform(GetState().Transform());

  state_stack_.pop_back();
  state_stack_.back()->ClearResolvedFilter();

  if (GetState().IsTransformInvertible())
    path_.Transform(GetState().Transform().Inverse());

  cc::PaintCanvas* c = DrawingCanvas();

  if (c)
    c->restore();

  ValidateStateStack();
}

void BaseRenderingContext2D::RestoreMatrixClipStack(cc::PaintCanvas* c) const {
  if (!c)
    return;
  HeapVector<Member<CanvasRenderingContext2DState>>::const_iterator curr_state;
  DCHECK(state_stack_.begin() < state_stack_.end());
  for (curr_state = state_stack_.begin(); curr_state < state_stack_.end();
       curr_state++) {
    c->setMatrix(SkMatrix::I());
    if (curr_state->Get()) {
      curr_state->Get()->PlaybackClips(c);
      c->setMatrix(AffineTransformToSkMatrix(curr_state->Get()->Transform()));
    }
    c->save();
  }
  c->restore();
  ValidateStateStack();
}

void BaseRenderingContext2D::UnwindStateStack() {
  if (size_t stack_size = state_stack_.size()) {
    if (cc::PaintCanvas* sk_canvas = ExistingDrawingCanvas()) {
      while (--stack_size)
        sk_canvas->restore();
    }
  }
}

void BaseRenderingContext2D::Reset() {
  ValidateStateStack();
  UnwindStateStack();
  state_stack_.resize(1);
  state_stack_.front() = CanvasRenderingContext2DState::Create();
  path_.Clear();
  if (cc::PaintCanvas* c = ExistingDrawingCanvas()) {
    // The canvas should always have an initial/unbalanced save frame, which
    // we use to reset the top level matrix and clip here.
    DCHECK_EQ(c->getSaveCount(), 2);
    c->restore();
    c->save();
    DCHECK(c->getTotalMatrix().isIdentity());
#if DCHECK_IS_ON()
    SkIRect clip_bounds;
    DCHECK(c->getDeviceClipBounds(&clip_bounds));
    DCHECK(clip_bounds == c->imageInfo().bounds());
#endif
  }
  ValidateStateStack();
  origin_tainted_by_content_ = false;
}

static inline void ConvertCanvasStyleToUnionType(
    CanvasStyle* style,
    StringOrCanvasGradientOrCanvasPattern& return_value) {
  if (CanvasGradient* gradient = style->GetCanvasGradient()) {
    return_value.SetCanvasGradient(gradient);
    return;
  }
  if (CanvasPattern* pattern = style->GetCanvasPattern()) {
    return_value.SetCanvasPattern(pattern);
    return;
  }
  return_value.SetString(style->GetColor());
}

void BaseRenderingContext2D::strokeStyle(
    StringOrCanvasGradientOrCanvasPattern& return_value) const {
  ConvertCanvasStyleToUnionType(GetState().StrokeStyle(), return_value);
}

void BaseRenderingContext2D::setStrokeStyle(
    const StringOrCanvasGradientOrCanvasPattern& style) {
  DCHECK(!style.IsNull());

  String color_string;
  CanvasStyle* canvas_style = nullptr;
  if (style.IsString()) {
    color_string = style.GetAsString();
    if (color_string == GetState().UnparsedStrokeColor())
      return;
    Color parsed_color = 0;
    if (!ParseColorOrCurrentColor(parsed_color, color_string))
      return;
    if (GetState().StrokeStyle()->IsEquivalentRGBA(parsed_color.Rgb())) {
      ModifiableState().SetUnparsedStrokeColor(color_string);
      return;
    }
    canvas_style = CanvasStyle::CreateFromRGBA(parsed_color.Rgb());
  } else if (style.IsCanvasGradient()) {
    canvas_style = CanvasStyle::CreateFromGradient(style.GetAsCanvasGradient());
  } else if (style.IsCanvasPattern()) {
    CanvasPattern* canvas_pattern = style.GetAsCanvasPattern();

    if (!origin_tainted_by_content_ && !canvas_pattern->OriginClean())
      SetOriginTaintedByContent();

    canvas_style = CanvasStyle::CreateFromPattern(canvas_pattern);
  }

  DCHECK(canvas_style);

  ModifiableState().SetStrokeStyle(canvas_style);
  ModifiableState().SetUnparsedStrokeColor(color_string);
  ModifiableState().ClearResolvedFilter();
}

void BaseRenderingContext2D::fillStyle(
    StringOrCanvasGradientOrCanvasPattern& return_value) const {
  ConvertCanvasStyleToUnionType(GetState().FillStyle(), return_value);
}

void BaseRenderingContext2D::setFillStyle(
    const StringOrCanvasGradientOrCanvasPattern& style) {
  DCHECK(!style.IsNull());
  ValidateStateStack();
  String color_string;
  CanvasStyle* canvas_style = nullptr;
  if (style.IsString()) {
    color_string = style.GetAsString();
    if (color_string == GetState().UnparsedFillColor())
      return;
    Color parsed_color = 0;
    if (!ParseColorOrCurrentColor(parsed_color, color_string))
      return;
    if (GetState().FillStyle()->IsEquivalentRGBA(parsed_color.Rgb())) {
      ModifiableState().SetUnparsedFillColor(color_string);
      return;
    }
    canvas_style = CanvasStyle::CreateFromRGBA(parsed_color.Rgb());
  } else if (style.IsCanvasGradient()) {
    canvas_style = CanvasStyle::CreateFromGradient(style.GetAsCanvasGradient());
  } else if (style.IsCanvasPattern()) {
    CanvasPattern* canvas_pattern = style.GetAsCanvasPattern();

    if (!origin_tainted_by_content_ && !canvas_pattern->OriginClean()) {
      SetOriginTaintedByContent();
    }
    if (canvas_pattern->GetPattern()->IsTextureBacked())
      DisableDeferral(kDisableDeferralReasonUsingTextureBackedPattern);
    canvas_style = CanvasStyle::CreateFromPattern(canvas_pattern);
  }

  DCHECK(canvas_style);
  ModifiableState().SetFillStyle(canvas_style);
  ModifiableState().SetUnparsedFillColor(color_string);
  ModifiableState().ClearResolvedFilter();
}

double BaseRenderingContext2D::lineWidth() const {
  return GetState().LineWidth();
}

void BaseRenderingContext2D::setLineWidth(double width) {
  if (!std::isfinite(width) || width <= 0)
    return;
  if (GetState().LineWidth() == width)
    return;
  ModifiableState().SetLineWidth(clampTo<float>(width));
}

String BaseRenderingContext2D::lineCap() const {
  return LineCapName(GetState().GetLineCap());
}

void BaseRenderingContext2D::setLineCap(const String& s) {
  LineCap cap;
  if (!ParseLineCap(s, cap))
    return;
  if (GetState().GetLineCap() == cap)
    return;
  ModifiableState().SetLineCap(cap);
}

String BaseRenderingContext2D::lineJoin() const {
  return LineJoinName(GetState().GetLineJoin());
}

void BaseRenderingContext2D::setLineJoin(const String& s) {
  LineJoin join;
  if (!ParseLineJoin(s, join))
    return;
  if (GetState().GetLineJoin() == join)
    return;
  ModifiableState().SetLineJoin(join);
}

double BaseRenderingContext2D::miterLimit() const {
  return GetState().MiterLimit();
}

void BaseRenderingContext2D::setMiterLimit(double limit) {
  if (!std::isfinite(limit) || limit <= 0)
    return;
  if (GetState().MiterLimit() == limit)
    return;
  ModifiableState().SetMiterLimit(clampTo<float>(limit));
}

double BaseRenderingContext2D::shadowOffsetX() const {
  return GetState().ShadowOffset().Width();
}

void BaseRenderingContext2D::setShadowOffsetX(double x) {
  if (!std::isfinite(x))
    return;
  if (GetState().ShadowOffset().Width() == x)
    return;
  ModifiableState().SetShadowOffsetX(clampTo<float>(x));
}

double BaseRenderingContext2D::shadowOffsetY() const {
  return GetState().ShadowOffset().Height();
}

void BaseRenderingContext2D::setShadowOffsetY(double y) {
  if (!std::isfinite(y))
    return;
  if (GetState().ShadowOffset().Height() == y)
    return;
  ModifiableState().SetShadowOffsetY(clampTo<float>(y));
}

double BaseRenderingContext2D::shadowBlur() const {
  return GetState().ShadowBlur();
}

void BaseRenderingContext2D::setShadowBlur(double blur) {
  if (!std::isfinite(blur) || blur < 0)
    return;
  if (GetState().ShadowBlur() == blur)
    return;
  ModifiableState().SetShadowBlur(clampTo<float>(blur));
}

String BaseRenderingContext2D::shadowColor() const {
  return Color(GetState().ShadowColor()).Serialized();
}

void BaseRenderingContext2D::setShadowColor(const String& color_string) {
  Color color;
  if (!ParseColorOrCurrentColor(color, color_string))
    return;
  if (GetState().ShadowColor() == color)
    return;
  ModifiableState().SetShadowColor(color.Rgb());
}

const Vector<double>& BaseRenderingContext2D::getLineDash() const {
  return GetState().LineDash();
}

static bool LineDashSequenceIsValid(const Vector<double>& dash) {
  return std::all_of(dash.begin(), dash.end(),
                     [](double d) { return std::isfinite(d) && d >= 0; });
}

void BaseRenderingContext2D::setLineDash(const Vector<double>& dash) {
  if (!LineDashSequenceIsValid(dash))
    return;
  ModifiableState().SetLineDash(dash);
}

double BaseRenderingContext2D::lineDashOffset() const {
  return GetState().LineDashOffset();
}

void BaseRenderingContext2D::setLineDashOffset(double offset) {
  if (!std::isfinite(offset) || GetState().LineDashOffset() == offset)
    return;
  ModifiableState().SetLineDashOffset(clampTo<float>(offset));
}

double BaseRenderingContext2D::globalAlpha() const {
  return GetState().GlobalAlpha();
}

void BaseRenderingContext2D::setGlobalAlpha(double alpha) {
  if (!(alpha >= 0 && alpha <= 1))
    return;
  if (GetState().GlobalAlpha() == alpha)
    return;
  ModifiableState().SetGlobalAlpha(alpha);
}

String BaseRenderingContext2D::globalCompositeOperation() const {
  return CompositeOperatorName(
      CompositeOperatorFromSkBlendMode(GetState().GlobalComposite()),
      BlendModeFromSkBlendMode(GetState().GlobalComposite()));
}

void BaseRenderingContext2D::setGlobalCompositeOperation(
    const String& operation) {
  CompositeOperator op = kCompositeSourceOver;
  BlendMode blend_mode = BlendMode::kNormal;
  if (!ParseCompositeAndBlendMode(operation, op, blend_mode))
    return;
  SkBlendMode sk_blend_mode = WebCoreCompositeToSkiaComposite(op, blend_mode);
  if (GetState().GlobalComposite() == sk_blend_mode)
    return;
  ModifiableState().SetGlobalComposite(sk_blend_mode);
}

String BaseRenderingContext2D::filter() const {
  return GetState().UnparsedFilter();
}

void BaseRenderingContext2D::setFilter(
    const ExecutionContext* execution_context,
    const String& filter_string) {
  if (filter_string == GetState().UnparsedFilter())
    return;

  const CSSValue* filter_value = CSSParser::ParseSingleValue(
      CSSPropertyFilter, filter_string,
      CSSParserContext::Create(kHTMLStandardMode,
                               execution_context->GetSecureContextMode()));

  if (!filter_value || filter_value->IsCSSWideKeyword())
    return;

  ModifiableState().SetUnparsedFilter(filter_string);
  ModifiableState().SetFilter(filter_value);
  SnapshotStateForFilter();
}

void BaseRenderingContext2D::scale(double sx, double sy) {
  cc::PaintCanvas* c = DrawingCanvas();
  if (!c)
    return;

  if (!std::isfinite(sx) || !std::isfinite(sy))
    return;

  AffineTransform new_transform = GetState().Transform();
  float fsx = clampTo<float>(sx);
  float fsy = clampTo<float>(sy);
  new_transform.ScaleNonUniform(fsx, fsy);
  if (GetState().Transform() == new_transform)
    return;

  ModifiableState().SetTransform(new_transform);
  if (!GetState().IsTransformInvertible())
    return;

  c->scale(fsx, fsy);
  path_.Transform(AffineTransform().ScaleNonUniform(1.0 / fsx, 1.0 / fsy));
}

void BaseRenderingContext2D::rotate(double angle_in_radians) {
  cc::PaintCanvas* c = DrawingCanvas();
  if (!c)
    return;

  if (!std::isfinite(angle_in_radians))
    return;

  AffineTransform new_transform = GetState().Transform();
  new_transform.RotateRadians(angle_in_radians);
  if (GetState().Transform() == new_transform)
    return;

  ModifiableState().SetTransform(new_transform);
  if (!GetState().IsTransformInvertible())
    return;
  c->rotate(clampTo<float>(angle_in_radians * (180.0 / kPiFloat)));
  path_.Transform(AffineTransform().RotateRadians(-angle_in_radians));
}

void BaseRenderingContext2D::translate(double tx, double ty) {
  cc::PaintCanvas* c = DrawingCanvas();
  if (!c)
    return;
  if (!GetState().IsTransformInvertible())
    return;

  if (!std::isfinite(tx) || !std::isfinite(ty))
    return;

  AffineTransform new_transform = GetState().Transform();
  // clamp to float to avoid float cast overflow when used as SkScalar
  float ftx = clampTo<float>(tx);
  float fty = clampTo<float>(ty);
  new_transform.Translate(ftx, fty);
  if (GetState().Transform() == new_transform)
    return;

  ModifiableState().SetTransform(new_transform);
  if (!GetState().IsTransformInvertible())
    return;
  c->translate(ftx, fty);
  path_.Transform(AffineTransform().Translate(-ftx, -fty));
}

void BaseRenderingContext2D::transform(double m11,
                                       double m12,
                                       double m21,
                                       double m22,
                                       double dx,
                                       double dy) {
  cc::PaintCanvas* c = DrawingCanvas();
  if (!c)
    return;

  if (!std::isfinite(m11) || !std::isfinite(m21) || !std::isfinite(dx) ||
      !std::isfinite(m12) || !std::isfinite(m22) || !std::isfinite(dy))
    return;

  // clamp to float to avoid float cast overflow when used as SkScalar
  float fm11 = clampTo<float>(m11);
  float fm12 = clampTo<float>(m12);
  float fm21 = clampTo<float>(m21);
  float fm22 = clampTo<float>(m22);
  float fdx = clampTo<float>(dx);
  float fdy = clampTo<float>(dy);

  AffineTransform transform(fm11, fm12, fm21, fm22, fdx, fdy);
  AffineTransform new_transform = GetState().Transform() * transform;
  if (GetState().Transform() == new_transform)
    return;

  ModifiableState().SetTransform(new_transform);
  if (!GetState().IsTransformInvertible())
    return;

  c->concat(AffineTransformToSkMatrix(transform));
  path_.Transform(transform.Inverse());
}

void BaseRenderingContext2D::resetTransform() {
  cc::PaintCanvas* c = DrawingCanvas();
  if (!c)
    return;

  AffineTransform ctm = GetState().Transform();
  bool invertible_ctm = GetState().IsTransformInvertible();
  // It is possible that CTM is identity while CTM is not invertible.
  // When CTM becomes non-invertible, realizeSaves() can make CTM identity.
  if (ctm.IsIdentity() && invertible_ctm)
    return;

  // resetTransform() resolves the non-invertible CTM state.
  ModifiableState().ResetTransform();
  c->setMatrix(AffineTransformToSkMatrix(AffineTransform()));

  if (invertible_ctm)
    path_.Transform(ctm);
  // When else, do nothing because all transform methods didn't update m_path
  // when CTM became non-invertible.
  // It means that resetTransform() restores m_path just before CTM became
  // non-invertible.
}

void BaseRenderingContext2D::setTransform(double m11,
                                          double m12,
                                          double m21,
                                          double m22,
                                          double dx,
                                          double dy) {
  if (!std::isfinite(m11) || !std::isfinite(m21) || !std::isfinite(dx) ||
      !std::isfinite(m12) || !std::isfinite(m22) || !std::isfinite(dy))
    return;

  resetTransform();
  // clamp to float to avoid float cast overflow when used as SkScalar
  float fm11 = clampTo<float>(m11);
  float fm12 = clampTo<float>(m12);
  float fm21 = clampTo<float>(m21);
  float fm22 = clampTo<float>(m22);
  float fdx = clampTo<float>(dx);
  float fdy = clampTo<float>(dy);

  transform(fm11, fm12, fm21, fm22, fdx, fdy);
}

void BaseRenderingContext2D::setTransform(DOMMatrix2DInit& transform,
                                          ExceptionState& exception_state) {
  DOMMatrixReadOnly* m =
      DOMMatrixReadOnly::fromMatrix2D(transform, exception_state);

  if (!m)
    return;

  setTransform(m->m11(), m->m12(), m->m21(), m->m22(), m->m41(), m->m42());
}

DOMMatrix* BaseRenderingContext2D::getTransform() {
  const AffineTransform& t = GetState().Transform();
  DOMMatrix* m = DOMMatrix::Create();
  m->setA(t.A());
  m->setB(t.B());
  m->setC(t.C());
  m->setD(t.D());
  m->setE(t.E());
  m->setF(t.F());
  return m;
}

void BaseRenderingContext2D::beginPath() {
  path_.Clear();
}

static bool ValidateRectForCanvas(double& x,
                                  double& y,
                                  double& width,
                                  double& height) {
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) ||
      !std::isfinite(height))
    return false;

  if (!width && !height)
    return false;

  if (width < 0) {
    width = -width;
    x -= width;
  }

  if (height < 0) {
    height = -height;
    y -= height;
  }

  return true;
}

bool BaseRenderingContext2D::IsFullCanvasCompositeMode(SkBlendMode op) {
  // See 4.8.11.1.3 Compositing
  // CompositeSourceAtop and CompositeDestinationOut are not listed here as the
  // platforms already implement the specification's behavior.
  return op == SkBlendMode::kSrcIn || op == SkBlendMode::kSrcOut ||
         op == SkBlendMode::kDstIn || op == SkBlendMode::kDstATop;
}

void BaseRenderingContext2D::DrawPathInternal(
    const Path& path,
    CanvasRenderingContext2DState::PaintType paint_type,
    SkPath::FillType fill_type) {
  if (path.IsEmpty())
    return;

  SkPath sk_path = path.GetSkPath();
  FloatRect bounds = path.BoundingRect();
  if (std::isnan(bounds.X()) || std::isnan(bounds.Y()) ||
      std::isnan(bounds.Width()) || std::isnan(bounds.Height()))
    return;
  sk_path.setFillType(fill_type);

  if (paint_type == CanvasRenderingContext2DState::kStrokePaintType)
    InflateStrokeRect(bounds);

  if (!DrawingCanvas())
    return;

  Draw([&sk_path](cc::PaintCanvas* c, const PaintFlags* flags)  // draw lambda
       { c->drawPath(sk_path, *flags); },
       [](const SkIRect& rect)  // overdraw test lambda
       { return false; },
       bounds, paint_type);
}

static SkPath::FillType ParseWinding(const String& winding_rule_string) {
  if (winding_rule_string == "nonzero")
    return SkPath::kWinding_FillType;
  if (winding_rule_string == "evenodd")
    return SkPath::kEvenOdd_FillType;

  NOTREACHED();
  return SkPath::kEvenOdd_FillType;
}

void BaseRenderingContext2D::fill(const String& winding_rule_string) {
  DrawPathInternal(path_, CanvasRenderingContext2DState::kFillPaintType,
                   ParseWinding(winding_rule_string));
}

void BaseRenderingContext2D::fill(Path2D* dom_path,
                                  const String& winding_rule_string) {
  DrawPathInternal(dom_path->GetPath(),
                   CanvasRenderingContext2DState::kFillPaintType,
                   ParseWinding(winding_rule_string));
}

void BaseRenderingContext2D::stroke() {
  DrawPathInternal(path_, CanvasRenderingContext2DState::kStrokePaintType);
}

void BaseRenderingContext2D::stroke(Path2D* dom_path) {
  DrawPathInternal(dom_path->GetPath(),
                   CanvasRenderingContext2DState::kStrokePaintType);
}

void BaseRenderingContext2D::fillRect(double x,
                                      double y,
                                      double width,
                                      double height) {
  if (!ValidateRectForCanvas(x, y, width, height))
    return;

  if (!DrawingCanvas())
    return;

  // clamp to float to avoid float cast overflow when used as SkScalar
  float fx = clampTo<float>(x);
  float fy = clampTo<float>(y);
  float fwidth = clampTo<float>(width);
  float fheight = clampTo<float>(height);

  SkRect rect = SkRect::MakeXYWH(fx, fy, fwidth, fheight);
  Draw([&rect](cc::PaintCanvas* c, const PaintFlags* flags)  // draw lambda
       { c->drawRect(rect, *flags); },
       [&rect, this](const SkIRect& clip_bounds)  // overdraw test lambda
       { return RectContainsTransformedRect(rect, clip_bounds); },
       rect, CanvasRenderingContext2DState::kFillPaintType);
}

static void StrokeRectOnCanvas(const FloatRect& rect,
                               cc::PaintCanvas* canvas,
                               const PaintFlags* flags) {
  DCHECK_EQ(flags->getStyle(), PaintFlags::kStroke_Style);
  if ((rect.Width() > 0) != (rect.Height() > 0)) {
    // When stroking, we must skip the zero-dimension segments
    SkPath path;
    path.moveTo(rect.X(), rect.Y());
    path.lineTo(rect.MaxX(), rect.MaxY());
    path.close();
    canvas->drawPath(path, *flags);
    return;
  }
  canvas->drawRect(rect, *flags);
}

void BaseRenderingContext2D::strokeRect(double x,
                                        double y,
                                        double width,
                                        double height) {
  if (!ValidateRectForCanvas(x, y, width, height))
    return;

  if (!DrawingCanvas())
    return;

  // clamp to float to avoid float cast overflow when used as SkScalar
  float fx = clampTo<float>(x);
  float fy = clampTo<float>(y);
  float fwidth = clampTo<float>(width);
  float fheight = clampTo<float>(height);

  SkRect rect = SkRect::MakeXYWH(fx, fy, fwidth, fheight);
  FloatRect bounds = rect;
  InflateStrokeRect(bounds);
  Draw([&rect](cc::PaintCanvas* c, const PaintFlags* flags)  // draw lambda
       { StrokeRectOnCanvas(rect, c, flags); },
       [](const SkIRect& clip_bounds)  // overdraw test lambda
       { return false; },
       bounds, CanvasRenderingContext2DState::kStrokePaintType);
}

void BaseRenderingContext2D::ClipInternal(const Path& path,
                                          const String& winding_rule_string) {
  cc::PaintCanvas* c = DrawingCanvas();
  if (!c) {
    return;
  }
  if (!GetState().IsTransformInvertible()) {
    return;
  }

  SkPath sk_path = path.GetSkPath();
  sk_path.setFillType(ParseWinding(winding_rule_string));
  ModifiableState().ClipPath(sk_path, clip_antialiasing_);
  c->clipPath(sk_path, SkClipOp::kIntersect,
              clip_antialiasing_ == kAntiAliased);
}

void BaseRenderingContext2D::clip(const String& winding_rule_string) {
  ClipInternal(path_, winding_rule_string);
}

void BaseRenderingContext2D::clip(Path2D* dom_path,
                                  const String& winding_rule_string) {
  ClipInternal(dom_path->GetPath(), winding_rule_string);
}

bool BaseRenderingContext2D::isPointInPath(const double x,
                                           const double y,
                                           const String& winding_rule_string) {
  return IsPointInPathInternal(path_, x, y, winding_rule_string);
}

bool BaseRenderingContext2D::isPointInPath(Path2D* dom_path,
                                           const double x,
                                           const double y,
                                           const String& winding_rule_string) {
  return IsPointInPathInternal(dom_path->GetPath(), x, y, winding_rule_string);
}

bool BaseRenderingContext2D::IsPointInPathInternal(
    const Path& path,
    const double x,
    const double y,
    const String& winding_rule_string) {
  cc::PaintCanvas* c = DrawingCanvas();
  if (!c)
    return false;
  if (!GetState().IsTransformInvertible())
    return false;

  if (!std::isfinite(x) || !std::isfinite(y))
    return false;
  FloatPoint point(clampTo<float>(x), clampTo<float>(y));
  AffineTransform ctm = GetState().Transform();
  FloatPoint transformed_point = ctm.Inverse().MapPoint(point);

  return path.Contains(transformed_point,
                       SkFillTypeToWindRule(ParseWinding(winding_rule_string)));
}

bool BaseRenderingContext2D::isPointInStroke(const double x, const double y) {
  return IsPointInStrokeInternal(path_, x, y);
}

bool BaseRenderingContext2D::isPointInStroke(Path2D* dom_path,
                                             const double x,
                                             const double y) {
  return IsPointInStrokeInternal(dom_path->GetPath(), x, y);
}

bool BaseRenderingContext2D::IsPointInStrokeInternal(const Path& path,
                                                     const double x,
                                                     const double y) {
  cc::PaintCanvas* c = DrawingCanvas();
  if (!c)
    return false;
  if (!GetState().IsTransformInvertible())
    return false;

  if (!std::isfinite(x) || !std::isfinite(y))
    return false;
  FloatPoint point(clampTo<float>(x), clampTo<float>(y));
  AffineTransform ctm = GetState().Transform();
  FloatPoint transformed_point = ctm.Inverse().MapPoint(point);

  StrokeData stroke_data;
  stroke_data.SetThickness(GetState().LineWidth());
  stroke_data.SetLineCap(GetState().GetLineCap());
  stroke_data.SetLineJoin(GetState().GetLineJoin());
  stroke_data.SetMiterLimit(GetState().MiterLimit());
  Vector<float> line_dash(GetState().LineDash().size());
  std::copy(GetState().LineDash().begin(), GetState().LineDash().end(),
            line_dash.begin());
  stroke_data.SetLineDash(line_dash, GetState().LineDashOffset());
  return path.StrokeContains(transformed_point, stroke_data);
}

void BaseRenderingContext2D::clearRect(double x,
                                       double y,
                                       double width,
                                       double height) {
  usage_counters_.num_clear_rect_calls++;

  if (!ValidateRectForCanvas(x, y, width, height))
    return;

  cc::PaintCanvas* c = DrawingCanvas();
  if (!c)
    return;
  if (!GetState().IsTransformInvertible())
    return;

  SkIRect clip_bounds;
  if (!c->getDeviceClipBounds(&clip_bounds))
    return;

  PaintFlags clear_flags;
  clear_flags.setBlendMode(SkBlendMode::kClear);
  clear_flags.setStyle(PaintFlags::kFill_Style);

  // clamp to float to avoid float cast overflow when used as SkScalar
  float fx = clampTo<float>(x);
  float fy = clampTo<float>(y);
  float fwidth = clampTo<float>(width);
  float fheight = clampTo<float>(height);

  FloatRect rect(fx, fy, fwidth, fheight);
  if (RectContainsTransformedRect(rect, clip_bounds)) {
    CheckOverdraw(rect, &clear_flags, CanvasRenderingContext2DState::kNoImage,
                  kClipFill);
    if (DrawingCanvas())
      DrawingCanvas()->drawRect(rect, clear_flags);
    DidDraw(clip_bounds);
  } else {
    SkIRect dirty_rect;
    if (ComputeDirtyRect(rect, clip_bounds, &dirty_rect)) {
      c->drawRect(rect, clear_flags);
      DidDraw(dirty_rect);
    }
  }
}

static inline FloatRect NormalizeRect(const FloatRect& rect) {
  return FloatRect(std::min(rect.X(), rect.MaxX()),
                   std::min(rect.Y(), rect.MaxY()),
                   std::max(rect.Width(), -rect.Width()),
                   std::max(rect.Height(), -rect.Height()));
}

static inline void ClipRectsToImageRect(const FloatRect& image_rect,
                                        FloatRect* src_rect,
                                        FloatRect* dst_rect) {
  if (image_rect.Contains(*src_rect))
    return;

  // Compute the src to dst transform
  FloatSize scale(dst_rect->Size().Width() / src_rect->Size().Width(),
                  dst_rect->Size().Height() / src_rect->Size().Height());
  FloatPoint scaled_src_location = src_rect->Location();
  scaled_src_location.Scale(scale.Width(), scale.Height());
  FloatSize offset = dst_rect->Location() - scaled_src_location;

  src_rect->Intersect(image_rect);

  // To clip the destination rectangle in the same proportion, transform the
  // clipped src rect
  *dst_rect = *src_rect;
  dst_rect->Scale(scale.Width(), scale.Height());
  dst_rect->Move(offset);
}

static inline CanvasImageSource* ToImageSourceInternal(
    const CanvasImageSourceUnion& value,
    ExceptionState& exception_state) {
  if (value.IsCSSImageValue()) {
    return value.GetAsCSSImageValue();
  }
  if (value.IsHTMLImageElement())
    return value.GetAsHTMLImageElement();
  if (value.IsHTMLVideoElement()) {
    HTMLVideoElement* video = value.GetAsHTMLVideoElement();
    video->VideoWillBeDrawnToCanvas();
    return video;
  }
  if (value.IsSVGImageElement())
    return value.GetAsSVGImageElement();
  if (value.IsHTMLCanvasElement()) {
    if (static_cast<HTMLCanvasElement*>(value.GetAsHTMLCanvasElement())
            ->Size()
            .IsEmpty()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          String::Format("The image argument is a canvas element with a width "
                         "or height of 0."));
      return nullptr;
    }
    return value.GetAsHTMLCanvasElement();
  }
  if (value.IsImageBitmap()) {
    if (static_cast<ImageBitmap*>(value.GetAsImageBitmap())->IsNeutered()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          String::Format("The image source is detached"));
      return nullptr;
    }
    return value.GetAsImageBitmap();
  }
  if (value.IsOffscreenCanvas()) {
    if (static_cast<OffscreenCanvas*>(value.GetAsOffscreenCanvas())
            ->IsNeutered()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          String::Format("The image source is detached"));
      return nullptr;
    }
    if (static_cast<OffscreenCanvas*>(value.GetAsOffscreenCanvas())
            ->Size()
            .IsEmpty()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          String::Format("The image argument is an OffscreenCanvas element "
                         "with a width or height of 0."));
      return nullptr;
    }
    return value.GetAsOffscreenCanvas();
  }
  NOTREACHED();
  return nullptr;
}

void BaseRenderingContext2D::drawImage(
    ScriptState* script_state,
    const CanvasImageSourceUnion& image_source,
    double x,
    double y,
    ExceptionState& exception_state) {
  CanvasImageSource* image_source_internal =
      ToImageSourceInternal(image_source, exception_state);
  if (!image_source_internal)
    return;
  FloatSize default_object_size(Width(), Height());
  FloatSize source_rect_size =
      image_source_internal->ElementSize(default_object_size);
  FloatSize dest_rect_size =
      image_source_internal->DefaultDestinationSize(default_object_size);
  drawImage(script_state, image_source_internal, 0, 0, source_rect_size.Width(),
            source_rect_size.Height(), x, y, dest_rect_size.Width(),
            dest_rect_size.Height(), exception_state);
}

void BaseRenderingContext2D::drawImage(
    ScriptState* script_state,
    const CanvasImageSourceUnion& image_source,
    double x,
    double y,
    double width,
    double height,
    ExceptionState& exception_state) {
  CanvasImageSource* image_source_internal =
      ToImageSourceInternal(image_source, exception_state);
  if (!image_source_internal)
    return;
  FloatSize default_object_size(this->Width(), this->Height());
  FloatSize source_rect_size =
      image_source_internal->ElementSize(default_object_size);
  drawImage(script_state, image_source_internal, 0, 0, source_rect_size.Width(),
            source_rect_size.Height(), x, y, width, height, exception_state);
}

void BaseRenderingContext2D::drawImage(
    ScriptState* script_state,
    const CanvasImageSourceUnion& image_source,
    double sx,
    double sy,
    double sw,
    double sh,
    double dx,
    double dy,
    double dw,
    double dh,
    ExceptionState& exception_state) {
  CanvasImageSource* image_source_internal =
      ToImageSourceInternal(image_source, exception_state);
  if (!image_source_internal)
    return;
  drawImage(script_state, image_source_internal, sx, sy, sw, sh, dx, dy, dw, dh,
            exception_state);
}

bool BaseRenderingContext2D::ShouldDrawImageAntialiased(
    const FloatRect& dest_rect) const {
  if (!GetState().ShouldAntialias())
    return false;
  cc::PaintCanvas* c = DrawingCanvas();
  DCHECK(c);

  const SkMatrix& ctm = c->getTotalMatrix();
  // Don't disable anti-aliasing if we're rotated or skewed.
  if (!ctm.rectStaysRect())
    return true;
  // Check if the dimensions of the destination are "small" (less than one
  // device pixel). To prevent sudden drop-outs. Since we know that
  // kRectStaysRect_Mask is set, the matrix either has scale and no skew or
  // vice versa. We can query the kAffine_Mask flag to determine which case
  // it is.
  // FIXME: This queries the CTM while drawing, which is generally
  // discouraged. Always drawing with AA can negatively impact performance
  // though - that's why it's not always on.
  SkScalar width_expansion, height_expansion;
  if (ctm.getType() & SkMatrix::kAffine_Mask) {
    width_expansion = ctm[SkMatrix::kMSkewY];
    height_expansion = ctm[SkMatrix::kMSkewX];
  } else {
    width_expansion = ctm[SkMatrix::kMScaleX];
    height_expansion = ctm[SkMatrix::kMScaleY];
  }
  return dest_rect.Width() * fabs(width_expansion) < 1 ||
         dest_rect.Height() * fabs(height_expansion) < 1;
}

void BaseRenderingContext2D::DrawImageInternal(cc::PaintCanvas* c,
                                               CanvasImageSource* image_source,
                                               Image* image,
                                               const FloatRect& src_rect,
                                               const FloatRect& dst_rect,
                                               const PaintFlags* flags) {
  int initial_save_count = c->getSaveCount();
  PaintFlags image_flags = *flags;

  if (flags->getImageFilter()) {
    SkMatrix ctm = c->getTotalMatrix();
    SkMatrix inv_ctm;
    if (!ctm.invert(&inv_ctm)) {
      // There is an earlier check for invertibility, but the arithmetic
      // in AffineTransform is not exactly identical, so it is possible
      // for SkMatrix to find the transform to be non-invertible at this stage.
      // crbug.com/504687
      return;
    }
    c->save();
    c->concat(inv_ctm);
    SkRect bounds = dst_rect;
    ctm.mapRect(&bounds);
    PaintFlags layer_flags;
    layer_flags.setBlendMode(flags->getBlendMode());
    layer_flags.setImageFilter(flags->getImageFilter());

    c->saveLayer(&bounds, &layer_flags);
    c->concat(ctm);
    image_flags.setBlendMode(SkBlendMode::kSrcOver);
    image_flags.setImageFilter(nullptr);
  }

  if (!image_source->IsVideoElement()) {
    image_flags.setAntiAlias(ShouldDrawImageAntialiased(dst_rect));
    image->Draw(c, image_flags, dst_rect, src_rect,
                kDoNotRespectImageOrientation,
                Image::kDoNotClampImageToSourceRect, Image::kSyncDecode);
  } else {
    c->save();
    c->clipRect(dst_rect);
    c->translate(dst_rect.X(), dst_rect.Y());
    c->scale(dst_rect.Width() / src_rect.Width(),
             dst_rect.Height() / src_rect.Height());
    c->translate(-src_rect.X(), -src_rect.Y());
    HTMLVideoElement* video = static_cast<HTMLVideoElement*>(image_source);
    video->PaintCurrentFrame(
        c,
        IntRect(IntPoint(), IntSize(video->videoWidth(), video->videoHeight())),
        &image_flags);
  }

  c->restoreToCount(initial_save_count);
}

bool ShouldDisableDeferral(CanvasImageSource* image_source,
                           DisableDeferralReason* reason) {
  DCHECK(reason);
  DCHECK_EQ(*reason, kDisableDeferralReasonUnknown);

  if (image_source->IsVideoElement()) {
    *reason = kDisableDeferralReasonDrawImageOfVideo;
    return true;
  }
  if (image_source->IsCanvasElement()) {
    HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(image_source);
    if (canvas->IsAnimated2d()) {
      *reason = kDisableDeferralReasonDrawImageOfAnimated2dCanvas;
      return true;
    }
  }
  return false;
}

void BaseRenderingContext2D::SetOriginTaintedByContent() {
  SetOriginTainted();
  origin_tainted_by_content_ = true;
  for (auto& state : state_stack_)
    state->ClearResolvedFilter();
}

void BaseRenderingContext2D::drawImage(ScriptState* script_state,
                                       CanvasImageSource* image_source,
                                       double sx,
                                       double sy,
                                       double sw,
                                       double sh,
                                       double dx,
                                       double dy,
                                       double dw,
                                       double dh,
                                       ExceptionState& exception_state) {
  if (!DrawingCanvas())
    return;

  base::TimeTicks start_time = base::TimeTicks::Now();

  scoped_refptr<Image> image;
  FloatSize default_object_size(Width(), Height());
  SourceImageStatus source_image_status = kInvalidSourceImageStatus;
  if (!image_source->IsVideoElement()) {
    AccelerationHint hint =
        IsAccelerated() ? kPreferAcceleration : kPreferNoAcceleration;
    image = image_source->GetSourceImageForCanvas(&source_image_status, hint,
                                                  default_object_size);
    if (source_image_status == kUndecodableSourceImageStatus) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "The HTMLImageElement provided is in the 'broken' state.");
    }
    if (!image || !image->width() || !image->height())
      return;
  } else {
    if (!static_cast<HTMLVideoElement*>(image_source)->HasAvailableVideoFrame())
      return;
  }

  if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dw) ||
      !std::isfinite(dh) || !std::isfinite(sx) || !std::isfinite(sy) ||
      !std::isfinite(sw) || !std::isfinite(sh) || !dw || !dh || !sw || !sh)
    return;

  // clamp to float to avoid float cast overflow when used as SkScalar
  float fsx = clampTo<float>(sx);
  float fsy = clampTo<float>(sy);
  float fsw = clampTo<float>(sw);
  float fsh = clampTo<float>(sh);
  float fdx = clampTo<float>(dx);
  float fdy = clampTo<float>(dy);
  float fdw = clampTo<float>(dw);
  float fdh = clampTo<float>(dh);

  FloatRect src_rect = NormalizeRect(FloatRect(fsx, fsy, fsw, fsh));
  FloatRect dst_rect = NormalizeRect(FloatRect(fdx, fdy, fdw, fdh));
  FloatSize image_size = image_source->ElementSize(default_object_size);

  ClipRectsToImageRect(FloatRect(FloatPoint(), image_size), &src_rect,
                       &dst_rect);

  image_source->AdjustDrawRects(&src_rect, &dst_rect);

  if (src_rect.IsEmpty())
    return;

  DisableDeferralReason reason = kDisableDeferralReasonUnknown;
  if (ShouldDisableDeferral(image_source, &reason))
    DisableDeferral(reason);
  else if (image->IsTextureBacked())
    DisableDeferral(kDisableDeferralDrawImageWithTextureBackedSourceImage);

  ValidateStateStack();

  WillDrawImage(image_source);

  ValidateStateStack();

  // Heuristic for disabling acceleration based on anticipated texture upload
  // overhead.
  // See comments in canvas_heuristic_parameters.h for explanation.
  if (CanCreateCanvas2dResourceProvider() && IsAccelerated() &&
      !image_source->IsAccelerated()) {
    float src_area = src_rect.Width() * src_rect.Height();
    if (src_area >
        canvas_heuristic_parameters::kDrawImageTextureUploadHardSizeLimit) {
      this->DisableAcceleration();
    } else if (src_area > canvas_heuristic_parameters::
                              kDrawImageTextureUploadSoftSizeLimit) {
      SkRect bounds = dst_rect;
      SkMatrix ctm = DrawingCanvas()->getTotalMatrix();
      ctm.mapRect(&bounds);
      float dst_area = dst_rect.Width() * dst_rect.Height();
      if (src_area >
          dst_area * canvas_heuristic_parameters::
                         kDrawImageTextureUploadSoftSizeLimitScaleThreshold) {
        this->DisableAcceleration();
      }
    }
  }

  ValidateStateStack();

  if (!origin_tainted_by_content_ &&
      WouldTaintOrigin(image_source, ExecutionContext::From(script_state)))
    SetOriginTaintedByContent();

  Draw(
      [this, &image_source, &image, &src_rect, dst_rect](
          cc::PaintCanvas* c, const PaintFlags* flags)  // draw lambda
      {
        DrawImageInternal(c, image_source, image.get(), src_rect, dst_rect,
                          flags);
      },
      [this, &dst_rect](const SkIRect& clip_bounds)  // overdraw test lambda
      { return RectContainsTransformedRect(dst_rect, clip_bounds); },
      dst_rect, CanvasRenderingContext2DState::kImagePaintType,
      image_source->IsOpaque()
          ? CanvasRenderingContext2DState::kOpaqueImage
          : CanvasRenderingContext2DState::kNonOpaqueImage);

  ValidateStateStack();

  if (!IsPaint2D()) {
    std::string histogram_name = "Blink.Canvas.DrawImage.Duration.";
    if (image_source->IsCanvasElement()) {
      histogram_name.append("Canvas.");
    } else if (image_source->IsCSSImageValue()) {
      histogram_name.append("CssImage.");
    } else if (image_source->IsImageElement()) {
      histogram_name.append("ImageElement.");
    } else if (image_source->IsImageBitmap()) {
      histogram_name.append("ImageBitmap.");
    } else if (image_source->IsOffscreenCanvas()) {
      histogram_name.append("OffscreenCanvas.");
    } else if (image_source->IsSVGSource()) {
      histogram_name.append("SVG.");
    } else if (image_source->IsVideoElement()) {
      histogram_name.append("Video.");
    } else {  // Unknown source.
      histogram_name.append("Unknown.");
    }
    histogram_name.append(
        CanCreateCanvas2dResourceProvider() && IsAccelerated() ? "GPU" : "CPU");
    base::TimeDelta elapsed = TimeTicks::Now() - start_time;
    UmaHistogramMicrosecondsTimes(histogram_name, elapsed);
  }
}

void BaseRenderingContext2D::ClearCanvas() {
  FloatRect canvas_rect(0, 0, Width(), Height());
  CheckOverdraw(canvas_rect, nullptr, CanvasRenderingContext2DState::kNoImage,
                kClipFill);
  cc::PaintCanvas* c = DrawingCanvas();
  if (c)
    c->clear(HasAlpha() ? SK_ColorTRANSPARENT : SK_ColorBLACK);
}

bool BaseRenderingContext2D::RectContainsTransformedRect(
    const FloatRect& rect,
    const SkIRect& transformed_rect) const {
  FloatQuad quad(rect);
  FloatQuad transformed_quad(
      FloatRect(transformed_rect.x(), transformed_rect.y(),
                transformed_rect.width(), transformed_rect.height()));
  return GetState().Transform().MapQuad(quad).ContainsQuad(transformed_quad);
}

CanvasGradient* BaseRenderingContext2D::createLinearGradient(double x0,
                                                             double y0,
                                                             double x1,
                                                             double y1) {
  if (!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(x1) ||
      !std::isfinite(y1))
    return nullptr;

  // clamp to float to avoid float cast overflow
  float fx0 = clampTo<float>(x0);
  float fy0 = clampTo<float>(y0);
  float fx1 = clampTo<float>(x1);
  float fy1 = clampTo<float>(y1);

  CanvasGradient* gradient =
      CanvasGradient::Create(FloatPoint(fx0, fy0), FloatPoint(fx1, fy1));
  return gradient;
}

CanvasGradient* BaseRenderingContext2D::createRadialGradient(
    double x0,
    double y0,
    double r0,
    double x1,
    double y1,
    double r1,
    ExceptionState& exception_state) {
  if (r0 < 0 || r1 < 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        String::Format("The %s provided is less than 0.",
                       r0 < 0 ? "r0" : "r1"));
    return nullptr;
  }

  if (!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(r0) ||
      !std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(r1))
    return nullptr;

  // clamp to float to avoid float cast overflow
  float fx0 = clampTo<float>(x0);
  float fy0 = clampTo<float>(y0);
  float fr0 = clampTo<float>(r0);
  float fx1 = clampTo<float>(x1);
  float fy1 = clampTo<float>(y1);
  float fr1 = clampTo<float>(r1);

  CanvasGradient* gradient = CanvasGradient::Create(FloatPoint(fx0, fy0), fr0,
                                                    FloatPoint(fx1, fy1), fr1);
  return gradient;
}

CanvasPattern* BaseRenderingContext2D::createPattern(
    ScriptState* script_state,
    const CanvasImageSourceUnion& image_source,
    const String& repetition_type,
    ExceptionState& exception_state) {
  CanvasImageSource* image_source_internal =
      ToImageSourceInternal(image_source, exception_state);
  if (!image_source_internal) {
    return nullptr;
  }

  return createPattern(script_state, image_source_internal, repetition_type,
                       exception_state);
}

CanvasPattern* BaseRenderingContext2D::createPattern(
    ScriptState* script_state,
    CanvasImageSource* image_source,
    const String& repetition_type,
    ExceptionState& exception_state) {
  if (!image_source) {
    return nullptr;
  }

  Pattern::RepeatMode repeat_mode =
      CanvasPattern::ParseRepetitionType(repetition_type, exception_state);
  if (exception_state.HadException())
    return nullptr;

  SourceImageStatus status;

  FloatSize default_object_size(Width(), Height());
  scoped_refptr<Image> image_for_rendering =
      image_source->GetSourceImageForCanvas(&status, kPreferNoAcceleration,
                                            default_object_size);

  switch (status) {
    case kNormalSourceImageStatus:
      break;
    case kZeroSizeCanvasSourceImageStatus:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          String::Format("The canvas %s is 0.",
                         image_source->ElementSize(default_object_size).Width()
                             ? "height"
                             : "width"));
      return nullptr;
    case kUndecodableSourceImageStatus:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Source image is in the 'broken' state.");
      return nullptr;
    case kInvalidSourceImageStatus:
      image_for_rendering = Image::NullImage();
      break;
    case kIncompleteSourceImageStatus:
      return nullptr;
    default:
      NOTREACHED();
      return nullptr;
  }
  DCHECK(image_for_rendering);

  bool origin_clean =
      !WouldTaintOrigin(image_source, ExecutionContext::From(script_state));

  return CanvasPattern::Create(std::move(image_for_rendering), repeat_mode,
                               origin_clean);
}

bool BaseRenderingContext2D::ComputeDirtyRect(const FloatRect& local_rect,
                                              SkIRect* dirty_rect) {
  SkIRect clip_bounds;
  if (!DrawingCanvas()->getDeviceClipBounds(&clip_bounds))
    return false;
  return ComputeDirtyRect(local_rect, clip_bounds, dirty_rect);
}

bool BaseRenderingContext2D::ComputeDirtyRect(
    const FloatRect& local_rect,
    const SkIRect& transformed_clip_bounds,
    SkIRect* dirty_rect) {
  FloatRect canvas_rect = GetState().Transform().MapRect(local_rect);

  if (AlphaChannel(GetState().ShadowColor())) {
    FloatRect shadow_rect(canvas_rect);
    shadow_rect.Move(GetState().ShadowOffset());
    shadow_rect.Inflate(clampTo<float>(GetState().ShadowBlur()));
    canvas_rect.Unite(shadow_rect);
  }

  SkIRect canvas_i_rect;
  static_cast<SkRect>(canvas_rect).roundOut(&canvas_i_rect);
  if (!canvas_i_rect.intersect(transformed_clip_bounds))
    return false;

  if (dirty_rect)
    *dirty_rect = canvas_i_rect;

  return true;
}

ImageDataColorSettings
BaseRenderingContext2D::GetColorSettingsAsImageDataColorSettings() const {
  ImageDataColorSettings color_settings;
  color_settings.setColorSpace(ColorSpaceAsString());
  if (PixelFormat() == kF16CanvasPixelFormat)
    color_settings.setStorageFormat(kFloat32ArrayStorageFormatName);
  return color_settings;
}

ImageData* BaseRenderingContext2D::createImageData(
    ImageData* image_data,
    ExceptionState& exception_state) const {
  ImageData* result = nullptr;
  ImageDataColorSettings color_settings =
      GetColorSettingsAsImageDataColorSettings();
  result = ImageData::Create(image_data->Size(), &color_settings);
  if (!result)
    exception_state.ThrowRangeError("Out of memory at ImageData creation");
  return result;
}

ImageData* BaseRenderingContext2D::createImageData(
    int sw,
    int sh,
    ExceptionState& exception_state) const {
  if (!sw || !sh) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        String::Format("The source %s is 0.", sw ? "height" : "width"));
    return nullptr;
  }

  IntSize size(abs(sw), abs(sh));
  ImageData* result = nullptr;
  ImageDataColorSettings color_settings =
      GetColorSettingsAsImageDataColorSettings();
  result = ImageData::Create(size, &color_settings);

  if (!result)
    exception_state.ThrowRangeError("Out of memory at ImageData creation");
  return result;
}

ImageData* BaseRenderingContext2D::createImageData(
    unsigned width,
    unsigned height,
    ImageDataColorSettings& color_settings,
    ExceptionState& exception_state) const {
  return ImageData::CreateImageData(width, height, color_settings,
                                    exception_state);
}

ImageData* BaseRenderingContext2D::createImageData(
    ImageDataArray& data_array,
    unsigned width,
    unsigned height,
    ExceptionState& exception_state) const {
  ImageDataColorSettings color_settings;
  return ImageData::CreateImageData(data_array, width, height, color_settings,
                                    exception_state);
}

ImageData* BaseRenderingContext2D::createImageData(
    ImageDataArray& data_array,
    unsigned width,
    unsigned height,
    ImageDataColorSettings& color_settings,
    ExceptionState& exception_state) const {
  return ImageData::CreateImageData(data_array, width, height, color_settings,
                                    exception_state);
}

ImageData* BaseRenderingContext2D::getImageData(
    int sx,
    int sy,
    int sw,
    int sh,
    ExceptionState& exception_state) {
  if (!base::CheckMul(sw, sh).IsValid<int>()) {
    exception_state.ThrowRangeError("Out of memory at ImageData creation");
    return nullptr;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();

  usage_counters_.num_get_image_data_calls++;
  usage_counters_.area_get_image_data_calls += sw * sh;
  if (!OriginClean()) {
    exception_state.ThrowSecurityError(
        "The canvas has been tainted by cross-origin data.");
  } else if (!sw || !sh) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        String::Format("The source %s is 0.", sw ? "height" : "width"));
  }

  if (exception_state.HadException())
    return nullptr;

  if (sw < 0) {
    if (!base::CheckAdd(sx, sw).IsValid<int>()) {
      exception_state.ThrowRangeError("Out of memory at ImageData creation");
      return nullptr;
    }
    sx += sw;
    sw = -sw;
  }
  if (sh < 0) {
    if (!base::CheckAdd(sy, sh).IsValid<int>()) {
      exception_state.ThrowRangeError("Out of memory at ImageData creation");
      return nullptr;
    }
    sy += sh;
    sh = -sh;
  }

  if (!base::CheckAdd(sx, sw).IsValid<int>() ||
      !base::CheckAdd(sy, sh).IsValid<int>()) {
    exception_state.ThrowRangeError("Out of memory at ImageData creation");
    return nullptr;
  }

  IntRect image_data_rect(sx, sy, sw, sh);
  bool hasResourceProvider = CanCreateCanvas2dResourceProvider();
  ImageDataColorSettings color_settings =
      GetColorSettingsAsImageDataColorSettings();
  if (!hasResourceProvider || isContextLost()) {
    ImageData* result =
        ImageData::Create(image_data_rect.Size(), &color_settings);
    if (!result)
      exception_state.ThrowRangeError("Out of memory at ImageData creation");
    return result;
  }

  WTF::ArrayBufferContents contents;

  const CanvasColorParams& color_params = ColorParams();
  scoped_refptr<StaticBitmapImage> snapshot = GetImage(kPreferNoAcceleration);

  if (!StaticBitmapImage::ConvertToArrayBufferContents(
          snapshot, contents, image_data_rect, color_params, IsAccelerated())) {
    exception_state.ThrowRangeError("Out of memory at ImageData creation");
    return nullptr;
  }

  if (!!snapshot) {
    // If source image is not null, the ConvertToArrayBufferContents function
    // must have invoked SkImage::readPixels.
    DidInvokeGPUReadbackInCurrentFrame();
  }

  NeedsFinalizeFrame();

  // Convert pixels to proper storage format if needed
  if (PixelFormat() != kRGBA8CanvasPixelFormat) {
    ImageDataStorageFormat storage_format =
        ImageData::GetImageDataStorageFormat(color_settings.storageFormat());
    DOMArrayBufferView* array_buffer_view =
        ImageData::ConvertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
            contents, PixelFormat(), storage_format);
    return ImageData::Create(image_data_rect.Size(),
                             NotShared<DOMArrayBufferView>(array_buffer_view),
                             &color_settings);
  }
  DOMArrayBuffer* array_buffer = DOMArrayBuffer::Create(contents);

  ImageData* imageData = ImageData::Create(
      image_data_rect.Size(),
      NotShared<DOMUint8ClampedArray>(DOMUint8ClampedArray::Create(
          array_buffer, 0, array_buffer->ByteLength())),
      &color_settings);

  if (!IsPaint2D()) {
    int scaled_time = getScaledElapsedTime(
        image_data_rect.Width(), image_data_rect.Height(), start_time);
    if (CanCreateCanvas2dResourceProvider() && IsAccelerated()) {
      UMA_HISTOGRAM_COUNTS_1000("Blink.Canvas.GetImageDataScaledDuration.GPU",
                                scaled_time);
    } else {
      UMA_HISTOGRAM_COUNTS_1000("Blink.Canvas.GetImageDataScaledDuration.CPU",
                                scaled_time);
    }
  }

  return imageData;
}

int BaseRenderingContext2D::getScaledElapsedTime(int width,
                                                 int height,
                                                 base::TimeTicks start_time) {
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  float sqrt_pixels = std::sqrt(width * height);
  return elapsed_time.InMicrosecondsF() * 10.0f /
         (sqrt_pixels == 0 ? 1 : sqrt_pixels);
}

void BaseRenderingContext2D::putImageData(ImageData* data,
                                          int dx,
                                          int dy,
                                          ExceptionState& exception_state) {
  putImageData(data, dx, dy, 0, 0, data->width(), data->height(),
               exception_state);
}

void BaseRenderingContext2D::putImageData(ImageData* data,
                                          int dx,
                                          int dy,
                                          int dirty_x,
                                          int dirty_y,
                                          int dirty_width,
                                          int dirty_height,
                                          ExceptionState& exception_state) {
  if (!base::CheckMul(dirty_width, dirty_height).IsValid<int>()) {
    return;
  }
  base::TimeTicks start_time = base::TimeTicks::Now();
  usage_counters_.num_put_image_data_calls++;
  usage_counters_.area_put_image_data_calls += dirty_width * dirty_height;

  if (data->BufferBase()->IsNeutered()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The source data has been neutered.");
    return;
  }

  bool hasResourceProvider = CanCreateCanvas2dResourceProvider();
  if (!hasResourceProvider)
    return;

  if (dirty_width < 0) {
    dirty_x += dirty_width;
    dirty_width = -dirty_width;
  }

  if (dirty_height < 0) {
    dirty_y += dirty_height;
    dirty_height = -dirty_height;
  }

  IntRect dest_rect(dirty_x, dirty_y, dirty_width, dirty_height);
  dest_rect.Intersect(IntRect(0, 0, data->width(), data->height()));
  IntSize dest_offset(static_cast<int>(dx), static_cast<int>(dy));
  dest_rect.Move(dest_offset);
  dest_rect.Intersect(IntRect(0, 0, Width(), Height()));
  if (dest_rect.IsEmpty())
    return;

  IntRect source_rect(dest_rect);
  source_rect.Move(-dest_offset);

  CheckOverdraw(dest_rect, nullptr, CanvasRenderingContext2DState::kNoImage,
                kUntransformedUnclippedFill);

  // Color / format convert ImageData to context 2D settings if needed. Color /
  // format conversion is not needed only if context 2D and ImageData are both
  // in sRGB color space and use uint8 pixel storage format. We use RGBA pixel
  // order for both ImageData and CanvasResourceProvider, therefore no
  // additional swizzling is needed.
  CanvasColorParams data_color_params = data->GetCanvasColorParams();
  CanvasColorParams context_color_params =
      CanvasColorParams(ColorParams().ColorSpace(), PixelFormat(), kNonOpaque);
  if (data_color_params.NeedsColorConversion(context_color_params) ||
      PixelFormat() == kF16CanvasPixelFormat) {
    size_t data_length =
        data->Size().Area() * context_color_params.BytesPerPixel();
    std::unique_ptr<uint8_t[]> converted_pixels(new uint8_t[data_length]);
    if (data->ImageDataInCanvasColorSettings(
            ColorParams().ColorSpace(), PixelFormat(), converted_pixels.get(),
            kRGBAColorType)) {
      PutByteArray(converted_pixels.get(),
                   IntSize(data->width(), data->height()), source_rect,
                   IntPoint(dest_offset));
    }
  } else {
    PutByteArray(data->data()->Data(), IntSize(data->width(), data->height()),
                 source_rect, IntPoint(dest_offset));
  }

  if (!IsPaint2D()) {
    int scaled_time =
        getScaledElapsedTime(dest_rect.Width(), dest_rect.Height(), start_time);
    if (CanCreateCanvas2dResourceProvider() && IsAccelerated()) {
      UMA_HISTOGRAM_COUNTS_1000("Blink.Canvas.PutImageDataScaledDuration.GPU",
                                scaled_time);
    } else {
      UMA_HISTOGRAM_COUNTS_1000("Blink.Canvas.PutImageDataScaledDuration.CPU",
                                scaled_time);
    }
  }

  DidDraw(dest_rect);
}

void BaseRenderingContext2D::PutByteArray(const unsigned char* source,
                                          const IntSize& source_size,
                                          const IntRect& source_rect,
                                          const IntPoint& dest_point) {
  if (!IsCanvas2DBufferValid())
    return;
  uint8_t bytes_per_pixel = ColorParams().BytesPerPixel();

  DCHECK_GT(source_rect.Width(), 0);
  DCHECK_GT(source_rect.Height(), 0);

  int origin_x = source_rect.X();
  int dest_x = dest_point.X() + source_rect.X();
  DCHECK_GE(dest_x, 0);
  DCHECK_LT(dest_x, Width());
  DCHECK_GE(origin_x, 0);
  DCHECK_LT(origin_x, source_rect.MaxX());

  int origin_y = source_rect.Y();
  int dest_y = dest_point.Y() + source_rect.Y();
  DCHECK_GE(dest_y, 0);
  DCHECK_LT(dest_y, Height());
  DCHECK_GE(origin_y, 0);
  DCHECK_LT(origin_y, source_rect.MaxY());

  const size_t src_bytes_per_row = bytes_per_pixel * source_size.Width();
  const void* src_addr =
      source + origin_y * src_bytes_per_row + origin_x * bytes_per_pixel;

  SkAlphaType alpha_type;
  if (kOpaque == ColorParams().GetOpacityMode()) {
    // If the surface is opaque, tell it that we are writing opaque
    // pixels.  Writing non-opaque pixels to opaque is undefined in
    // Skia.  There is some discussion about whether it should be
    // defined in skbug.com/6157.  For now, we can get the desired
    // behavior (memcpy) by pretending the write is opaque.
    alpha_type = kOpaque_SkAlphaType;
  } else {
    alpha_type = kUnpremul_SkAlphaType;
  }

  SkImageInfo info;
  if (ColorParams().GetSkColorSpaceForSkSurfaces()) {
    info = SkImageInfo::Make(source_rect.Width(), source_rect.Height(),
                             ColorParams().GetSkColorType(), alpha_type,
                             ColorParams().GetSkColorSpaceForSkSurfaces());
    if (info.colorType() == kN32_SkColorType)
      info = info.makeColorType(kRGBA_8888_SkColorType);
  } else {
    info = SkImageInfo::Make(source_rect.Width(), source_rect.Height(),
                             kRGBA_8888_SkColorType, alpha_type);
  }
  WritePixels(info, src_addr, src_bytes_per_row, dest_x, dest_y);
}

void BaseRenderingContext2D::InflateStrokeRect(FloatRect& rect) const {
  // Fast approximation of the stroke's bounding rect.
  // This yields a slightly oversized rect but is very fast
  // compared to Path::strokeBoundingRect().
  static const double kRoot2 = sqrtf(2);
  double delta = GetState().LineWidth() / 2;
  if (GetState().GetLineJoin() == kMiterJoin)
    delta *= GetState().MiterLimit();
  else if (GetState().GetLineCap() == kSquareCap)
    delta *= kRoot2;

  rect.Inflate(clampTo<float>(delta));
}

bool BaseRenderingContext2D::imageSmoothingEnabled() const {
  return GetState().ImageSmoothingEnabled();
}

void BaseRenderingContext2D::setImageSmoothingEnabled(bool enabled) {
  if (enabled == GetState().ImageSmoothingEnabled())
    return;

  ModifiableState().SetImageSmoothingEnabled(enabled);
}

String BaseRenderingContext2D::imageSmoothingQuality() const {
  return GetState().ImageSmoothingQuality();
}

void BaseRenderingContext2D::setImageSmoothingQuality(const String& quality) {
  if (quality == GetState().ImageSmoothingQuality())
    return;

  ModifiableState().SetImageSmoothingQuality(quality);
}

void BaseRenderingContext2D::CheckOverdraw(
    const SkRect& rect,
    const PaintFlags* flags,
    CanvasRenderingContext2DState::ImageType image_type,
    DrawType draw_type) {
  cc::PaintCanvas* c = DrawingCanvas();
  if (!c)
    return;

  SkRect device_rect;
  if (draw_type == kUntransformedUnclippedFill) {
    device_rect = rect;
  } else {
    DCHECK_EQ(draw_type, kClipFill);
    if (GetState().HasComplexClip())
      return;

    SkIRect sk_i_bounds;
    if (!c->getDeviceClipBounds(&sk_i_bounds))
      return;
    device_rect = SkRect::Make(sk_i_bounds);
  }

  const SkImageInfo& image_info = c->imageInfo();
  if (!device_rect.contains(
          SkRect::MakeWH(image_info.width(), image_info.height())))
    return;

  bool is_source_over = true;
  unsigned alpha = 0xFF;
  if (flags) {
    if (flags->getLooper() || flags->getImageFilter() || flags->getMaskFilter())
      return;

    SkBlendMode mode = flags->getBlendMode();
    is_source_over = mode == SkBlendMode::kSrcOver;
    if (!is_source_over && mode != SkBlendMode::kSrc &&
        mode != SkBlendMode::kClear)
      return;  // The code below only knows how to handle Src, SrcOver, and
               // Clear

    alpha = flags->getAlpha();

    if (is_source_over &&
        image_type == CanvasRenderingContext2DState::kNoImage) {
      if (flags->HasShader()) {
        if (flags->ShaderIsOpaque() && alpha == 0xFF)
          WillOverwriteCanvas();
        return;
      }
    }
  }

  if (is_source_over) {
    // With source over, we need to certify that alpha == 0xFF for all pixels
    if (image_type == CanvasRenderingContext2DState::kNonOpaqueImage)
      return;
    if (alpha < 0xFF)
      return;
  }

  WillOverwriteCanvas();
}

float BaseRenderingContext2D::GetFontBaseline(
    const SimpleFontData& font_data) const {
  return TextMetrics::GetFontBaseline(GetState().GetTextBaseline(), font_data);
}

String BaseRenderingContext2D::textAlign() const {
  return TextAlignName(GetState().GetTextAlign());
}

void BaseRenderingContext2D::setTextAlign(const String& s) {
  TextAlign align;
  if (!ParseTextAlign(s, align))
    return;
  if (GetState().GetTextAlign() == align)
    return;
  ModifiableState().SetTextAlign(align);
}

String BaseRenderingContext2D::textBaseline() const {
  return TextBaselineName(GetState().GetTextBaseline());
}

void BaseRenderingContext2D::setTextBaseline(const String& s) {
  TextBaseline baseline;
  if (!ParseTextBaseline(s, baseline))
    return;
  if (GetState().GetTextBaseline() == baseline)
    return;
  ModifiableState().SetTextBaseline(baseline);
}

const BaseRenderingContext2D::UsageCounters&
BaseRenderingContext2D::GetUsage() {
  return usage_counters_;
}

void BaseRenderingContext2D::Trace(blink::Visitor* visitor) {
  visitor->Trace(state_stack_);
}

BaseRenderingContext2D::UsageCounters::UsageCounters()
    : num_draw_calls{0, 0, 0, 0, 0, 0, 0},
      bounding_box_perimeter_draw_calls{0.0f, 0.0f, 0.0f, 0.0f,
                                        0.0f, 0.0f, 0.0f},
      bounding_box_area_draw_calls{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
      bounding_box_area_fill_type{0.0f, 0.0f, 0.0f, 0.0f},
      num_non_convex_fill_path_calls(0),
      non_convex_fill_path_area(0.0f),
      num_radial_gradients(0),
      num_linear_gradients(0),
      num_patterns(0),
      num_draw_with_complex_clips(0),
      num_blurred_shadows(0),
      bounding_box_area_times_shadow_blur_squared(0.0f),
      bounding_box_perimeter_times_shadow_blur_squared(0.0f),
      num_filters(0),
      num_get_image_data_calls(0),
      area_get_image_data_calls(0.0),
      num_put_image_data_calls(0),
      area_put_image_data_calls(0.0),
      num_clear_rect_calls(0),
      num_draw_focus_calls(0),
      num_frames_since_reset(0) {}

}  // namespace blink
