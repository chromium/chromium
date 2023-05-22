// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "base/check_deref.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_canvasfilter_string.h"
#include "third_party/blink/renderer/core/css/cssom/css_color_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/paint/filter_effect_builder.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_color_cache.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_operation_resolver.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_pattern.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d_state.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/path_2d.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/v8_canvas_style.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/string_resource.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

BASE_FEATURE(kDisableCanvasOverdrawOptimization,
             "DisableCanvasOverdrawOptimization",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char BaseRenderingContext2D::kDefaultFont[] = "10px sans-serif";
const char BaseRenderingContext2D::kInheritDirectionString[] = "inherit";
const char BaseRenderingContext2D::kRtlDirectionString[] = "rtl";
const char BaseRenderingContext2D::kLtrDirectionString[] = "ltr";
const char BaseRenderingContext2D::kAutoKerningString[] = "auto";
const char BaseRenderingContext2D::kNormalKerningString[] = "normal";
const char BaseRenderingContext2D::kNoneKerningString[] = "none";
const char BaseRenderingContext2D::kUltraCondensedString[] = "ultra-condensed";
const char BaseRenderingContext2D::kExtraCondensedString[] = "extra-condensed";
const char BaseRenderingContext2D::kCondensedString[] = "condensed";
const char BaseRenderingContext2D::kSemiCondensedString[] = "semi-condensed";
const char BaseRenderingContext2D::kNormalStretchString[] = "normal";
const char BaseRenderingContext2D::kSemiExpandedString[] = "semi-expanded";
const char BaseRenderingContext2D::kExpandedString[] = "expanded";
const char BaseRenderingContext2D::kExtraExpandedString[] = "extra-expanded";
const char BaseRenderingContext2D::kUltraExpandedString[] = "ultra-expanded";
const char BaseRenderingContext2D::kNormalVariantString[] = "normal";
const char BaseRenderingContext2D::kSmallCapsVariantString[] = "small-caps";
const char BaseRenderingContext2D::kAllSmallCapsVariantString[] =
    "all-small-caps";
const char BaseRenderingContext2D::kPetiteVariantString[] = "petite-caps";
const char BaseRenderingContext2D::kAllPetiteVariantString[] =
    "all-petite-caps";
const char BaseRenderingContext2D::kUnicaseVariantString[] = "unicase";
const char BaseRenderingContext2D::kTitlingCapsVariantString[] = "titling-caps";
const char BaseRenderingContext2D::kAutoRendering[] = "auto";
const char BaseRenderingContext2D::kOptimizeSpeedRendering[] = "optimizespeed";
const char BaseRenderingContext2D::kOptimizeLegibilityRendering[] =
    "optimizelegibility";
const char BaseRenderingContext2D::kGeometricPrecisionRendering[] =
    "geometricprecision";

// Dummy overdraw test for ops that do not support overdraw detection
const auto kNoOverdraw = [](const SkIRect& clip_bounds) { return false; };

// After context lost, it waits |kTryRestoreContextInterval| before start the
// restore the context. This wait needs to be long enough to avoid spamming the
// GPU process with retry attempts and short enough to provide decent UX. It's
// currently set to 500ms.
const base::TimeDelta kTryRestoreContextInterval = base::Milliseconds(500);

BaseRenderingContext2D::BaseRenderingContext2D(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : dispatch_context_lost_event_timer_(
          task_runner,
          this,
          &BaseRenderingContext2D::DispatchContextLostEvent),
      dispatch_context_restored_event_timer_(
          task_runner,
          this,
          &BaseRenderingContext2D::DispatchContextRestoredEvent),
      try_restore_context_event_timer_(
          task_runner,
          this,
          &BaseRenderingContext2D::TryRestoreContextEvent),
      clip_antialiasing_(kNotAntiAliased),
      origin_tainted_by_content_(false),
      path2d_use_paint_cache_(
          base::FeatureList::IsEnabled(features::kPath2DPaintCache)
              ? UsePaintCache::kEnabled
              : UsePaintCache::kDisabled) {
  state_stack_.push_back(MakeGarbageCollected<CanvasRenderingContext2DState>());
  color_cache_ = CanvasColorCache::Create();
}

BaseRenderingContext2D::~BaseRenderingContext2D() {
  UMA_HISTOGRAM_CUSTOM_COUNTS("Blink.Canvas.MaximumStateStackDepth",
                              max_state_stack_depth_, 1, 33, 32);
}

void BaseRenderingContext2D::save() {
  if (UNLIKELY(isContextLost())) {
    return;
  }
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSave);
  }

  ValidateStateStack();

  DCHECK_GE(state_stack_.size(), 1u);

  // GetOrCreatePaintCanvas() can call RestoreMatrixClipStack which syncs
  // canvas to state_stack_. Get the canvas before adjusting state_stack_ to
  // ensure canvas is synced prior to adjusting state_stack_.
  cc::PaintCanvas* canvas = GetOrCreatePaintCanvas();

  state_stack_.push_back(MakeGarbageCollected<CanvasRenderingContext2DState>(
      GetState(), CanvasRenderingContext2DState::kDontCopyClipList,
      CanvasRenderingContext2DState::SaveType::kSaveRestore));
  max_state_stack_depth_ =
      std::max(state_stack_.size(), max_state_stack_depth_);

  if (canvas)
    canvas->save();

  ValidateStateStack();
}

void BaseRenderingContext2D::restore() {
  if (UNLIKELY(isContextLost())) {
    return;
  }

  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kRestore);
  }
  ValidateStateStack();
  if (state_stack_.size() <= 1)
    return;

  DCHECK_GT(state_stack_.size(), static_cast<WTF::wtf_size_t>(layer_count_));

  // Verify that the top of the stack was pushed with Save.
  if (RuntimeEnabledFeatures::Canvas2dLayersEnabled() &&
      state_stack_.back()->GetSaveType() !=
          CanvasRenderingContext2DState::SaveType::kSaveRestore) {
    return;
  }

  // Verify that the current state's transform is invertible.
  if (IsTransformInvertible())
    GetModifiablePath().Transform(GetState().GetTransform());

  PopAndRestore();
  ValidateStateStack();
}

void BaseRenderingContext2D::beginLayer(ExecutionContext* execution_context,
                                        const V8CanvasFilterInput* filter_init,
                                        ExceptionState& exception_state) {
  if (UNLIKELY(isContextLost())) {
    return;
  }
  // TODO(crbug.com/1234113): Instrument new canvas APIs.
  identifiability_study_helper_.set_encountered_skipped_ops();

  ValidateStateStack();

  DCHECK_GE(state_stack_.size(), 1u);

  // GetOrCreatePaintCanvas() can call RestoreMatrixClipStack which syncs
  // canvas to state_stack_. Get the canvas before adjusting state_stack_ to
  // ensure canvas is synced prior to adjusting state_stack_.
  cc::PaintCanvas* canvas = GetOrCreatePaintCanvas();
  if (!canvas)
    return;

  ++layer_count_;

  CanvasRenderingContext2DState& state = GetState();
  if (filter_init != nullptr) {
    FilterEffectBuilder filter_effect_builder(
        gfx::RectF(Width(), Height()),
        1.0f);  // Deliberately ignore zoom on the canvas element.
    // Save the layer's filter in the parent state, along with all the other
    // render states impacting the layer. Technically, this is only required so
    // that we could restore the `cc::PaintCanvas` matrix stack (in
    // `RestoreMatrixClipStack`) if a frame is rendered while the layer is
    // opened. The filter can be discarded from the parent state as soon as the
    // layer is closed.
    state.SetLayerFilter(paint_filter_builder::Build(
        filter_effect_builder.BuildFilterEffect(
            CanvasFilterOperationResolver::CreateFilterOperations(
                CHECK_DEREF(filter_init), CHECK_DEREF(execution_context),
                exception_state),
            !OriginClean()),
        kInterpolationSpaceSRGB));
  }

  using SaveType = CanvasRenderingContext2DState::SaveType;
  SaveType save_type = SaveLayerForState(state, *canvas);

#if DCHECK_IS_ON()
  if (save_type == SaveType::kBeginEndLayerTwoSaves) {
    ++layer_extra_saves_;
  }
#endif

  state_stack_.push_back(MakeGarbageCollected<CanvasRenderingContext2DState>(
      state, CanvasRenderingContext2DState::kDontCopyClipList, save_type));
  max_state_stack_depth_ =
      std::max(state_stack_.size(), max_state_stack_depth_);

  ValidateStateStack();

  // Reset compositing attributes.
  setShadowOffsetX(0);
  setShadowOffsetY(0);
  setShadowBlur(0);
  GetState().SetShadowColor(Color::kTransparent);
  DCHECK(!GetState().ShouldDrawShadows());
  setGlobalAlpha(1.0);
  setGlobalCompositeOperation("source-over");
  setFilter(GetTopExecutionContext(),
            MakeGarbageCollected<V8UnionCanvasFilterOrString>("none"));
}

CanvasRenderingContext2DState::SaveType
BaseRenderingContext2D::SaveLayerForState(
    const CanvasRenderingContext2DState& state,
    cc::PaintCanvas& canvas) const {
  const int initial_save_count = canvas.getSaveCount();
  bool needs_compositing = state.GlobalComposite() != SkBlendMode::kSrcOver;

  // Global states must be applied on the result of the layer's filter.
  // For alpha + shadows or compositing, we must use two nested layers. The
  // inner one applies the alpha and the outer one applies the shadow and/or
  // compositing. This is needed to to get a transparent foreground, as the
  // alpha would otherwise be applied to the result of foreground+background.
  if (state.ShouldDrawShadows() || BlendModeRequiresCompositedDraw(state)) {
    cc::PaintFlags flags;
    flags.setBlendMode(state.GlobalComposite());
    needs_compositing = false;
    if (state.ShouldDrawShadows()) {
      flags.setImageFilter(state.ShadowAndForegroundImageFilter());
    }
    canvas.saveLayer(flags);
  }

  if (state.HasLayerFilter() || needs_compositing) {
    cc::PaintFlags flags;
    flags.setAlphaf(static_cast<float>(state.GlobalAlpha()));
    flags.setImageFilter(state.GetLayerFilter());
    if (needs_compositing) {
      flags.setBlendMode(state.GlobalComposite());
    }
    canvas.saveLayer(flags);
  } else if (state.GlobalAlpha() != 1 ||
             initial_save_count == canvas.getSaveCount()) {
    canvas.saveLayerAlphaf(state.GlobalAlpha());
  }

  const int save_diff = canvas.getSaveCount() - initial_save_count;
  CHECK(save_diff == 1 || save_diff == 2);
  using SaveType = CanvasRenderingContext2DState::SaveType;
  return save_diff == 2 ? SaveType::kBeginEndLayerTwoSaves
                        : SaveType::kBeginEndLayerOneSave;
}

void BaseRenderingContext2D::endLayer() {
  if (UNLIKELY(isContextLost())) {
    return;
  }
  // TODO(crbug.com/1234113): Instrument new canvas APIs.
  identifiability_study_helper_.set_encountered_skipped_ops();

  ValidateStateStack();
  if (state_stack_.size() <= 1 || layer_count_ <= 0)
    return;

  DCHECK_GT(state_stack_.size(), static_cast<WTF::wtf_size_t>(layer_count_));

  // Verify that the current state's transform is invertible.
  if (IsTransformInvertible())
    GetModifiablePath().Transform(GetState().GetTransform());

  // All saves performed since the last beginLayer are no-ops.
  while (state_stack_.back()->GetSaveType() ==
         CanvasRenderingContext2DState::SaveType::kSaveRestore) {
    PopAndRestore();
  }

  // If we do an endLayer, we have to be sure that we did a beginLayer (that
  // could have introduced an extra state).
  DCHECK(state_stack_.back()->IsLayerSaveType());

  PopAndRestore();

  --layer_count_;
  ValidateStateStack();
}

void BaseRenderingContext2D::PopAndRestore() {
  if (state_stack_.size() <= 1) {
    NOTREACHED();
    return;
  }

  cc::PaintCanvas* canvas = GetOrCreatePaintCanvas();

  if (!canvas)
    return;

  if (state_stack_.back()->GetSaveType() ==
      CanvasRenderingContext2DState::SaveType::kBeginEndLayerTwoSaves) {
    canvas->restore();
#if DCHECK_IS_ON()
    --layer_extra_saves_;
#endif
  }

  canvas->restore();
  state_stack_.pop_back();
  CanvasRenderingContext2DState& state = GetState();
  state.ClearResolvedFilter();
  // If we popped a layer, we can clear its filter as it's no longer needed.
  state.SetLayerFilter(nullptr);

  SetIsTransformInvertible(GetState().IsTransformInvertible());
  if (IsTransformInvertible())
    GetModifiablePath().Transform(GetState().GetTransform().Inverse());
}

void BaseRenderingContext2D::RestoreMatrixClipStack(cc::PaintCanvas* c) const {
  if (!c)
    return;
  CanvasRenderingContext2DState* prev_state = nullptr;
  for (Member<CanvasRenderingContext2DState> curr_state : state_stack_) {
    if (curr_state->IsLayerSaveType()) {
      // Layers are rendered with the render states of their parent.
      CanvasRenderingContext2DState::SaveType save_type =
          SaveLayerForState(CHECK_DEREF(prev_state), *c);
      CHECK_EQ(save_type, curr_state->GetSaveType());
    } else {
      c->save();
    }

    c->setMatrix(SkM44());
    curr_state->PlaybackClips(c);
    c->setMatrix(AffineTransformToSkM44(curr_state->GetTransform()));

    prev_state = curr_state.Get();
  }
  ValidateStateStackWithCanvas(c);
}

void BaseRenderingContext2D::ResetInternal() {
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kReset);
  }
  ValidateStateStack();
  state_stack_.resize(1);
  state_stack_.front() = MakeGarbageCollected<CanvasRenderingContext2DState>();
  SetIsTransformInvertible(true);
  Clear();
  if (cc::PaintCanvas* c = GetPaintCanvas()) {
    // The canvas should always have an initial/unbalanced save frame, which
    // we use to reset the top level matrix and clip here.
    c->restoreToCount(1);
    // Save once, to match the first entry in `state_stack_`.
    c->save();
    DCHECK(c->getLocalToDevice() == SkM44());
#if DCHECK_IS_ON()
    SkIRect clip_bounds;
    DCHECK(c->getDeviceClipBounds(&clip_bounds));
    DCHECK(clip_bounds == c->imageInfo().bounds());
#endif
    // We only want to clear the backing buffer if the surface exists because
    // this function is also used when the context is lost.
    clearRect(0, 0, Width(), Height(), /*for_reset=*/true);
  }
  ValidateStateStack();
  origin_tainted_by_content_ = false;
}

void BaseRenderingContext2D::reset() {
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DReset);
  ResetInternal();
}

void BaseRenderingContext2D::IdentifiabilityUpdateForStyleUnion(
    const V8CanvasStyle& style) {
  switch (style.type) {
    case V8CanvasStyleType::kCSSColorValue:
      break;
    case V8CanvasStyleType::kGradient:
      identifiability_study_helper_.UpdateBuilder(
          style.gradient->GetIdentifiableToken());
      break;
    case V8CanvasStyleType::kPattern:
      identifiability_study_helper_.UpdateBuilder(
          style.pattern->GetIdentifiableToken());
      break;
    case V8CanvasStyleType::kString:
      identifiability_study_helper_.UpdateBuilder(
          IdentifiabilityBenignStringToken(style.string));
      break;
  }
}

RespectImageOrientationEnum
BaseRenderingContext2D::RespectImageOrientationInternal(
    CanvasImageSource* image_source) {
  if ((image_source->IsImageBitmap() || image_source->IsImageElement()) &&
      image_source->WouldTaintOrigin())
    return kRespectImageOrientation;
  return RespectImageOrientation();
}

v8::Local<v8::Value> BaseRenderingContext2D::strokeStyle(
    ScriptState* script_state) const {
  return CanvasStyleToV8(script_state, GetState().StrokeStyle());
}

void BaseRenderingContext2D::
    UpdateIdentifiabilityStudyBeforeSettingStrokeOrFill(
        const V8CanvasStyle& v8_style,
        CanvasOps op) {
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(op);
    IdentifiabilityUpdateForStyleUnion(v8_style);
  }
}

bool BaseRenderingContext2D::ExtractColorFromV8ValueAndUpdateCache(
    const V8CanvasStyle& v8_style,
    Color& color) {
  // This should only be called for string styles.
  DCHECK_EQ(v8_style.type, V8CanvasStyleType::kString);
  if (color_cache_) {
    const CachedColor* cached_color =
        color_cache_->GetCachedColor(v8_style.string);
    if (cached_color) {
      if (cached_color->parse_result == ColorParseResult::kColor) {
        color = cached_color->color;
        return true;
      }
      if (cached_color->parse_result == ColorParseResult::kCurrentColor) {
        color = GetCurrentColor();
        return true;
      }
      DCHECK_EQ(cached_color->parse_result, ColorParseResult::kParseFailed);
      return false;
    }
  }
  const ColorParseResult parse_result =
      ParseColorOrCurrentColor(v8_style.string, color);
  if (color_cache_) {
    color_cache_->SetCachedColor(v8_style.string, color, parse_result);
  }
  return parse_result != ColorParseResult::kParseFailed;
}

void BaseRenderingContext2D::setStrokeStyle(v8::Isolate* isolate,
                                            v8::Local<v8::Value> value,
                                            ExceptionState& exception_state) {
  V8CanvasStyle v8_style;
  if (!ExtractV8CanvasStyle(isolate, value, v8_style, exception_state))
    return;

  UpdateIdentifiabilityStudyBeforeSettingStrokeOrFill(
      v8_style, CanvasOps::kSetStrokeStyle);

  switch (v8_style.type) {
    case V8CanvasStyleType::kCSSColorValue:
      GetState().SetStrokeColor(v8_style.css_color_value);
      break;
    case V8CanvasStyleType::kGradient:
      GetState().SetStrokeGradient(v8_style.gradient);
      break;
    case V8CanvasStyleType::kPattern:
      if (!origin_tainted_by_content_ && !v8_style.pattern->OriginClean())
        SetOriginTaintedByContent();
      GetState().SetStrokePattern(v8_style.pattern);
      break;
    case V8CanvasStyleType::kString: {
      if (v8_style.string == GetState().UnparsedStrokeColor())
        return;
      Color parsed_color = Color::kTransparent;
      if (!ExtractColorFromV8ValueAndUpdateCache(v8_style, parsed_color)) {
        return;
      }
      if (GetState().StrokeStyle().IsEquivalentColor(parsed_color)) {
        GetState().SetUnparsedStrokeColor(v8_style.string);
        return;
      }
      GetState().SetStrokeColor(parsed_color);
      break;
    }
  }

  GetState().SetUnparsedStrokeColor(v8_style.string);
  GetState().ClearResolvedFilter();
}

ColorParseResult BaseRenderingContext2D::ParseColorOrCurrentColor(
    const String& color_string,
    Color& color) const {
  const ColorParseResult parse_result =
      ParseCanvasColorString(color_string, color_scheme_, color);
  if (parse_result == ColorParseResult::kCurrentColor) {
    color = GetCurrentColor();
  }
  return parse_result;
}

v8::Local<v8::Value> BaseRenderingContext2D::fillStyle(
    ScriptState* script_state) const {
  return CanvasStyleToV8(script_state, GetState().FillStyle());
}

void BaseRenderingContext2D::setFillStyle(v8::Isolate* isolate,
                                          v8::Local<v8::Value> value,
                                          ExceptionState& exception_state) {
  V8CanvasStyle v8_style;
  if (!ExtractV8CanvasStyle(isolate, value, v8_style, exception_state))
    return;

  ValidateStateStack();

  UpdateIdentifiabilityStudyBeforeSettingStrokeOrFill(v8_style,
                                                      CanvasOps::kSetFillStyle);

  switch (v8_style.type) {
    case V8CanvasStyleType::kCSSColorValue:
      GetState().SetFillColor(v8_style.css_color_value);
      break;
    case V8CanvasStyleType::kGradient:
      GetState().SetFillGradient(v8_style.gradient);
      break;
    case V8CanvasStyleType::kPattern:
      if (!origin_tainted_by_content_ && !v8_style.pattern->OriginClean())
        SetOriginTaintedByContent();
      GetState().SetFillPattern(v8_style.pattern);
      break;
    case V8CanvasStyleType::kString: {
      if (v8_style.string == GetState().UnparsedFillColor())
        return;
      Color parsed_color = Color::kTransparent;
      if (!ExtractColorFromV8ValueAndUpdateCache(v8_style, parsed_color)) {
        return;
      }
      if (GetState().FillStyle().IsEquivalentColor(parsed_color)) {
        GetState().SetUnparsedFillColor(v8_style.string);
        return;
      }
      GetState().SetFillColor(parsed_color);
      break;
    }
  }

  GetState().SetUnparsedFillColor(v8_style.string);
  GetState().ClearResolvedFilter();
}

double BaseRenderingContext2D::lineWidth() const {
  return GetState().LineWidth();
}

void BaseRenderingContext2D::setLineWidth(double width) {
  if (!std::isfinite(width) || width <= 0)
    return;
  if (GetState().LineWidth() == width)
    return;
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetLineWidth,
                                                width);
  }
  GetState().SetLineWidth(ClampTo<float>(width));
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
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetLineCap, cap);
  }
  GetState().SetLineCap(cap);
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
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetLineJoin, join);
  }
  GetState().SetLineJoin(join);
}

double BaseRenderingContext2D::miterLimit() const {
  return GetState().MiterLimit();
}

void BaseRenderingContext2D::setMiterLimit(double limit) {
  if (!std::isfinite(limit) || limit <= 0)
    return;
  if (GetState().MiterLimit() == limit)
    return;
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetMiterLimit,
                                                limit);
  }
  GetState().SetMiterLimit(ClampTo<float>(limit));
}

double BaseRenderingContext2D::shadowOffsetX() const {
  return GetState().ShadowOffset().x();
}

void BaseRenderingContext2D::setShadowOffsetX(double x) {
  if (!std::isfinite(x))
    return;
  if (GetState().ShadowOffset().x() == x)
    return;
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetShadowOffsetX,
                                                x);
  }
  GetState().SetShadowOffsetX(ClampTo<float>(x));
}

double BaseRenderingContext2D::shadowOffsetY() const {
  return GetState().ShadowOffset().y();
}

void BaseRenderingContext2D::setShadowOffsetY(double y) {
  if (!std::isfinite(y))
    return;
  if (GetState().ShadowOffset().y() == y)
    return;
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetShadowOffsetY,
                                                y);
  }
  GetState().SetShadowOffsetY(ClampTo<float>(y));
}

double BaseRenderingContext2D::shadowBlur() const {
  return GetState().ShadowBlur();
}

void BaseRenderingContext2D::setShadowBlur(double blur) {
  if (!std::isfinite(blur) || blur < 0)
    return;
  if (GetState().ShadowBlur() == blur)
    return;
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetShadowBlur,
                                                blur);
  }
  GetState().SetShadowBlur(ClampTo<float>(blur));
}

String BaseRenderingContext2D::shadowColor() const {
  // TODO(https://1351544): CanvasRenderingContext2DState's shadow color should
  // be a Color, not an SkColor or SkColor4f.
  return GetState().ShadowColor().SerializeAsCanvasColor();
}

void BaseRenderingContext2D::setShadowColor(const String& color_string) {
  Color color;
  if (ParseColorOrCurrentColor(color_string, color) ==
      ColorParseResult::kParseFailed) {
    return;
  }
  if (GetState().ShadowColor() == color) {
    return;
  }
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetShadowColor,
                                                color.Rgb());
  }
  GetState().SetShadowColor(color);
}

const Vector<double>& BaseRenderingContext2D::getLineDash() const {
  return GetState().LineDash();
}

static bool LineDashSequenceIsValid(const Vector<double>& dash) {
  return base::ranges::all_of(
      dash, [](double d) { return std::isfinite(d) && d >= 0; });
}

void BaseRenderingContext2D::setLineDash(const Vector<double>& dash) {
  if (!LineDashSequenceIsValid(dash))
    return;
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetLineDash,
                                                base::make_span(dash));
  }
  GetState().SetLineDash(dash);
}

double BaseRenderingContext2D::lineDashOffset() const {
  return GetState().LineDashOffset();
}

void BaseRenderingContext2D::setLineDashOffset(double offset) {
  if (!std::isfinite(offset) || GetState().LineDashOffset() == offset)
    return;
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetLineDashOffset,
                                                offset);
  }
  GetState().SetLineDashOffset(ClampTo<float>(offset));
}

double BaseRenderingContext2D::globalAlpha() const {
  return GetState().GlobalAlpha();
}

void BaseRenderingContext2D::setGlobalAlpha(double alpha) {
  if (!(alpha >= 0 && alpha <= 1))
    return;
  if (GetState().GlobalAlpha() == alpha)
    return;
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetGlobalAlpha,
                                                alpha);
  }
  GetState().SetGlobalAlpha(alpha);
}

String BaseRenderingContext2D::globalCompositeOperation() const {
  auto [composite_op, blend_mode] =
      CompositeAndBlendOpsFromSkBlendMode(GetState().GlobalComposite());
  return CanvasCompositeOperatorName(composite_op, blend_mode);
}

void BaseRenderingContext2D::setGlobalCompositeOperation(
    const String& operation) {
  CompositeOperator op = kCompositeSourceOver;
  BlendMode blend_mode = BlendMode::kNormal;
  if (!ParseCanvasCompositeAndBlendMode(operation, op, blend_mode))
    return;
  SkBlendMode sk_blend_mode = WebCoreCompositeToSkiaComposite(op, blend_mode);
  if (GetState().GlobalComposite() == sk_blend_mode)
    return;
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kSetGlobalCompositeOpertion, sk_blend_mode);
  }
  GetState().SetGlobalComposite(sk_blend_mode);
}

const V8UnionCanvasFilterOrString* BaseRenderingContext2D::filter() const {
  if (CanvasFilter* filter = GetState().GetCanvasFilter()) {
    return MakeGarbageCollected<V8UnionCanvasFilterOrString>(filter);
  }
  return MakeGarbageCollected<V8UnionCanvasFilterOrString>(
      GetState().UnparsedCSSFilter());
}

void BaseRenderingContext2D::setFilter(
    const ExecutionContext* execution_context,
    const V8UnionCanvasFilterOrString* input) {
  if (!input)
    return;

  switch (input->GetContentType()) {
    case V8UnionCanvasFilterOrString::ContentType::kCanvasFilter:
      UseCounter::Count(GetTopExecutionContext(),
                        WebFeature::kCanvasRenderingContext2DCanvasFilter);
      GetState().SetCanvasFilter(input->GetAsCanvasFilter());
      SnapshotStateForFilter();
      // TODO(crbug.com/1234113): Instrument new canvas APIs.
      identifiability_study_helper_.set_encountered_skipped_ops();
      break;
    case V8UnionCanvasFilterOrString::ContentType::kString: {
      const String& filter_string = input->GetAsString();
      if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
        identifiability_study_helper_.UpdateBuilder(
            CanvasOps::kSetFilter,
            IdentifiabilitySensitiveStringToken(filter_string));
      }
      if (!GetState().GetCanvasFilter() && !GetState().IsFontDirtyForFilter() &&
          filter_string == GetState().UnparsedCSSFilter()) {
        return;
      }
      if (!execution_context)
        return;
      const CSSValue* css_value = CSSParser::ParseSingleValue(
          CSSPropertyID::kFilter, filter_string,
          MakeGarbageCollected<CSSParserContext>(
              kHTMLStandardMode, execution_context->GetSecureContextMode()));
      if (!css_value || css_value->IsCSSWideKeyword())
        return;
      GetState().SetUnparsedCSSFilter(filter_string);
      GetState().SetCSSFilter(css_value);
      SnapshotStateForFilter();
      break;
    }
  }
}

void BaseRenderingContext2D::scale(double sx, double sy) {
  // TODO(crbug.com/1140535): Investigate the performance impact of simply
  // calling the 3d version of this function
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;

  if (!std::isfinite(sx) || !std::isfinite(sy))
    return;
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kScale, sx, sy);
  }

  AffineTransform new_transform = GetState().GetTransform();
  float fsx = ClampTo<float>(sx);
  float fsy = ClampTo<float>(sy);
  new_transform.ScaleNonUniform(fsx, fsy);
  if (GetState().GetTransform() == new_transform)
    return;

  SetTransform(new_transform);
  if (UNLIKELY(!IsTransformInvertible())) {
    return;
  }

  c->scale(fsx, fsy);
  GetModifiablePath().Transform(
      AffineTransform().ScaleNonUniform(1.0 / fsx, 1.0 / fsy));
}

void BaseRenderingContext2D::rotate(double angle_in_radians) {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;

  if (!std::isfinite(angle_in_radians))
    return;
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kRotate,
                                                angle_in_radians);
  }

  AffineTransform new_transform = GetState().GetTransform();
  new_transform.RotateRadians(angle_in_radians);
  if (GetState().GetTransform() == new_transform)
    return;

  SetTransform(new_transform);
  if (UNLIKELY(!IsTransformInvertible())) {
    return;
  }
  c->rotate(ClampTo<float>(angle_in_radians * (180.0 / kPiFloat)));
  GetModifiablePath().Transform(
      AffineTransform().RotateRadians(-angle_in_radians));
}

void BaseRenderingContext2D::translate(double tx, double ty) {
  // TODO(crbug.com/1140535): Investigate the performance impact of simply
  // calling the 3d version of this function
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;

  if (UNLIKELY(!IsTransformInvertible())) {
    return;
  }

  if (!std::isfinite(tx) || !std::isfinite(ty))
    return;
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kTranslate, tx, ty);
  }

  AffineTransform new_transform = GetState().GetTransform();
  // clamp to float to avoid float cast overflow when used as SkScalar
  float ftx = ClampTo<float>(tx);
  float fty = ClampTo<float>(ty);
  new_transform.Translate(ftx, fty);
  if (GetState().GetTransform() == new_transform)
    return;

  SetTransform(new_transform);
  if (UNLIKELY(!IsTransformInvertible())) {
    return;
  }

  c->translate(ftx, fty);
  GetModifiablePath().Transform(AffineTransform().Translate(-ftx, -fty));
}

void BaseRenderingContext2D::transform(double m11,
                                       double m12,
                                       double m21,
                                       double m22,
                                       double dx,
                                       double dy) {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;

  if (!std::isfinite(m11) || !std::isfinite(m21) || !std::isfinite(dx) ||
      !std::isfinite(m12) || !std::isfinite(m22) || !std::isfinite(dy))
    return;

  // clamp to float to avoid float cast overflow when used as SkScalar
  float fm11 = ClampTo<float>(m11);
  float fm12 = ClampTo<float>(m12);
  float fm21 = ClampTo<float>(m21);
  float fm22 = ClampTo<float>(m22);
  float fdx = ClampTo<float>(dx);
  float fdy = ClampTo<float>(dy);
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kTransform, fm11,
                                                fm12, fm21, fm22, fdx, fdy);
  }

  AffineTransform transform(fm11, fm12, fm21, fm22, fdx, fdy);
  AffineTransform new_transform = GetState().GetTransform() * transform;
  if (GetState().GetTransform() == new_transform)
    return;

  SetTransform(new_transform);
  if (UNLIKELY(!IsTransformInvertible())) {
    return;
  }

  c->concat(AffineTransformToSkM44(transform));
  GetModifiablePath().Transform(transform.Inverse());
}

void BaseRenderingContext2D::resetTransform() {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kResetTransform);
  }

  AffineTransform ctm = GetState().GetTransform();
  bool invertible_ctm = IsTransformInvertible();
  // It is possible that CTM is identity while CTM is not invertible.
  // When CTM becomes non-invertible, realizeSaves() can make CTM identity.
  if (ctm.IsIdentity() && invertible_ctm)
    return;

  // resetTransform() resolves the non-invertible CTM state.
  GetState().ResetTransform();
  SetIsTransformInvertible(true);
  // Set the SkCanvas' matrix to identity.
  c->setMatrix(SkM44());

  if (invertible_ctm)
    GetModifiablePath().Transform(ctm);
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
  transform(m11, m12, m21, m22, dx, dy);
}

void BaseRenderingContext2D::setTransform(DOMMatrixInit* transform,
                                          ExceptionState& exception_state) {
  DOMMatrixReadOnly* m =
      DOMMatrixReadOnly::fromMatrix(transform, exception_state);

  if (!m)
    return;

  setTransform(m->m11(), m->m12(), m->m21(), m->m22(), m->m41(), m->m42());
}

DOMMatrix* BaseRenderingContext2D::getTransform() {
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

AffineTransform BaseRenderingContext2D::GetTransform() const {
  return GetState().GetTransform();
}

void BaseRenderingContext2D::beginPath() {
  Clear();
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kBeginPath);
  }
}

void BaseRenderingContext2D::DrawPathInternal(
    const CanvasPath& path,
    CanvasRenderingContext2DState::PaintType paint_type,
    SkPathFillType fill_type,
    UsePaintCache use_paint_cache) {
  if (path.IsEmpty())
    return;

  gfx::RectF bounds(path.BoundingRect());
  if (std::isnan(bounds.x()) || std::isnan(bounds.y()) ||
      std::isnan(bounds.width()) || std::isnan(bounds.height()))
    return;

  if (paint_type == CanvasRenderingContext2DState::kStrokePaintType)
    InflateStrokeRect(bounds);

  if (path.IsLine()) {
    auto line = path.line();
    Draw<OverdrawOp::kNone>(
        [line](cc::PaintCanvas* c,
               const cc::PaintFlags* flags)  // draw lambda
        {
          c->drawLine(SkFloatToScalar(line.start.x()),
                      SkFloatToScalar(line.start.y()),
                      SkFloatToScalar(line.end.x()),
                      SkFloatToScalar(line.end.y()), *flags);
        },
        [](const SkIRect& rect)  // overdraw test lambda
        { return false; },
        bounds, paint_type,
        GetState().HasPattern(paint_type)
            ? CanvasRenderingContext2DState::kNonOpaqueImage
            : CanvasRenderingContext2DState::kNoImage,
        CanvasPerformanceMonitor::DrawType::kPath);
    return;
  }

  SkPath sk_path = path.GetPath().GetSkPath();
  sk_path.setFillType(fill_type);

  Draw<OverdrawOp::kNone>(
      [sk_path, use_paint_cache](cc::PaintCanvas* c,
                                 const cc::PaintFlags* flags)  // draw lambda
      { c->drawPath(sk_path, *flags, use_paint_cache); },
      [](const SkIRect& rect)  // overdraw test lambda
      { return false; },
      bounds, paint_type,
      GetState().HasPattern(paint_type)
          ? CanvasRenderingContext2DState::kNonOpaqueImage
          : CanvasRenderingContext2DState::kNoImage,
      CanvasPerformanceMonitor::DrawType::kPath);
}

static SkPathFillType ParseWinding(const String& winding_rule_string) {
  if (winding_rule_string == "nonzero")
    return SkPathFillType::kWinding;
  if (winding_rule_string == "evenodd")
    return SkPathFillType::kEvenOdd;

  NOTREACHED();
  return SkPathFillType::kEvenOdd;
}

void BaseRenderingContext2D::fill(const String& winding_rule_string) {
  const SkPathFillType winding_rule = ParseWinding(winding_rule_string);
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kFill, winding_rule);
  }
  DrawPathInternal(*this, CanvasRenderingContext2DState::kFillPaintType,
                   winding_rule, UsePaintCache::kDisabled);
}

void BaseRenderingContext2D::fill(Path2D* dom_path,
                                  const String& winding_rule_string) {
  const SkPathFillType winding_rule = ParseWinding(winding_rule_string);
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kFill__Path, dom_path->GetIdentifiableToken(), winding_rule);
  }
  DrawPathInternal(*dom_path, CanvasRenderingContext2DState::kFillPaintType,
                   winding_rule, path2d_use_paint_cache_);
}

void BaseRenderingContext2D::stroke() {
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kStroke);
  }
  DrawPathInternal(*this, CanvasRenderingContext2DState::kStrokePaintType,
                   SkPathFillType::kWinding, UsePaintCache::kDisabled);
}

void BaseRenderingContext2D::stroke(Path2D* dom_path) {
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kStroke__Path, dom_path->GetIdentifiableToken());
  }
  DrawPathInternal(*dom_path, CanvasRenderingContext2DState::kStrokePaintType,
                   SkPathFillType::kWinding, path2d_use_paint_cache_);
}

void BaseRenderingContext2D::fillRect(double x,
                                      double y,
                                      double width,
                                      double height) {
  if (!ValidateRectForCanvas(x, y, width, height))
    return;

  if (!GetOrCreatePaintCanvas())
    return;
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kFillRect, x, y,
                                                width, height);
  }

  // We are assuming that if the pattern is not accelerated and the current
  // canvas is accelerated, the texture of the pattern will not be able to be
  // moved to the texture of the canvas receiving the pattern (because if the
  // pattern was unaccelerated is because it was not possible to hold that image
  // in an accelerated texture - that is, into the GPU). That's why we disable
  // the acceleration to be sure that it will work.
  if (IsAccelerated() &&
      GetState().HasPattern(CanvasRenderingContext2DState::kFillPaintType) &&
      !GetState().PatternIsAccelerated(
          CanvasRenderingContext2DState::kFillPaintType)) {
    DisableAcceleration();
    base::UmaHistogramEnumeration(
        "Blink.Canvas.GPUFallbackToCPU",
        GPUFallbackToCPUScenario::kLargePatternDrawnToGPU);
  }

  // clamp to float to avoid float cast overflow when used as SkScalar
  AdjustRectForCanvas(x, y, width, height);
  gfx::RectF rect(ClampTo<float>(x), ClampTo<float>(y), ClampTo<float>(width),
                  ClampTo<float>(height));
  Draw<OverdrawOp::kNone>(
      [rect](cc::PaintCanvas* c, const cc::PaintFlags* flags)  // draw lambda
      { c->drawRect(gfx::RectFToSkRect(rect), *flags); },
      [rect, this](const SkIRect& clip_bounds)  // overdraw test lambda
      { return RectContainsTransformedRect(rect, clip_bounds); },
      rect, CanvasRenderingContext2DState::kFillPaintType,
      GetState().HasPattern(CanvasRenderingContext2DState::kFillPaintType)
          ? CanvasRenderingContext2DState::kNonOpaqueImage
          : CanvasRenderingContext2DState::kNoImage,
      CanvasPerformanceMonitor::DrawType::kRectangle);
}

static void StrokeRectOnCanvas(const gfx::RectF& rect,
                               cc::PaintCanvas* canvas,
                               const cc::PaintFlags* flags) {
  DCHECK_EQ(flags->getStyle(), cc::PaintFlags::kStroke_Style);
  if ((rect.width() > 0) != (rect.height() > 0)) {
    // When stroking, we must skip the zero-dimension segments
    SkPathBuilder path;
    path.moveTo(rect.x(), rect.y());
    path.lineTo(rect.right(), rect.bottom());
    path.close();
    canvas->drawPath(path.detach(), *flags);
    return;
  }
  canvas->drawRect(gfx::RectFToSkRect(rect), *flags);
}

void BaseRenderingContext2D::strokeRect(double x,
                                        double y,
                                        double width,
                                        double height) {
  if (!ValidateRectForCanvas(x, y, width, height))
    return;

  if (!GetOrCreatePaintCanvas())
    return;
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kStrokeRect, x, y,
                                                width, height);
  }

  // clamp to float to avoid float cast overflow when used as SkScalar
  AdjustRectForCanvas(x, y, width, height);
  float fx = ClampTo<float>(x);
  float fy = ClampTo<float>(y);
  float fwidth = ClampTo<float>(width);
  float fheight = ClampTo<float>(height);

  gfx::RectF rect(fx, fy, fwidth, fheight);
  gfx::RectF bounds = rect;
  InflateStrokeRect(bounds);

  if (!ValidateRectForCanvas(bounds.x(), bounds.y(), bounds.width(),
                             bounds.height()))
    return;

  Draw<OverdrawOp::kNone>(
      [rect](cc::PaintCanvas* c, const cc::PaintFlags* flags)  // draw lambda
      { StrokeRectOnCanvas(rect, c, flags); },
      kNoOverdraw, bounds, CanvasRenderingContext2DState::kStrokePaintType,
      GetState().HasPattern(CanvasRenderingContext2DState::kStrokePaintType)
          ? CanvasRenderingContext2DState::kNonOpaqueImage
          : CanvasRenderingContext2DState::kNoImage,
      CanvasPerformanceMonitor::DrawType::kRectangle);
}

void BaseRenderingContext2D::ClipInternal(const Path& path,
                                          const String& winding_rule_string,
                                          UsePaintCache use_paint_cache) {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c) {
    return;
  }
  if (UNLIKELY(!IsTransformInvertible())) {
    return;
  }

  SkPath sk_path = path.GetSkPath();
  sk_path.setFillType(ParseWinding(winding_rule_string));
  GetState().ClipPath(sk_path, clip_antialiasing_);
  c->clipPath(sk_path, SkClipOp::kIntersect, clip_antialiasing_ == kAntiAliased,
              use_paint_cache);
}

void BaseRenderingContext2D::clip(const String& winding_rule_string) {
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kClip,
        IdentifiabilitySensitiveStringToken(winding_rule_string));
  }
  ClipInternal(GetPath(), winding_rule_string, UsePaintCache::kDisabled);
}

void BaseRenderingContext2D::clip(Path2D* dom_path,
                                  const String& winding_rule_string) {
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kClip__Path, dom_path->GetIdentifiableToken(),
        IdentifiabilitySensitiveStringToken(winding_rule_string));
  }
  ClipInternal(dom_path->GetPath(), winding_rule_string,
               UsePaintCache::kEnabled);
}

bool BaseRenderingContext2D::isPointInPath(const double x,
                                           const double y,
                                           const String& winding_rule_string) {
  return IsPointInPathInternal(GetPath(), x, y, winding_rule_string);
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
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return false;
  if (UNLIKELY(!IsTransformInvertible())) {
    return false;
  }

  if (!std::isfinite(x) || !std::isfinite(y))
    return false;
  gfx::PointF point(ClampTo<float>(x), ClampTo<float>(y));
  AffineTransform ctm = GetState().GetTransform();
  gfx::PointF transformed_point = ctm.Inverse().MapPoint(point);

  return path.Contains(transformed_point,
                       SkFillTypeToWindRule(ParseWinding(winding_rule_string)));
}

bool BaseRenderingContext2D::isPointInStroke(const double x, const double y) {
  return IsPointInStrokeInternal(GetPath(), x, y);
}

bool BaseRenderingContext2D::isPointInStroke(Path2D* dom_path,
                                             const double x,
                                             const double y) {
  return IsPointInStrokeInternal(dom_path->GetPath(), x, y);
}

bool BaseRenderingContext2D::IsPointInStrokeInternal(const Path& path,
                                                     const double x,
                                                     const double y) {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return false;
  if (UNLIKELY(!IsTransformInvertible())) {
    return false;
  }

  if (!std::isfinite(x) || !std::isfinite(y))
    return false;
  gfx::PointF point(ClampTo<float>(x), ClampTo<float>(y));
  const AffineTransform& ctm = GetState().GetTransform();
  gfx::PointF transformed_point = ctm.Inverse().MapPoint(point);

  StrokeData stroke_data;
  stroke_data.SetThickness(GetState().LineWidth());
  stroke_data.SetLineCap(GetState().GetLineCap());
  stroke_data.SetLineJoin(GetState().GetLineJoin());
  stroke_data.SetMiterLimit(GetState().MiterLimit());
  Vector<float> line_dash(GetState().LineDash().size());
  base::ranges::copy(GetState().LineDash(), line_dash.begin());
  stroke_data.SetLineDash(line_dash, GetState().LineDashOffset());
  return path.StrokeContains(transformed_point, stroke_data, ctm);
}

void BaseRenderingContext2D::clearRect(double x,
                                       double y,
                                       double width,
                                       double height,
                                       bool for_reset) {
  if (!ValidateRectForCanvas(x, y, width, height))
    return;

  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;
  if (UNLIKELY(!IsTransformInvertible())) {
    return;
  }

  SkIRect clip_bounds;
  if (!c->getDeviceClipBounds(&clip_bounds))
    return;
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kClearRect, x, y,
                                                width, height);
  }

  cc::PaintFlags clear_flags;
  clear_flags.setStyle(cc::PaintFlags::kFill_Style);
  if (HasAlpha()) {
    clear_flags.setBlendMode(SkBlendMode::kClear);
  } else {
    clear_flags.setColor(SK_ColorBLACK);
  }

  // clamp to float to avoid float cast overflow when used as SkScalar
  AdjustRectForCanvas(x, y, width, height);
  float fx = ClampTo<float>(x);
  float fy = ClampTo<float>(y);
  float fwidth = ClampTo<float>(width);
  float fheight = ClampTo<float>(height);

  gfx::RectF rect(fx, fy, fwidth, fheight);
  if (RectContainsTransformedRect(rect, clip_bounds)) {
    if (for_reset) {
      // In the reset case, we can use kUntransformedUnclippedFill because we
      // know the state state was reset.
      CheckOverdraw(&clear_flags, CanvasRenderingContext2DState::kNoImage,
                    OverdrawOp::kContextReset);
    } else {
      CheckOverdraw(&clear_flags, CanvasRenderingContext2DState::kNoImage,
                    OverdrawOp::kClearRect);
    }
    WillDraw(clip_bounds, CanvasPerformanceMonitor::DrawType::kOther);
    c->drawRect(gfx::RectFToSkRect(rect), clear_flags);
  } else {
    SkIRect dirty_rect;
    if (ComputeDirtyRect(rect, clip_bounds, &dirty_rect)) {
      WillDraw(clip_bounds, CanvasPerformanceMonitor::DrawType::kOther);
      c->drawRect(gfx::RectFToSkRect(rect), clear_flags);
    }
  }
}

static inline void ClipRectsToImageRect(const gfx::RectF& image_rect,
                                        gfx::RectF* src_rect,
                                        gfx::RectF* dst_rect) {
  if (image_rect.Contains(*src_rect))
    return;

  // Compute the src to dst transform
  gfx::SizeF scale(dst_rect->size().width() / src_rect->size().width(),
                   dst_rect->size().height() / src_rect->size().height());
  gfx::PointF scaled_src_location = src_rect->origin();
  scaled_src_location.Scale(scale.width(), scale.height());
  gfx::Vector2dF offset = dst_rect->origin() - scaled_src_location;

  src_rect->Intersect(image_rect);

  // To clip the destination rectangle in the same proportion, transform the
  // clipped src rect
  *dst_rect = *src_rect;
  dst_rect->Scale(scale.width(), scale.height());
  dst_rect->Offset(offset);
}

void BaseRenderingContext2D::drawImage(const V8CanvasImageSource* image_source,
                                       double x,
                                       double y,
                                       ExceptionState& exception_state) {
  CanvasImageSource* image_source_internal =
      ToCanvasImageSource(image_source, exception_state);
  if (!image_source_internal)
    return;
  RespectImageOrientationEnum respect_orientation =
      RespectImageOrientationInternal(image_source_internal);
  gfx::SizeF default_object_size(Width(), Height());
  gfx::SizeF source_rect_size = image_source_internal->ElementSize(
      default_object_size, respect_orientation);
  gfx::SizeF dest_rect_size = image_source_internal->DefaultDestinationSize(
      default_object_size, respect_orientation);
  drawImage(image_source_internal, 0, 0, source_rect_size.width(),
            source_rect_size.height(), x, y, dest_rect_size.width(),
            dest_rect_size.height(), exception_state);
}

void BaseRenderingContext2D::drawImage(const V8CanvasImageSource* image_source,
                                       double x,
                                       double y,
                                       double width,
                                       double height,
                                       ExceptionState& exception_state) {
  CanvasImageSource* image_source_internal =
      ToCanvasImageSource(image_source, exception_state);
  if (!image_source_internal)
    return;
  gfx::SizeF default_object_size(Width(), Height());
  gfx::SizeF source_rect_size = image_source_internal->ElementSize(
      default_object_size,
      RespectImageOrientationInternal(image_source_internal));
  drawImage(image_source_internal, 0, 0, source_rect_size.width(),
            source_rect_size.height(), x, y, width, height, exception_state);
}

void BaseRenderingContext2D::drawImage(const V8CanvasImageSource* image_source,
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
      ToCanvasImageSource(image_source, exception_state);
  if (!image_source_internal)
    return;
  drawImage(image_source_internal, sx, sy, sw, sh, dx, dy, dw, dh,
            exception_state);
}

bool BaseRenderingContext2D::ShouldDrawImageAntialiased(
    const gfx::RectF& dest_rect) const {
  if (!GetState().ShouldAntialias())
    return false;
  const cc::PaintCanvas* c = GetPaintCanvas();
  DCHECK(c);

  const SkMatrix& ctm = c->getLocalToDevice().asM33();
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
  return dest_rect.width() * fabs(width_expansion) < 1 ||
         dest_rect.height() * fabs(height_expansion) < 1;
}

void BaseRenderingContext2D::DispatchContextLostEvent(TimerBase*) {
  Event* event = Event::CreateCancelable(event_type_names::kContextlost);
  GetCanvasRenderingContextHost()->HostDispatchEvent(event);

  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DContextLostEvent);
  if (event->defaultPrevented()) {
    context_restorable_ = false;
  }

  if (context_restorable_ &&
      (context_lost_mode_ == CanvasRenderingContext::kRealLostContext ||
       context_lost_mode_ == CanvasRenderingContext::kSyntheticLostContext)) {
    try_restore_context_attempt_count_ = 0;
    try_restore_context_event_timer_.StartOneShot(kTryRestoreContextInterval,
                                                  FROM_HERE);
  }
}

void BaseRenderingContext2D::DispatchContextRestoredEvent(TimerBase*) {
  // Since canvas may trigger contextlost event by multiple different ways (ex:
  // gpu crashes and frame eviction), it's possible to triggeer this
  // function while the context is already restored. In this case, we
  // abort it here.
  if (context_lost_mode_ == CanvasRenderingContext::kNotLostContext)
    return;
  ResetInternal();
  context_lost_mode_ = CanvasRenderingContext::kNotLostContext;
  Event* event(Event::Create(event_type_names::kContextrestored));
  GetCanvasRenderingContextHost()->HostDispatchEvent(event);
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DContextRestoredEvent);
}

void BaseRenderingContext2D::DrawImageInternal(
    cc::PaintCanvas* c,
    CanvasImageSource* image_source,
    Image* image,
    const gfx::RectF& src_rect,
    const gfx::RectF& dst_rect,
    const SkSamplingOptions& sampling,
    const cc::PaintFlags* flags) {
  cc::RecordPaintCanvas::DisableFlushCheckScope disable_flush_check_scope(
      static_cast<cc::RecordPaintCanvas*>(c));
  int initial_save_count = c->getSaveCount();
  cc::PaintFlags image_flags = *flags;

  if (flags->getImageFilter()) {
    SkM44 ctm = c->getLocalToDevice();
    SkM44 inv_ctm;
    if (!ctm.invert(&inv_ctm)) {
      // There is an earlier check for invertibility, but the arithmetic
      // in AffineTransform is not exactly identical, so it is possible
      // for SkMatrix to find the transform to be non-invertible at this stage.
      // crbug.com/504687
      return;
    }
    SkRect bounds = gfx::RectFToSkRect(dst_rect);
    ctm.asM33().mapRect(&bounds);
    if (!bounds.isFinite()) {
      // There is an earlier check for the correctness of the bounds, but it is
      // possible that after applying the matrix transformation we get a faulty
      // set of bounds, so we want to catch this asap and avoid sending a draw
      // command. crbug.com/1039125
      // We want to do this before the save command is sent.
      return;
    }
    c->save();
    c->concat(inv_ctm);

    cc::PaintFlags layer_flags;
    layer_flags.setBlendMode(flags->getBlendMode());
    layer_flags.setImageFilter(flags->getImageFilter());

    c->saveLayer(bounds, layer_flags);
    c->concat(ctm);
    image_flags.setBlendMode(SkBlendMode::kSrcOver);
    image_flags.setImageFilter(nullptr);
  }

  if (image_source->IsVideoElement()) {
    c->save();
    c->clipRect(gfx::RectFToSkRect(dst_rect));
    c->translate(dst_rect.x(), dst_rect.y());
    c->scale(dst_rect.width() / src_rect.width(),
             dst_rect.height() / src_rect.height());
    c->translate(-src_rect.x(), -src_rect.y());
    HTMLVideoElement* video = static_cast<HTMLVideoElement*>(image_source);
    video->PaintCurrentFrame(
        c, gfx::Rect(video->videoWidth(), video->videoHeight()), &image_flags);
  } else if (image_source->IsVideoFrame()) {
    VideoFrame* frame = static_cast<VideoFrame*>(image_source);
    auto media_frame = frame->frame();
    bool ignore_transformation =
        RespectImageOrientationInternal(image_source) ==
        kDoNotRespectImageOrientation;
    gfx::RectF corrected_src_rect = src_rect;

    if (!ignore_transformation) {
      auto orientation_enum = VideoTransformationToImageOrientation(
          media_frame->metadata().transformation.value_or(
              media::kNoTransformation));
      if (ImageOrientation(orientation_enum).UsesWidthAsHeight())
        corrected_src_rect = gfx::TransposeRect(src_rect);
    }

    c->save();
    c->clipRect(gfx::RectFToSkRect(dst_rect));
    c->translate(dst_rect.x(), dst_rect.y());
    c->scale(dst_rect.width() / corrected_src_rect.width(),
             dst_rect.height() / corrected_src_rect.height());
    c->translate(-corrected_src_rect.x(), -corrected_src_rect.y());
    DrawVideoFrameIntoCanvas(std::move(media_frame), c, image_flags,
                             ignore_transformation);
  } else {
    // We always use the image-orientation property on the canvas element
    // because the alternative would result in complex rules depending on
    // the source of the image.
    RespectImageOrientationEnum respect_orientation =
        RespectImageOrientationInternal(image_source);
    gfx::RectF corrected_src_rect = src_rect;
    if (respect_orientation == kRespectImageOrientation &&
        !image->HasDefaultOrientation()) {
      corrected_src_rect = image->CorrectSrcRectForImageOrientation(
          image->SizeAsFloat(kRespectImageOrientation), src_rect);
    }
    image_flags.setAntiAlias(ShouldDrawImageAntialiased(dst_rect));
    ImageDrawOptions draw_options;
    draw_options.sampling_options = sampling;
    draw_options.respect_orientation = respect_orientation;
    draw_options.clamping_mode = Image::kDoNotClampImageToSourceRect;
    image->Draw(c, image_flags, dst_rect, corrected_src_rect, draw_options);
  }

  c->restoreToCount(initial_save_count);
}

void BaseRenderingContext2D::SetOriginTaintedByContent() {
  SetOriginTainted();
  origin_tainted_by_content_ = true;
  for (auto& state : state_stack_)
    state->ClearResolvedFilter();
}

void BaseRenderingContext2D::drawImage(CanvasImageSource* image_source,
                                       double sx,
                                       double sy,
                                       double sw,
                                       double sh,
                                       double dx,
                                       double dy,
                                       double dw,
                                       double dh,
                                       ExceptionState& exception_state) {
  if (!GetOrCreatePaintCanvas())
    return;

  scoped_refptr<Image> image;
  gfx::SizeF default_object_size(Width(), Height());
  SourceImageStatus source_image_status = kInvalidSourceImageStatus;
  if (!image_source->IsVideoElement()) {
    image = image_source->GetSourceImageForCanvas(
        CanvasResourceProvider::FlushReason::kDrawImage, &source_image_status,
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
  AdjustRectForCanvas(sx, sy, sw, sh);
  AdjustRectForCanvas(dx, dy, dw, dh);
  float fsx = ClampTo<float>(sx);
  float fsy = ClampTo<float>(sy);
  float fsw = ClampTo<float>(sw);
  float fsh = ClampTo<float>(sh);
  float fdx = ClampTo<float>(dx);
  float fdy = ClampTo<float>(dy);
  float fdw = ClampTo<float>(dw);
  float fdh = ClampTo<float>(dh);

  gfx::RectF src_rect(fsx, fsy, fsw, fsh);
  gfx::RectF dst_rect(fdx, fdy, fdw, fdh);
  gfx::SizeF image_size = image_source->ElementSize(
      default_object_size, RespectImageOrientationInternal(image_source));

  ClipRectsToImageRect(gfx::RectF(image_size), &src_rect, &dst_rect);

  if (src_rect.IsEmpty())
    return;
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kDrawImage, fsx, fsy, fsw, fsh, fdx, fdy, fdw, fdh,
        image ? image->width() : 0, image ? image->height() : 0);
    identifiability_study_helper_.set_encountered_partially_digested_image();
  }

  ValidateStateStack();

  WillDrawImage(image_source);

  if (!origin_tainted_by_content_ && WouldTaintOrigin(image_source))
    SetOriginTaintedByContent();

  Draw<OverdrawOp::kDrawImage>(
      [this, image_source, image, src_rect, dst_rect](
          cc::PaintCanvas* c, const cc::PaintFlags* flags)  // draw lambda
      {
        SkSamplingOptions sampling =
            cc::PaintFlags::FilterQualityToSkSamplingOptions(
                flags ? flags->getFilterQuality()
                      : cc::PaintFlags::FilterQuality::kNone);
        DrawImageInternal(c, image_source, image.get(), src_rect, dst_rect,
                          sampling, flags);
      },
      [this, dst_rect](const SkIRect& clip_bounds)  // overdraw test lambda
      { return RectContainsTransformedRect(dst_rect, clip_bounds); },
      dst_rect, CanvasRenderingContext2DState::kImagePaintType,
      image_source->IsOpaque() ? CanvasRenderingContext2DState::kOpaqueImage
                               : CanvasRenderingContext2DState::kNonOpaqueImage,
      CanvasPerformanceMonitor::DrawType::kImage);
}

bool BaseRenderingContext2D::RectContainsTransformedRect(
    const gfx::RectF& rect,
    const SkIRect& transformed_rect) const {
  gfx::QuadF quad(rect);
  gfx::QuadF transformed_quad(
      gfx::RectF(transformed_rect.x(), transformed_rect.y(),
                 transformed_rect.width(), transformed_rect.height()));
  return GetState().GetTransform().MapQuad(quad).ContainsQuad(transformed_quad);
}

CanvasGradient* BaseRenderingContext2D::createLinearGradient(double x0,
                                                             double y0,
                                                             double x1,
                                                             double y1) {
  if (!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(x1) ||
      !std::isfinite(y1))
    return nullptr;

  // clamp to float to avoid float cast overflow
  float fx0 = ClampTo<float>(x0);
  float fy0 = ClampTo<float>(y0);
  float fx1 = ClampTo<float>(x1);
  float fy1 = ClampTo<float>(y1);

  auto* gradient = MakeGarbageCollected<CanvasGradient>(gfx::PointF(fx0, fy0),
                                                        gfx::PointF(fx1, fy1));
  gradient->SetExecutionContext(
      identifiability_study_helper_.execution_context());
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
  float fx0 = ClampTo<float>(x0);
  float fy0 = ClampTo<float>(y0);
  float fr0 = ClampTo<float>(r0);
  float fx1 = ClampTo<float>(x1);
  float fy1 = ClampTo<float>(y1);
  float fr1 = ClampTo<float>(r1);

  auto* gradient = MakeGarbageCollected<CanvasGradient>(
      gfx::PointF(fx0, fy0), fr0, gfx::PointF(fx1, fy1), fr1);
  gradient->SetExecutionContext(
      identifiability_study_helper_.execution_context());
  return gradient;
}

CanvasGradient* BaseRenderingContext2D::createConicGradient(double startAngle,
                                                            double centerX,
                                                            double centerY) {
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DConicGradient);
  if (!std::isfinite(startAngle) || !std::isfinite(centerX) ||
      !std::isfinite(centerY))
    return nullptr;
  // TODO(crbug.com/1234113): Instrument new canvas APIs.
  identifiability_study_helper_.set_encountered_skipped_ops();

  // clamp to float to avoid float cast overflow
  float a = ClampTo<float>(startAngle);
  float x = ClampTo<float>(centerX);
  float y = ClampTo<float>(centerY);

  // convert |startAngle| from radians to degree and rotate 90 degree, so
  // |startAngle| at 0 starts from x-axis.
  a = Rad2deg(a) + 90;

  auto* gradient = MakeGarbageCollected<CanvasGradient>(a, gfx::PointF(x, y));
  gradient->SetExecutionContext(
      identifiability_study_helper_.execution_context());
  return gradient;
}

CanvasPattern* BaseRenderingContext2D::createPattern(

    const V8CanvasImageSource* image_source,
    const String& repetition_type,
    ExceptionState& exception_state) {
  CanvasImageSource* image_source_internal =
      ToCanvasImageSource(image_source, exception_state);
  if (!image_source_internal) {
    return nullptr;
  }

  return createPattern(image_source_internal, repetition_type, exception_state);
}

CanvasPattern* BaseRenderingContext2D::createPattern(
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

  gfx::SizeF default_object_size(Width(), Height());
  scoped_refptr<Image> image_for_rendering =
      image_source->GetSourceImageForCanvas(
          CanvasResourceProvider::FlushReason::kCreatePattern, &status,
          default_object_size);

  switch (status) {
    case kNormalSourceImageStatus:
      break;
    case kZeroSizeCanvasSourceImageStatus:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          String::Format("The canvas %s is 0.",
                         image_source
                                 ->ElementSize(default_object_size,
                                               RespectImageOrientationInternal(
                                                   image_source))
                                 .width()
                             ? "height"
                             : "width"));
      return nullptr;
    case kZeroSizeImageSourceStatus:
      return nullptr;
    case kUndecodableSourceImageStatus:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Source image is in the 'broken' state.");
      return nullptr;
    case kInvalidSourceImageStatus:
      image_for_rendering = BitmapImage::Create();
      break;
    case kIncompleteSourceImageStatus:
      return nullptr;
    default:
      NOTREACHED();
      return nullptr;
  }

  if (!image_for_rendering)
    return nullptr;

  bool origin_clean = !WouldTaintOrigin(image_source);

  auto* pattern = MakeGarbageCollected<CanvasPattern>(
      std::move(image_for_rendering), repeat_mode, origin_clean);
  pattern->SetExecutionContext(
      identifiability_study_helper_.execution_context());
  return pattern;
}

bool BaseRenderingContext2D::ComputeDirtyRect(const gfx::RectF& local_rect,
                                              SkIRect* dirty_rect) {
  SkIRect clip_bounds;
  cc::PaintCanvas* paint_canvas = GetOrCreatePaintCanvas();
  if (!paint_canvas || !paint_canvas->getDeviceClipBounds(&clip_bounds))
    return false;
  return ComputeDirtyRect(local_rect, clip_bounds, dirty_rect);
}

ImageData* BaseRenderingContext2D::createImageData(
    ImageData* image_data,
    ExceptionState& exception_state) const {
  ImageData::ValidateAndCreateParams params;
  params.context_2d_error_mode = true;
  return ImageData::ValidateAndCreate(
      image_data->Size().width(), image_data->Size().height(), absl::nullopt,
      image_data->getSettings(), params, exception_state);
}

ImageData* BaseRenderingContext2D::createImageData(
    int sw,
    int sh,
    ExceptionState& exception_state) const {
  ImageData::ValidateAndCreateParams params;
  params.context_2d_error_mode = true;
  params.default_color_space = GetDefaultImageDataColorSpace();
  return ImageData::ValidateAndCreate(std::abs(sw), std::abs(sh), absl::nullopt,
                                      /*settings=*/nullptr, params,
                                      exception_state);
}

ImageData* BaseRenderingContext2D::createImageData(
    int sw,
    int sh,
    ImageDataSettings* image_data_settings,
    ExceptionState& exception_state) const {
  ImageData::ValidateAndCreateParams params;
  params.context_2d_error_mode = true;
  params.default_color_space = GetDefaultImageDataColorSpace();
  return ImageData::ValidateAndCreate(std::abs(sw), std::abs(sh), absl::nullopt,
                                      image_data_settings, params,
                                      exception_state);
}

ImageData* BaseRenderingContext2D::getImageData(
    int sx,
    int sy,
    int sw,
    int sh,
    ExceptionState& exception_state) {
  return getImageDataInternal(sx, sy, sw, sh, /*image_data_settings=*/nullptr,
                              exception_state);
}

ImageData* BaseRenderingContext2D::getImageData(
    int sx,
    int sy,
    int sw,
    int sh,
    ImageDataSettings* image_data_settings,
    ExceptionState& exception_state) {
  return getImageDataInternal(sx, sy, sw, sh, image_data_settings,
                              exception_state);
}

ImageData* BaseRenderingContext2D::getImageDataInternal(
    int sx,
    int sy,
    int sw,
    int sh,
    ImageDataSettings* image_data_settings,
    ExceptionState& exception_state) {
  if (!base::CheckMul(sw, sh).IsValid<int>()) {
    exception_state.ThrowRangeError("Out of memory at ImageData creation");
    return nullptr;
  }

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
    sw = base::saturated_cast<int>(base::SafeUnsignedAbs(sw));
  }
  if (sh < 0) {
    if (!base::CheckAdd(sy, sh).IsValid<int>()) {
      exception_state.ThrowRangeError("Out of memory at ImageData creation");
      return nullptr;
    }
    sy += sh;
    sh = base::saturated_cast<int>(base::SafeUnsignedAbs(sh));
  }

  if (!base::CheckAdd(sx, sw).IsValid<int>() ||
      !base::CheckAdd(sy, sh).IsValid<int>()) {
    exception_state.ThrowRangeError("Out of memory at ImageData creation");
    return nullptr;
  }

  const gfx::Rect image_data_rect(sx, sy, sw, sh);

  ImageData::ValidateAndCreateParams validate_and_create_params;
  validate_and_create_params.context_2d_error_mode = true;
  validate_and_create_params.default_color_space =
      GetDefaultImageDataColorSpace();

  if (UNLIKELY(!CanCreateCanvas2dResourceProvider() || isContextLost())) {
    return ImageData::ValidateAndCreate(
        sw, sh, absl::nullopt, image_data_settings, validate_and_create_params,
        exception_state);
  }

  // Deferred offscreen canvases might have recorded commands, make sure
  // that those get drawn here
  FinalizeFrame(CanvasResourceProvider::FlushReason::kGetImageData);

  num_readbacks_performed_++;
  CanvasContextCreationAttributesCore::WillReadFrequently
      will_read_frequently_value = GetCanvasRenderingContextHost()
                                       ->RenderingContext()
                                       ->CreationAttributes()
                                       .will_read_frequently;
  if (num_readbacks_performed_ == 2 && GetCanvasRenderingContextHost() &&
      GetCanvasRenderingContextHost()->RenderingContext()) {
    if (will_read_frequently_value == CanvasContextCreationAttributesCore::
                                          WillReadFrequently::kUndefined) {
      const String& message =
          "Canvas2D: Multiple readback operations using getImageData are "
          "faster with the willReadFrequently attribute set to true. See: "
          "https://html.spec.whatwg.org/multipage/"
          "canvas.html#concept-canvas-will-read-frequently";
      GetTopExecutionContext()->AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kRendering,
              mojom::blink::ConsoleMessageLevel::kWarning, message));
    }
  }

  // The default behavior before the willReadFrequently feature existed:
  // Accelerated canvases fall back to CPU when there is a readback.
  if (will_read_frequently_value ==
      CanvasContextCreationAttributesCore::WillReadFrequently::kUndefined) {
    // GetImageData is faster in Unaccelerated canvases.
    // In Desynchronized canvas disabling the acceleration will break
    // putImageData: crbug.com/1112060.
    if (IsAccelerated() && !IsDesynchronized()) {
      read_count_++;
      if (read_count_ >= kFallbackToCPUAfterReadbacks ||
          ShouldDisableAccelerationBecauseOfReadback()) {
        DisableAcceleration();
        base::UmaHistogramEnumeration("Blink.Canvas.GPUFallbackToCPU",
                                      GPUFallbackToCPUScenario::kGetImageData);
      }
    }
  }

  scoped_refptr<StaticBitmapImage> snapshot =
      GetImage(CanvasResourceProvider::FlushReason::kGetImageData);

  // Determine if the array should be zero initialized, or if it will be
  // completely overwritten.
  validate_and_create_params.zero_initialize = false;
  if (IsAccelerated()) {
    // GPU readback may fail silently.
    validate_and_create_params.zero_initialize = true;
  } else if (snapshot) {
    // Zero-initialize if some of the readback area is out of bounds.
    if (image_data_rect.x() < 0 || image_data_rect.y() < 0 ||
        image_data_rect.right() > snapshot->Size().width() ||
        image_data_rect.bottom() > snapshot->Size().height()) {
      validate_and_create_params.zero_initialize = true;
    }
  }

  ImageData* image_data =
      ImageData::ValidateAndCreate(sw, sh, absl::nullopt, image_data_settings,
                                   validate_and_create_params, exception_state);
  if (!image_data)
    return nullptr;

  // Read pixels into |image_data|.
  if (snapshot) {
    SkPixmap image_data_pixmap = image_data->GetSkPixmap();
    const bool read_pixels_successful =
        snapshot->PaintImageForCurrentFrame().readPixels(
            image_data_pixmap.info(), image_data_pixmap.writable_addr(),
            image_data_pixmap.rowBytes(), sx, sy);
    if (!read_pixels_successful) {
      SkIRect bounds =
          snapshot->PaintImageForCurrentFrame().GetSkImageInfo().bounds();
      DCHECK(!bounds.intersect(SkIRect::MakeXYWH(sx, sy, sw, sh)));
    }
  }

  return image_data;
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

  if (data->IsBufferBaseDetached()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The source data has been detached.");
    return;
  }

  bool hasResourceProvider = CanCreateCanvas2dResourceProvider();
  if (!hasResourceProvider)
    return;

  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kPutImageData, data->width(), data->height(),
        data->GetPredefinedColorSpace(), data->GetImageDataStorageFormat(), dx,
        dy, dirty_x, dirty_y, dirty_width, dirty_height);
    identifiability_study_helper_.set_encountered_partially_digested_image();
  }

  if (dirty_width < 0) {
    if (dirty_x < 0) {
      dirty_x = dirty_width = 0;
    } else {
      dirty_x += dirty_width;
      dirty_width =
          base::saturated_cast<int>(base::SafeUnsignedAbs(dirty_width));
    }
  }

  if (dirty_height < 0) {
    if (dirty_y < 0) {
      dirty_y = dirty_height = 0;
    } else {
      dirty_y += dirty_height;
      dirty_height =
          base::saturated_cast<int>(base::SafeUnsignedAbs(dirty_height));
    }
  }

  gfx::Rect dest_rect(dirty_x, dirty_y, dirty_width, dirty_height);
  dest_rect.Intersect(gfx::Rect(0, 0, data->width(), data->height()));
  gfx::Vector2d dest_offset(static_cast<int>(dx), static_cast<int>(dy));
  dest_rect.Offset(dest_offset);
  dest_rect.Intersect(gfx::Rect(0, 0, Width(), Height()));
  if (dest_rect.IsEmpty())
    return;

  gfx::Rect source_rect = dest_rect;
  source_rect.Offset(-dest_offset);

  SkPixmap data_pixmap = data->GetSkPixmap();

  // WritePixels (called by PutByteArray) requires that the source and
  // destination pixel formats have the same bytes per pixel.
  if (auto* host = GetCanvasRenderingContextHost()) {
    SkColorType dest_color_type =
        host->GetRenderingContextSkColorInfo().colorType();
    if (SkColorTypeBytesPerPixel(dest_color_type) !=
        SkColorTypeBytesPerPixel(data_pixmap.colorType())) {
      SkImageInfo converted_info =
          data_pixmap.info().makeColorType(dest_color_type);
      SkBitmap converted_bitmap;
      if (!converted_bitmap.tryAllocPixels(converted_info)) {
        exception_state.ThrowRangeError("Out of memory in putImageData");
        return;
      }
      if (!converted_bitmap.writePixels(data_pixmap, 0, 0))
        NOTREACHED() << "Failed to convert ImageData with writePixels.";

      PutByteArray(converted_bitmap.pixmap(), source_rect, dest_offset);
      if (GetPaintCanvas()) {
        WillDraw(gfx::RectToSkIRect(dest_rect),
                 CanvasPerformanceMonitor::DrawType::kImageData);
      }
      return;
    }
  }

  PutByteArray(data_pixmap, source_rect, dest_offset);
  if (GetPaintCanvas()) {
    WillDraw(gfx::RectToSkIRect(dest_rect),
             CanvasPerformanceMonitor::DrawType::kImageData);
  }
}

void BaseRenderingContext2D::PutByteArray(const SkPixmap& source,
                                          const gfx::Rect& source_rect,
                                          const gfx::Vector2d& dest_offset) {
  if (!IsCanvas2DBufferValid())
    return;

  DCHECK(gfx::Rect(source.width(), source.height()).Contains(source_rect));
  int dest_x = dest_offset.x() + source_rect.x();
  DCHECK_GE(dest_x, 0);
  DCHECK_LT(dest_x, Width());
  int dest_y = dest_offset.y() + source_rect.y();
  DCHECK_GE(dest_y, 0);
  DCHECK_LT(dest_y, Height());

  SkImageInfo info =
      source.info().makeWH(source_rect.width(), source_rect.height());
  if (!HasAlpha()) {
    // If the surface is opaque, tell it that we are writing opaque
    // pixels.  Writing non-opaque pixels to opaque is undefined in
    // Skia.  There is some discussion about whether it should be
    // defined in skbug.com/6157.  For now, we can get the desired
    // behavior (memcpy) by pretending the write is opaque.
    info = info.makeAlphaType(kOpaque_SkAlphaType);
  } else {
    info = info.makeAlphaType(kUnpremul_SkAlphaType);
  }

  WritePixels(info, source.addr(source_rect.x(), source_rect.y()),
              source.rowBytes(), dest_x, dest_y);
}

void BaseRenderingContext2D::InflateStrokeRect(gfx::RectF& rect) const {
  // Fast approximation of the stroke's bounding rect.
  // This yields a slightly oversized rect but is very fast
  // compared to Path::strokeBoundingRect().
  static const double kRoot2 = sqrtf(2);
  double delta = GetState().LineWidth() / 2;
  if (GetState().GetLineJoin() == kMiterJoin)
    delta *= GetState().MiterLimit();
  else if (GetState().GetLineCap() == kSquareCap)
    delta *= kRoot2;

  rect.Outset(ClampTo<float>(delta));
}

bool BaseRenderingContext2D::imageSmoothingEnabled() const {
  return GetState().ImageSmoothingEnabled();
}

void BaseRenderingContext2D::setImageSmoothingEnabled(bool enabled) {
  if (enabled == GetState().ImageSmoothingEnabled())
    return;
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kSetImageSmoothingEnabled, enabled);
  }

  GetState().SetImageSmoothingEnabled(enabled);
}

String BaseRenderingContext2D::imageSmoothingQuality() const {
  return GetState().ImageSmoothingQuality();
}

void BaseRenderingContext2D::setImageSmoothingQuality(const String& quality) {
  if (quality == GetState().ImageSmoothingQuality())
    return;

  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kSetImageSmoothingQuality,
        IdentifiabilitySensitiveStringToken(quality));
  }
  GetState().SetImageSmoothingQuality(quality);
}

String BaseRenderingContext2D::letterSpacing() const {
  return GetState().GetLetterSpacing();
}

String BaseRenderingContext2D::wordSpacing() const {
  return GetState().GetWordSpacing();
}

String BaseRenderingContext2D::textRendering() const {
  return ToStringForIdl(GetState().GetTextRendering());
}

float BaseRenderingContext2D::GetFontBaseline(
    const SimpleFontData& font_data) const {
  return TextMetrics::GetFontBaseline(GetState().GetTextBaseline(), font_data);
}

String BaseRenderingContext2D::textAlign() const {
  return TextAlignName(GetState().GetTextAlign());
}

void BaseRenderingContext2D::setTextAlign(const String& s) {
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kSetTextAlign, IdentifiabilityBenignStringToken(s));
  }
  TextAlign align;
  if (!ParseTextAlign(s, align))
    return;
  if (GetState().GetTextAlign() == align)
    return;
  GetState().SetTextAlign(align);
}

String BaseRenderingContext2D::textBaseline() const {
  return TextBaselineName(GetState().GetTextBaseline());
}

void BaseRenderingContext2D::setTextBaseline(const String& s) {
  if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kSetTextBaseline, IdentifiabilityBenignStringToken(s));
  }
  TextBaseline baseline;
  if (!ParseTextBaseline(s, baseline))
    return;
  if (GetState().GetTextBaseline() == baseline)
    return;
  GetState().SetTextBaseline(baseline);
}

String BaseRenderingContext2D::fontKerning() const {
  return FontDescription::ToString(GetState().GetFontKerning()).LowerASCII();
}

String BaseRenderingContext2D::fontStretch() const {
  return FontDescription::ToString(GetState().GetFontStretch()).LowerASCII();
}

String BaseRenderingContext2D::fontVariantCaps() const {
  return FontDescription::ToStringForIdl(GetState().GetFontVariantCaps());
}

void BaseRenderingContext2D::Trace(Visitor* visitor) const {
  visitor->Trace(state_stack_);
  visitor->Trace(dispatch_context_lost_event_timer_);
  visitor->Trace(dispatch_context_restored_event_timer_);
  visitor->Trace(try_restore_context_event_timer_);
  CanvasPath::Trace(visitor);
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

namespace {

void CanvasOverdrawHistogram(BaseRenderingContext2D::OverdrawOp op) {
  UMA_HISTOGRAM_ENUMERATION("Blink.Canvas.OverdrawOp", op);
}

}  // unnamed namespace

void BaseRenderingContext2D::WillOverwriteCanvas(
    BaseRenderingContext2D::OverdrawOp op) {
  auto* host = GetCanvasRenderingContextHost();
  if (host) {  // CSS paint use cases not counted.
    UseCounter::Count(GetTopExecutionContext(),
                      WebFeature::kCanvasRenderingContext2DHasOverdraw);
    CanvasOverdrawHistogram(op);
    CanvasOverdrawHistogram(OverdrawOp::kTotal);
  }

  // We only hit the kHasTransform bucket if the op is affected by transforms.
  if (op == OverdrawOp::kClearRect || op == OverdrawOp::kDrawImage) {
    bool has_clip = GetState().HasClip();
    bool has_transform = !GetState().GetTransform().IsIdentity();
    if (has_clip && has_transform) {
      CanvasOverdrawHistogram(OverdrawOp::kHasClipAndTransform);
    }
    if (has_clip) {
      CanvasOverdrawHistogram(OverdrawOp::kHasClip);
    }
    if (has_transform) {
      CanvasOverdrawHistogram(OverdrawOp::kHasTransform);
    }
  }

  WillOverwriteCanvas();
}

}  // namespace blink
