// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_2d_recorder_context.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <optional>
#include <ostream>  // IWYU pragma: keep (https://github.com/clangd/clangd/issues/2053)
#include <string>  // IWYU pragma: keep (for String::Utf8())
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/record_paint_canvas.h"
#include "cc/paint/refcounted_buffer.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_transformation.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/color_scheme.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_object_objectarray_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_begin_layer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_2d_gpu_transfer_option.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_fill_rule.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_canvasfilter_string.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/text_link_colors.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix_read_only.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_performance_monitor.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/paint/filter_effect_builder.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/filter_operations.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/cached_color.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_gradient.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_image_source_util.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_path.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_pattern.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d_state.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/mesh_2d_index_buffer.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/mesh_2d_uv_buffer.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/mesh_2d_vertex_buffer.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/path_2d.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/v8_canvas_style.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/geometry/skia_geometry_utils.h"
#include "third_party/blink/renderer/platform/geometry/stroke_data.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/blend_mode.h"
#include "third_party/blink/renderer/platform/graphics/canvas_high_entropy_op_type.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/interpolation_space.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_filter.h"
#include "third_party/blink/renderer/platform/graphics/pattern.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkClipOp.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathTypes.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSamplingOptions.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/private/base/SkTo.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "v8/include/v8-local-handle.h"

// UMA Histogram macros trigger a bug in IWYU.
// https://github.com/include-what-you-use/include-what-you-use/issues/1546
// IWYU pragma: no_include <atomic>
// IWYU pragma: no_include <string_view>
// IWYU pragma: no_include "base/metrics/histogram_base.h"

// `base::HashingLRUCache` uses a std::list internally and a bug in IWYU leaks
// that implementation detail.
// https://github.com/include-what-you-use/include-what-you-use/issues/1539
// IWYU pragma: no_include <list>

enum SkColorType : int;

namespace gpu {
struct Mailbox;
}  // namespace gpu

namespace v8 {
class Isolate;
class Value;
}  // namespace v8

namespace blink {

class DOMMatrixInit;
class ImageDataSettings;
class ScriptState;
class SimpleFontData;

using ::cc::UsePaintCache;

namespace {

constexpr auto kCanvasCompositeOperatorNames = std::to_array<const char* const>(
    {"clear", "copy", "source-over", "source-in", "source-out", "source-atop",
     "destination-over", "destination-in", "destination-out",
     "destination-atop", "xor", "lighter"});

constexpr auto kCanvasBlendModeNames = std::to_array<const char* const>(
    {"normal", "multiply", "screen", "overlay", "darken", "lighten",
     "color-dodge", "color-burn", "hard-light", "soft-light", "difference",
     "exclusion", "hue", "saturation", "color", "luminosity"});

bool ParseCanvasCompositeAndBlendMode(const String& s,
                                      CompositeOperator& op,
                                      BlendMode& blend_op) {
  if (auto it = std::ranges::find(kCanvasCompositeOperatorNames, s);
      it != kCanvasCompositeOperatorNames.end()) {
    op = static_cast<CompositeOperator>(
        std::distance(kCanvasCompositeOperatorNames.begin(), it));
    blend_op = BlendMode::kNormal;
    return true;
  }

  if (auto it = std::ranges::find(kCanvasBlendModeNames, s);
      it != kCanvasBlendModeNames.end()) {
    blend_op = static_cast<BlendMode>(
        std::distance(kCanvasBlendModeNames.begin(), it));
    op = kCompositeSourceOver;
    return true;
  }

  return false;
}

String CanvasCompositeOperatorName(CompositeOperator op, BlendMode blend_op) {
  DCHECK_GE(op, 0);
  DCHECK_LT(op, kCanvasCompositeOperatorNames.size());
  DCHECK_GE(static_cast<int>(blend_op), 0);
  DCHECK_LT(static_cast<size_t>(blend_op), kCanvasBlendModeNames.size());
  if (blend_op != BlendMode::kNormal) {
    return kCanvasBlendModeNames[static_cast<unsigned>(blend_op)];
  }
  return kCanvasCompositeOperatorNames[op];
}

std::pair<CompositeOperator, BlendMode> CompositeAndBlendOpsFromSkBlendMode(
    SkBlendMode sk_blend_mode) {
  CompositeOperator composite_op = kCompositeSourceOver;
  BlendMode blend_mode = BlendMode::kNormal;
  switch (sk_blend_mode) {
    // The following are SkBlendMode values that map to CompositeOperators.
    case SkBlendMode::kClear:
      composite_op = kCompositeClear;
      break;
    case SkBlendMode::kSrc:
      composite_op = kCompositeCopy;
      break;
    case SkBlendMode::kSrcOver:
      composite_op = kCompositeSourceOver;
      break;
    case SkBlendMode::kDstOver:
      composite_op = kCompositeDestinationOver;
      break;
    case SkBlendMode::kSrcIn:
      composite_op = kCompositeSourceIn;
      break;
    case SkBlendMode::kDstIn:
      composite_op = kCompositeDestinationIn;
      break;
    case SkBlendMode::kSrcOut:
      composite_op = kCompositeSourceOut;
      break;
    case SkBlendMode::kDstOut:
      composite_op = kCompositeDestinationOut;
      break;
    case SkBlendMode::kSrcATop:
      composite_op = kCompositeSourceAtop;
      break;
    case SkBlendMode::kDstATop:
      composite_op = kCompositeDestinationAtop;
      break;
    case SkBlendMode::kXor:
      composite_op = kCompositeXOR;
      break;
    case SkBlendMode::kPlus:
      composite_op = kCompositePlusLighter;
      break;

    // The following are SkBlendMode values that map to BlendModes.
    case SkBlendMode::kScreen:
      blend_mode = BlendMode::kScreen;
      break;
    case SkBlendMode::kOverlay:
      blend_mode = BlendMode::kOverlay;
      break;
    case SkBlendMode::kDarken:
      blend_mode = BlendMode::kDarken;
      break;
    case SkBlendMode::kLighten:
      blend_mode = BlendMode::kLighten;
      break;
    case SkBlendMode::kColorDodge:
      blend_mode = BlendMode::kColorDodge;
      break;
    case SkBlendMode::kColorBurn:
      blend_mode = BlendMode::kColorBurn;
      break;
    case SkBlendMode::kHardLight:
      blend_mode = BlendMode::kHardLight;
      break;
    case SkBlendMode::kSoftLight:
      blend_mode = BlendMode::kSoftLight;
      break;
    case SkBlendMode::kDifference:
      blend_mode = BlendMode::kDifference;
      break;
    case SkBlendMode::kExclusion:
      blend_mode = BlendMode::kExclusion;
      break;
    case SkBlendMode::kMultiply:
      blend_mode = BlendMode::kMultiply;
      break;
    case SkBlendMode::kHue:
      blend_mode = BlendMode::kHue;
      break;
    case SkBlendMode::kSaturation:
      blend_mode = BlendMode::kSaturation;
      break;
    case SkBlendMode::kColor:
      blend_mode = BlendMode::kColor;
      break;
    case SkBlendMode::kLuminosity:
      blend_mode = BlendMode::kLuminosity;
      break;

    // We don't handle other SkBlendModes.
    default:
      break;
  }
  return std::make_pair(composite_op, blend_mode);
}

bool ParseLineCap(const String& s, LineCap& cap) {
  if (s == "butt") {
    cap = kButtCap;
    return true;
  }
  if (s == "round") {
    cap = kRoundCap;
    return true;
  }
  if (s == "square") {
    cap = kSquareCap;
    return true;
  }
  return false;
}

String LineCapName(LineCap cap) {
  DCHECK_GE(cap, 0);
  DCHECK_LT(cap, 3);
  constexpr std::array<const char* const, 3> kNames = {"butt", "round",
                                                       "square"};
  return kNames[cap];
}

bool ParseLineJoin(const String& s, LineJoin& join) {
  if (s == "miter") {
    join = kMiterJoin;
    return true;
  }
  if (s == "round") {
    join = kRoundJoin;
    return true;
  }
  if (s == "bevel") {
    join = kBevelJoin;
    return true;
  }
  return false;
}

String LineJoinName(LineJoin join) {
  DCHECK_GE(join, 0);
  DCHECK_LT(join, 3);
  constexpr std::array<const char* const, 3> kNames = {"miter", "round",
                                                       "bevel"};
  return kNames[join];
}

}  // namespace

// Maximum number of colors in the color cache
// (`Canvas2DRecorderContext::color_cache_`).
constexpr size_t kColorCacheMaxSize = 8;

Canvas2DRecorderContext::Canvas2DRecorderContext(float effective_zoom)
    : effective_zoom_(effective_zoom),
      path2d_use_paint_cache_(
          base::FeatureList::IsEnabled(features::kPath2DPaintCache)
              ? UsePaintCache::kEnabled
              : UsePaintCache::kDisabled) {
  state_stack_.push_back(MakeGarbageCollected<CanvasRenderingContext2DState>());
}

Canvas2DRecorderContext::~Canvas2DRecorderContext() {
  UMA_HISTOGRAM_CUSTOM_COUNTS("Blink.Canvas.MaximumStateStackDepth",
                              max_state_stack_depth_, 1, 33, 32);
}

void Canvas2DRecorderContext::save() {
  if (isContextLost()) [[unlikely]] {
    return;
  }

  ValidateStateStack();

  // GetOrCreatePaintCanvas() can call RestoreMatrixClipStack which syncs
  // canvas to state_stack_. Get the canvas before adjusting state_stack_ to
  // ensure canvas is synced prior to adjusting state_stack_.
  cc::PaintCanvas* canvas = GetOrCreatePaintCanvas();

  state_stack_.push_back(MakeGarbageCollected<CanvasRenderingContext2DState>(
      GetState(), CanvasRenderingContext2DState::kDontCopyClipList,
      CanvasRenderingContext2DState::SaveType::kSaveRestore));
  max_state_stack_depth_ =
      std::max(state_stack_.size(), max_state_stack_depth_);

  if (canvas) {
    canvas->save();
  }

  ValidateStateStack();
}

void Canvas2DRecorderContext::restore(ExceptionState& exception_state) {
  if (isContextLost()) [[unlikely]] {
    return;
  }

  ValidateStateStack();
  if (state_stack_.size() <= 1) {
    // State stack is empty. Extra `restore()` are silently ignored.
    return;
  }

  // Verify that the top of the stack was pushed with Save.
  if (GetState().GetSaveType() !=
      CanvasRenderingContext2DState::SaveType::kSaveRestore) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Called `restore()` with no matching `save()` inside layer.");
    return;
  }

  if (cc::PaintCanvas* canvas = GetOrCreatePaintCanvas()) {
    canvas->restore();
  }

  PopStateStack();
  ValidateStateStack();
}

void Canvas2DRecorderContext::beginLayerImpl(ScriptState* script_state,
                                             const BeginLayerOptions* options,
                                             ExceptionState* exception_state) {
  if (isContextLost()) [[unlikely]] {
    return;
  }

  // Make sure we have a recorder and paint canvas.
  if (!GetOrCreatePaintCanvas()) {
    return;
  }

  MemoryManagedPaintRecorder* recorder = Recorder();
  if (!recorder) {
    return;
  }

  ValidateStateStack();

  sk_sp<PaintFilter> filter;
  if (options != nullptr) {
    CHECK(exception_state != nullptr);
    if (const V8CanvasFilterInput* filter_input = options->filter();
        filter_input != nullptr) {
      AddLayerFilterUserCount(filter_input);

      HTMLCanvasElement* canvas_for_filter = HostAsHTMLCanvasElement();
      FilterOperations filter_operations = CanvasFilter::CreateFilterOperations(
          *filter_input, AccessFont(canvas_for_filter), canvas_for_filter,
          CHECK_DEREF(ExecutionContext::From(script_state)), *exception_state);
      if (exception_state->HadException()) {
        return;
      }

      const gfx::SizeF canvas_viewport(Width(), Height());
      FilterEffectBuilder filter_effect_builder(
          gfx::RectF(canvas_viewport), canvas_viewport,
          1.0f,  // Deliberately ignore zoom on the canvas element.
          Color::kBlack, mojom::blink::ColorScheme::kLight);

      filter = paint_filter_builder::Build(
          filter_effect_builder.BuildFilterEffect(std::move(filter_operations),
                                                  !OriginClean()),
          kInterpolationSpaceSRGB);
    }
  }

  if (layer_count_ == 0) {
    recorder->BeginSideRecording();
  }

  ++layer_count_;

  // Layers are recorded on a side canvas to allow flushes with unclosed layers.
  // When calling `BeginSideRecording()` for the top level layer,
  // `getRecordingCanvas()` goes from returning the main canvas to returning the
  // side canvas storing layer content.
  cc::PaintCanvas& layer_canvas = recorder->getRecordingCanvas();

  const CanvasRenderingContext2DState& state = GetState();
  CanvasRenderingContext2DState::SaveType save_type =
      SaveLayerForState(state, filter, layer_canvas);
  state_stack_.push_back(MakeGarbageCollected<CanvasRenderingContext2DState>(
      state, CanvasRenderingContext2DState::kDontCopyClipList, save_type));
  max_state_stack_depth_ =
      std::max(state_stack_.size(), max_state_stack_depth_);

  ValidateStateStack();

  // Reset compositing attributes.
  setShadowOffsetX(0);
  setShadowOffsetY(0);
  setShadowBlur(0);
  CanvasRenderingContext2DState& layer_state = GetState();
  layer_state.SetShadowColor(Color::kTransparent);
  DCHECK(!layer_state.ShouldDrawShadows());
  setGlobalAlpha(1.0);
  setGlobalCompositeOperation("source-over");
  setFilter(script_state,
            MakeGarbageCollected<V8UnionCanvasFilterOrString>("none"));
}

void Canvas2DRecorderContext::AddLayerFilterUserCount(
    const V8CanvasFilterInput* filter_input) {
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvas2DLayersFilters);
  if (filter_input->GetContentType() ==
      V8CanvasFilterInput::ContentType::kString) {
    UseCounter::Count(GetTopExecutionContext(),
                      WebFeature::kCanvas2DLayersCSSFilters);
  } else {
    UseCounter::Count(GetTopExecutionContext(),
                      WebFeature::kCanvas2DLayersFilterObjects);
  }
}

class ScopedResetCtm {
 public:
  ScopedResetCtm(const CanvasRenderingContext2DState& state,
                 cc::PaintCanvas& canvas)
      : canvas_(canvas) {
    if (!state.GetTransform().IsIdentity()) {
      ctm_to_restore_ = canvas_->getLocalToDevice();
      canvas_->save();
      canvas_->setMatrix(SkM44());
    }
  }
  ~ScopedResetCtm() {
    if (ctm_to_restore_.has_value()) {
      canvas_->setMatrix(*ctm_to_restore_);
    }
  }

 private:
  const raw_ref<cc::PaintCanvas> canvas_;
  std::optional<SkM44> ctm_to_restore_;
};

namespace {
sk_sp<PaintFilter> CombineFilters(sk_sp<PaintFilter> first,
                                  sk_sp<PaintFilter> second) {
  if (second) {
    return sk_make_sp<ComposePaintFilter>(std::move(first), std::move(second));
  }
  return first;
}
}  // namespace

CanvasRenderingContext2DState::SaveType
Canvas2DRecorderContext::SaveLayerForState(
    const CanvasRenderingContext2DState& state,
    sk_sp<PaintFilter> layer_filter,
    cc::PaintCanvas& canvas) {
  if (!IsTransformInvertible()) {
    canvas.saveLayerAlphaf(1.0f);
    return CanvasRenderingContext2DState::SaveType::kBeginEndLayerOneSave;
  }

  const int initial_save_count = canvas.getSaveCount();
  bool needs_compositing = state.GlobalComposite() != SkBlendMode::kSrcOver;
  sk_sp<PaintFilter> context_filter = StateGetFilter();

  // The "copy" globalCompositeOperation replaces everything that was in the
  // canvas. We therefore have to clear the canvas before proceeding. Since the
  // shadow and foreground are composited one after the other, the foreground
  // gets composited over the shadow itself. This means that in "copy"
  // compositing mode, drawing the foreground will clear the shadow. There's
  // therefore no need to draw the shadow at all.
  //
  // Global states must be applied on the result of the layer's filter, so the
  // filter has to go in a nested layer.
  //
  // For globalAlpha + (shadows or compositing), we must use two nested layers.
  // The inner one applies the alpha and the outer one applies the shadow and/or
  // compositing. This is needed to get a transparent foreground, as the alpha
  // would otherwise be applied to the result of foreground+background.
  if (state.GlobalComposite() == SkBlendMode::kSrc) {
    canvas.clear(HasAlpha() ? SkColors::kTransparent : SkColors::kBlack);
    if (context_filter) {
      ScopedResetCtm scoped_reset_ctm(state, canvas);
      cc::PaintFlags flags;
      flags.setImageFilter(std::move(context_filter));
      canvas.saveLayer(flags);
    }
    needs_compositing = false;
  } else if (bool should_draw_shadow = state.ShouldDrawShadows(),
             needs_composited_draw = BlendModeRequiresCompositedDraw(state);
             context_filter || should_draw_shadow || needs_composited_draw) {
    if (should_draw_shadow && (context_filter || needs_composited_draw)) {
      ScopedResetCtm scoped_reset_ctm(state, canvas);
      // According to the WHATWG spec, the shadow and foreground need to be
      // composited independently to the canvas, one after the other
      // (https://html.spec.whatwg.org/multipage/canvas.html#drawing-model).
      // This is done by drawing twice, once for the background and once more
      // for the foreground. For layers, we can do this by passing two filters
      // that will each do a composite pass of the input to the destination.
      // Passing `nullptr` for the second pass means no filter is applied to the
      // foreground.
      cc::PaintFlags flags;
      flags.setBlendMode(state.GlobalComposite());
      sk_sp<PaintFilter> shadow_filter =
          CombineFilters(state.ShadowOnlyImageFilter(), context_filter);
      canvas.saveLayerFilters(
          {{std::move(shadow_filter), std::move(context_filter)}}, flags);
    } else if (should_draw_shadow) {
      ScopedResetCtm scoped_reset_ctm(state, canvas);
      cc::PaintFlags flags;
      flags.setImageFilter(state.ShadowAndForegroundImageFilter());
      flags.setBlendMode(state.GlobalComposite());
      canvas.saveLayer(flags);
    } else if (context_filter) {
      ScopedResetCtm scoped_reset_ctm(state, canvas);
      cc::PaintFlags flags;
      flags.setBlendMode(state.GlobalComposite());
      flags.setImageFilter(std::move(context_filter));
      canvas.saveLayer(flags);
    } else {
      cc::PaintFlags flags;
      flags.setBlendMode(state.GlobalComposite());
      canvas.saveLayer(flags);
    }
    needs_compositing = false;
  }

  if (layer_filter || needs_compositing) {
    cc::PaintFlags flags;
    flags.setAlphaf(static_cast<float>(state.GlobalAlpha()));
    flags.setImageFilter(layer_filter);
    if (needs_compositing) {
      flags.setBlendMode(state.GlobalComposite());
    }
    canvas.saveLayer(flags);
  } else if (state.GlobalAlpha() != 1 ||
             initial_save_count == canvas.getSaveCount()) {
    canvas.saveLayerAlphaf(state.GlobalAlpha());
  }

  const int save_diff = canvas.getSaveCount() - initial_save_count;
  return CanvasRenderingContext2DState::LayerSaveCountToSaveType(save_diff);
}

void Canvas2DRecorderContext::endLayer(ExceptionState& exception_state) {
  if (isContextLost()) [[unlikely]] {
    return;
  }

  ValidateStateStack();
  if (state_stack_.size() <= 1 || layer_count_ <= 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Called `endLayer()` with no matching `beginLayer()`.");
    return;
  }

  // Verify that the top of the stack was pushed with `beginLayer`.
  if (!GetState().IsLayerSaveType()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Called `endLayer()` with no matching `beginLayer()` inside parent "
        "`save()`/`restore()` pair.");
    return;
  }

  // Make sure we have a recorder and paint canvas.
  if (!GetOrCreatePaintCanvas()) {
    return;
  }

  MemoryManagedPaintRecorder* recorder = Recorder();
  if (!recorder) {
    return;
  }

  cc::PaintCanvas& layer_canvas = recorder->getRecordingCanvas();
  for (int i = 0, to_restore = state_stack_.back()->LayerSaveCount();
       i < to_restore; ++i) {
    layer_canvas.restore();
  }

  PopStateStack();

  --layer_count_;
  if (layer_count_ == 0) {
    recorder->EndSideRecording();
  }

  // Layers are recorded on a side canvas to allow flushes with unclosed layers.
  // When calling `EndSideRecording()` for the lop layer, `getRecordingCanvas()`
  // goes from returning the side canvas storing the layers content to returning
  // the main canvas.
  cc::PaintCanvas& parent_canvas = recorder->getRecordingCanvas();
  SkIRect clip_bounds;
  if (parent_canvas.getDeviceClipBounds(&clip_bounds)) {
    WillDraw(clip_bounds, CanvasPerformanceMonitor::DrawType::kOther);
  }

  ValidateStateStack();
}

void Canvas2DRecorderContext::PopStateStack() {
  if (IsTransformInvertible() && !GetState().GetTransform().IsIdentity()) {
    GetModifiablePath().Transform(GetState().GetTransform());
  }

  state_stack_.pop_back();
  CanvasRenderingContext2DState& state = GetState();
  state.ClearResolvedFilter();

  SetIsTransformInvertible(state.IsTransformInvertible());
  if (IsTransformInvertible() && !GetState().GetTransform().IsIdentity()) {
    GetModifiablePath().Transform(state.GetTransform().Inverse());
  }
}

void Canvas2DRecorderContext::ValidateStateStackImpl(
    const cc::PaintCanvas* canvas) const {
  DCHECK_GE(state_stack_.size(), 1u);
  DCHECK_GT(state_stack_.size(), base::checked_cast<wtf_size_t>(layer_count_));

  using SaveType = CanvasRenderingContext2DState::SaveType;
  DCHECK_EQ(state_stack_[0]->GetSaveType(), SaveType::kInitial);

  int actual_layer_count = 0;
  int extra_layer_saves = 0;
  for (wtf_size_t i = 1; i < state_stack_.size(); ++i) {
    if (RuntimeEnabledFeatures::Canvas2dLayersEnabled()) {
      DCHECK_NE(state_stack_[i]->GetSaveType(), SaveType::kInitial);
    } else {
      DCHECK_EQ(state_stack_[i]->GetSaveType(), SaveType::kSaveRestore);
    }

    if (state_stack_[i]->IsLayerSaveType()) {
      ++actual_layer_count;
      extra_layer_saves += state_stack_[i]->LayerSaveCount() - 1;
    }
  }
  DCHECK_EQ(layer_count_, actual_layer_count);

  if (const MemoryManagedPaintRecorder* recorder = Recorder();
      recorder != nullptr) {
    if (canvas == nullptr) {
      canvas = &recorder->GetMainCanvas();
    }
    const cc::PaintCanvas* layer_canvas = recorder->GetSideCanvas();

    // The canvas should always have an initial save frame, to support
    // resetting the top level matrix and clip.
    DCHECK_GT(canvas->getSaveCount(), 1);

    if (context_lost_mode_ == CanvasRenderingContext::kNotLostContext) {
      // Recording canvases always starts with a baseline save that we have to
      // account for here.
      int main_saves = canvas->getSaveCount() - 1;
      int layer_saves = layer_canvas ? layer_canvas->getSaveCount() - 1 : 0;

      // The state stack depth should match the number of saves in the
      // recording (taking in to account that some layers require two saves).
      DCHECK_EQ(base::checked_cast<wtf_size_t>(main_saves + layer_saves),
                state_stack_.size() + extra_layer_saves);
    }
  }
}

void Canvas2DRecorderContext::RestoreMatrixClipStack(cc::PaintCanvas* c) const {
  if (!c) {
    return;
  }
  AffineTransform prev_transform;
  for (const Member<CanvasRenderingContext2DState>& curr_state : state_stack_) {
    if (curr_state->IsLayerSaveType()) {
      // Layers are stored in a separate recording that never gets flushed, so
      // we are done restoring the main recording.
      break;
    }

    c->save();

    if (curr_state->HasClip()) {
      if (!prev_transform.IsIdentity()) {
        c->setMatrix(SkM44());
        prev_transform = AffineTransform();
      }
      curr_state->PlaybackClips(c);
    }

    if (AffineTransform curr_transform = curr_state->GetTransform();
        prev_transform != curr_transform) {
      c->setMatrix(curr_transform.ToSkM44());
      prev_transform = curr_transform;
    }
  }
  ValidateStateStack(c);
}

void Canvas2DRecorderContext::ResetInternal() {
  ValidateStateStack();
  state_stack_.resize(1);
  state_stack_.front() = MakeGarbageCollected<CanvasRenderingContext2DState>();
  layer_count_ = 0;
  SetIsTransformInvertible(true);

  CanvasPath::Clear();
  if (MemoryManagedPaintRecorder* recorder = Recorder(); recorder != nullptr) {
    recorder->RestartRecording();
  }

  // Clear the frame in case a flush previously drew to the canvas surface.
  if (cc::PaintCanvas* c = GetPaintCanvas()) {
    int width = Width();  // Keeping results to avoid repetitive virtual calls.
    int height = Height();
    WillDraw(SkIRect::MakeXYWH(0, 0, width, height),
             CanvasPerformanceMonitor::DrawType::kOther);
    c->drawRect(SkRect::MakeXYWH(0.0f, 0.0f, width, height), GetClearFlags());
  }

  ValidateStateStack();
  origin_tainted_by_content_ = false;
}

void Canvas2DRecorderContext::reset() {
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DReset);
  ResetInternal();
}

RespectImageOrientationEnum
Canvas2DRecorderContext::RespectImageOrientationInternal(
    CanvasImageSource* image_source) {
  if ((image_source->IsImageBitmap() || image_source->IsImageElement()) &&
      image_source->WouldTaintOrigin()) {
    return kRespectImageOrientation;
  }
  return RespectImageOrientation();
}

v8::Local<v8::Value> Canvas2DRecorderContext::strokeStyle(
    ScriptState* script_state) const {
  return CanvasStyleToV8(script_state, GetState().StrokeStyle());
}

bool Canvas2DRecorderContext::ExtractColorFromV8StringAndUpdateCache(
    v8::Isolate* isolate,
    v8::Local<v8::String> v8_string,
    ExceptionState& exception_state,
    Color& color) {
  // Internalize the string so that we can use pointer comparison for equality
  // rather than string comparison.
  v8_string = v8_string->InternalizeString(isolate);
  if (v8_string->Length()) {
    const auto it = color_cache_.Find<ColorCacheHashTranslator>(v8_string);
    if (it != color_cache_.end()) {
      color_cache_.MoveTo(it, color_cache_.begin());
      const CachedColor* cached_color = it->Get();
      switch (cached_color->parse_result) {
        case ColorParseResult::kColor:
          color = cached_color->color;
          return true;
        case ColorParseResult::kCurrentColor:
          color = GetCurrentColor();
          return true;
        case ColorParseResult::kColorFunction:
          // ParseColorOrCurrentColor() never returns kColorMix.
          NOTREACHED();
        case ColorParseResult::kParseFailed:
          return false;
      }
    }
  }
  // It's a bit unfortunate to create a string here, we should instead plumb
  // through a StringView.
  String color_string = NativeValueTraits<IDLString>::NativeValue(
      isolate, v8_string, exception_state);
  const ColorParseResult parse_result =
      ParseColorOrCurrentColor(color_string, color);
  if (v8_string->Length()) {
    // Limit the size of the cache.
    if (color_cache_.size() == kColorCacheMaxSize) {
      color_cache_.pop_back();
    }
    auto* cached_color = MakeGarbageCollected<CachedColor>(isolate, v8_string,
                                                           color, parse_result);
    color_cache_.InsertBefore(color_cache_.begin(), cached_color);
  }
  return parse_result != ColorParseResult::kParseFailed;
}

void Canvas2DRecorderContext::setStrokeStyle(v8::Isolate* isolate,
                                             v8::Local<v8::Value> value,
                                             ExceptionState& exception_state) {
  CanvasRenderingContext2DState& state = GetState();
  // Use of a string for the stroke is very common (and parsing the color
  // from the string is expensive) so we keep a map of string to color.
  if (value->IsString()) {
    v8::Local<v8::String> v8_string = value.As<v8::String>();
    if (state.IsUnparsedStrokeColor(v8_string)) {
      return;
    }
    Color parsed_color = Color::kTransparent;
    if (!ExtractColorFromV8StringAndUpdateCache(
            isolate, v8_string, exception_state, parsed_color)) {
      return;
    }
    if (state.StrokeStyle().IsEquivalentColor(parsed_color)) {
      state.SetUnparsedStrokeColor(isolate, v8_string);
      return;
    }
    state.SetStrokeColor(parsed_color);
    state.ClearUnparsedStrokeColor();
    state.ClearResolvedFilter();
    return;
  }

  // Use ExtractV8CanvasStyle to extract the other possible types. Note that
  // a string may still be returned. This is a fallback in cases where the
  // value can be converted to a string (such as an integer).
  V8CanvasStyle v8_style;
  if (!ExtractV8CanvasStyle(isolate, value, v8_style, exception_state)) {
    return;
  }

  switch (v8_style.type) {
    case V8CanvasStyleType::kCSSColorValue:
      state.SetStrokeColor(v8_style.css_color_value);
      break;
    case V8CanvasStyleType::kGradient:
      state.SetStrokeGradient(v8_style.gradient);
      break;
    case V8CanvasStyleType::kPattern:
      if (!origin_tainted_by_content_ && !v8_style.pattern->OriginClean()) {
        SetOriginTaintedByContent();
      }
      state.SetStrokePattern(v8_style.pattern);
      break;
    case V8CanvasStyleType::kString: {
      Color parsed_color = Color::kTransparent;
      if (ParseColorOrCurrentColor(v8_style.string, parsed_color) ==
          ColorParseResult::kParseFailed) {
        return;
      }
      if (!state.StrokeStyle().IsEquivalentColor(parsed_color)) {
        state.SetStrokeColor(parsed_color);
      }
      break;
    }
  }

  state.ClearUnparsedStrokeColor();
  state.ClearResolvedFilter();
}

ColorParseResult Canvas2DRecorderContext::ParseColorOrCurrentColor(
    const String& color_string,
    Color& color) const {
  const ColorParseResult parse_result =
      ParseCanvasColorString(color_string, color_scheme_, color,
                             GetColorProvider(), IsInWebAppScope());
  if (parse_result == ColorParseResult::kCurrentColor) {
    color = GetCurrentColor();
  }

  if (parse_result == ColorParseResult::kColorFunction) {
    const CSSValue* color_value = CSSParser::ParseSingleValue(
        CSSPropertyID::kColor, color_string,
        StrictCSSParserContext(SecureContextMode::kInsecureContext));

    if (!color_value) {
      return ColorParseResult::kParseFailed;
    }
    static const TextLinkColors kDefaultTextLinkColors{};
    auto* window = DynamicTo<LocalDOMWindow>(GetTopExecutionContext());
    const TextLinkColors& text_link_colors =
        window ? window->document()->GetTextLinkColors()
               : kDefaultTextLinkColors;
    // TODO(40946458): Don't use default length resolver here!
    const ResolveColorValueContext context{
        .conversion_data = CSSToLengthConversionData(/*element=*/nullptr),
        .text_link_colors = text_link_colors,
        .used_color_scheme = color_scheme_,
        .color_provider = GetColorProvider(),
        .is_in_web_app_scope = IsInWebAppScope()};
    const StyleColor style_color = ResolveColorValue(*color_value, context);
    color = style_color.Resolve(GetCurrentColor(), color_scheme_);
    return ColorParseResult::kColor;
  }
  return parse_result;
}

const ui::ColorProvider* Canvas2DRecorderContext::GetColorProvider() const {
  if (HTMLCanvasElement* canvas = HostAsHTMLCanvasElement()) {
    return canvas->GetDocument().GetColorProviderForPainting(color_scheme_);
  }

  return nullptr;
}

bool Canvas2DRecorderContext::IsInWebAppScope() const {
  if (HTMLCanvasElement* canvas = HostAsHTMLCanvasElement()) {
    return canvas->GetDocument().IsInWebAppScope();
  }
  return false;
}

v8::Local<v8::Value> Canvas2DRecorderContext::fillStyle(
    ScriptState* script_state) const {
  return CanvasStyleToV8(script_state, GetState().FillStyle());
}

void Canvas2DRecorderContext::setFillStyle(v8::Isolate* isolate,
                                           v8::Local<v8::Value> value,
                                           ExceptionState& exception_state) {
  ValidateStateStack();

  CanvasRenderingContext2DState& state = GetState();
  // This block is similar to that in setStrokeStyle(), see comments there for
  // details on this.
  if (value->IsString()) {
    v8::Local<v8::String> v8_string = value.As<v8::String>();
    if (state.IsUnparsedFillColor(v8_string)) {
      return;
    }
    Color parsed_color = Color::kTransparent;
    if (!ExtractColorFromV8StringAndUpdateCache(
            isolate, v8_string, exception_state, parsed_color)) {
      return;
    }
    if (state.FillStyle().IsEquivalentColor(parsed_color)) {
      state.SetUnparsedFillColor(isolate, v8_string);
      return;
    }
    state.SetFillColor(parsed_color);
    state.ClearUnparsedFillColor();
    state.ClearResolvedFilter();
    return;
  }
  V8CanvasStyle v8_style;
  if (!ExtractV8CanvasStyle(isolate, value, v8_style, exception_state)) {
    return;
  }

  switch (v8_style.type) {
    case V8CanvasStyleType::kCSSColorValue:
      state.SetFillColor(v8_style.css_color_value);
      break;
    case V8CanvasStyleType::kGradient:
      state.SetFillGradient(v8_style.gradient);
      break;
    case V8CanvasStyleType::kPattern:
      if (!origin_tainted_by_content_ && !v8_style.pattern->OriginClean()) {
        SetOriginTaintedByContent();
      }
      state.SetFillPattern(v8_style.pattern);
      break;
    case V8CanvasStyleType::kString: {
      Color parsed_color = Color::kTransparent;
      if (ParseColorOrCurrentColor(v8_style.string, parsed_color) ==
          ColorParseResult::kParseFailed) {
        return;
      }
      if (!state.FillStyle().IsEquivalentColor(parsed_color)) {
        state.SetFillColor(parsed_color);
      }
      break;
    }
  }

  state.ClearUnparsedFillColor();
  state.ClearResolvedFilter();
}

double Canvas2DRecorderContext::lineWidth() const {
  return GetState().LineWidth();
}

void Canvas2DRecorderContext::setLineWidth(double width) {
  if (!std::isfinite(width) || width <= 0) {
    return;
  }
  CanvasRenderingContext2DState& state = GetState();
  if (state.LineWidth() == width) {
    return;
  }
  state.SetLineWidth(ClampTo<float>(width));
}

String Canvas2DRecorderContext::lineCap() const {
  return LineCapName(GetState().GetLineCap());
}

void Canvas2DRecorderContext::setLineCap(const String& s) {
  LineCap cap;
  if (!ParseLineCap(s, cap)) {
    return;
  }
  CanvasRenderingContext2DState& state = GetState();
  if (state.GetLineCap() == cap) {
    return;
  }
  state.SetLineCap(cap);
}

String Canvas2DRecorderContext::lineJoin() const {
  return LineJoinName(GetState().GetLineJoin());
}

void Canvas2DRecorderContext::setLineJoin(const String& s) {
  LineJoin join;
  if (!ParseLineJoin(s, join)) {
    return;
  }
  CanvasRenderingContext2DState& state = GetState();
  if (state.GetLineJoin() == join) {
    return;
  }
  state.SetLineJoin(join);
}

double Canvas2DRecorderContext::miterLimit() const {
  return GetState().MiterLimit();
}

void Canvas2DRecorderContext::setMiterLimit(double limit) {
  if (!std::isfinite(limit) || limit <= 0) {
    return;
  }
  CanvasRenderingContext2DState& state = GetState();
  if (state.MiterLimit() == limit) {
    return;
  }
  state.SetMiterLimit(ClampTo<float>(limit));
}

// We need to account for the |effective_zoom_| for shadow effects, and not
// for line width. This is because the line width is affected by skia's current
// transform matrix (CTM) while shadows are not. The skia's CTM combines both
// the canvas context transform and the CSS layout transform. That means, the
// |effective_zoom_| is implicitly applied to line width through CTM.
double Canvas2DRecorderContext::shadowOffsetX() const {
  return GetState().ShadowOffset().x() / effective_zoom_;
}

void Canvas2DRecorderContext::setShadowOffsetX(double x) {
  x *= effective_zoom_;
  if (!std::isfinite(x)) {
    return;
  }
  CanvasRenderingContext2DState& state = GetState();
  if (state.ShadowOffset().x() == x) {
    return;
  }
  state.SetShadowOffsetX(ClampTo<float>(x));
}

double Canvas2DRecorderContext::shadowOffsetY() const {
  return GetState().ShadowOffset().y() / effective_zoom_;
}

void Canvas2DRecorderContext::setShadowOffsetY(double y) {
  y *= effective_zoom_;
  if (!std::isfinite(y)) {
    return;
  }
  CanvasRenderingContext2DState& state = GetState();
  if (state.ShadowOffset().y() == y) {
    return;
  }
  state.SetShadowOffsetY(ClampTo<float>(y));
}

double Canvas2DRecorderContext::shadowBlur() const {
  return GetState().ShadowBlur() / effective_zoom_;
}

void Canvas2DRecorderContext::setShadowBlur(double blur) {
  blur *= effective_zoom_;
  if (!std::isfinite(blur) || blur < 0) {
    return;
  }
  CanvasRenderingContext2DState& state = GetState();
  if (state.ShadowBlur() == blur) {
    return;
  }
  state.SetShadowBlur(ClampTo<float>(blur));
}

String Canvas2DRecorderContext::shadowColor() const {
  // TODO(crbug.com://40234521): CanvasRenderingContext2DState's shadow color
  // should be a Color, not an SkColor or SkColor4f.
  return GetState().ShadowColor().SerializeAsCanvasColor();
}

void Canvas2DRecorderContext::setShadowColor(const String& color_string) {
  Color color;
  if (ParseColorOrCurrentColor(color_string, color) ==
      ColorParseResult::kParseFailed) {
    return;
  }
  CanvasRenderingContext2DState& state = GetState();
  if (state.ShadowColor() == color) {
    return;
  }
  state.SetShadowColor(color);
}

const Vector<double>& Canvas2DRecorderContext::getLineDash() const {
  return GetState().LineDash();
}

static bool LineDashSequenceIsValid(const Vector<double>& dash) {
  return std::ranges::all_of(
      dash, [](double d) { return std::isfinite(d) && d >= 0; });
}

void Canvas2DRecorderContext::setLineDash(const Vector<double>& dash) {
  if (!LineDashSequenceIsValid(dash)) {
    return;
  }
  GetState().SetLineDash(dash);
}

double Canvas2DRecorderContext::lineDashOffset() const {
  return GetState().LineDashOffset();
}

void Canvas2DRecorderContext::setLineDashOffset(double offset) {
  CanvasRenderingContext2DState& state = GetState();
  if (!std::isfinite(offset) || state.LineDashOffset() == offset) {
    return;
  }
  state.SetLineDashOffset(ClampTo<float>(offset));
}

double Canvas2DRecorderContext::globalAlpha() const {
  return GetState().GlobalAlpha();
}

void Canvas2DRecorderContext::setGlobalAlpha(double alpha) {
  if (!(alpha >= 0 && alpha <= 1)) {
    return;
  }
  CanvasRenderingContext2DState& state = GetState();
  if (state.GlobalAlpha() == alpha) {
    return;
  }
  state.SetGlobalAlpha(alpha);
}

double Canvas2DRecorderContext::globalHDRHeadroom() const {
  return GetState().GlobalHDRHeadroom();
}

void Canvas2DRecorderContext::setGlobalHDRHeadroom(double h) {
  if (h < 0.f || std::isnan(h)) {
    return;
  }
  GetState().SetGlobalHDRHeadroom(h);
}

String Canvas2DRecorderContext::globalCompositeOperation() const {
  auto [composite_op, blend_mode] =
      CompositeAndBlendOpsFromSkBlendMode(GetState().GlobalComposite());
  return CanvasCompositeOperatorName(composite_op, blend_mode);
}

void Canvas2DRecorderContext::setGlobalCompositeOperation(
    const String& operation) {
  CompositeOperator op = kCompositeSourceOver;
  BlendMode blend_mode = BlendMode::kNormal;
  if (!ParseCanvasCompositeAndBlendMode(operation, op, blend_mode)) {
    return;
  }
  SkBlendMode sk_blend_mode = ToSkBlendMode(op, blend_mode);
  CanvasRenderingContext2DState& state = GetState();
  if (state.GlobalComposite() == sk_blend_mode) {
    return;
  }
  state.SetGlobalComposite(sk_blend_mode);
}

const V8UnionCanvasFilterOrString* Canvas2DRecorderContext::filter() const {
  const CanvasRenderingContext2DState& state = GetState();
  if (CanvasFilter* filter = state.GetCanvasFilter()) {
    return MakeGarbageCollected<V8UnionCanvasFilterOrString>(filter);
  }
  return MakeGarbageCollected<V8UnionCanvasFilterOrString>(
      state.UnparsedCSSFilter());
}

void Canvas2DRecorderContext::setFilter(
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
      break;
    case V8UnionCanvasFilterOrString::ContentType::kString: {
      const String& filter_string = input->GetAsString();
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

void Canvas2DRecorderContext::scale(double sx, double sy) {
  // TODO(crbug.com/40153853): Investigate the performance impact of simply
  // calling the 3d version of this function
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c) {
    return;
  }

  if (!std::isfinite(sx) || !std::isfinite(sy)) {
    return;
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

void Canvas2DRecorderContext::rotate(double angle_in_radians) {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c) {
    return;
  }

  if (!std::isfinite(angle_in_radians)) {
    return;
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

void Canvas2DRecorderContext::translate(double tx, double ty) {
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

void Canvas2DRecorderContext::transform(double m11,
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

  AffineTransform transform(fm11, fm12, fm21, fm22, fdx, fdy);
  const CanvasRenderingContext2DState& state = GetState();
  AffineTransform new_transform = state.GetTransform() * transform;
  if (state.GetTransform() == new_transform) {
    return;
  }

  SetTransform(new_transform);
  c->concat(transform.ToSkM44());

  if (IsTransformInvertible()) [[likely]] {
    GetModifiablePath().Transform(transform.Inverse());
  }
}

// On a platform where zoom_for_dsf is not enabled, the recording canvas has its
// logic to account for the device scale factor. Therefore, when the transform
// of the canvas happen, we must account for the effective_zoom_ such that the
// recording canvas would have the correct behavior.
//
// The setTransform always call resetTransform, so integrating the
// |effective_zoom_| in resetTransform instead of setTransform, to avoid
// integrating it twice if we have resetTransform and setTransform API calls.
void Canvas2DRecorderContext::resetTransform() {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c) {
    return;
  }

  CanvasRenderingContext2DState& state = GetState();
  AffineTransform ctm = state.GetTransform();
  bool invertible_ctm = IsTransformInvertible();
  // It is possible that CTM is identity while CTM is not invertible.
  // When CTM becomes non-invertible, realizeSaves() can make CTM identity.
  if (ctm.IsIdentity() && invertible_ctm) {
    if (effective_zoom_ != 1) {
      transform(effective_zoom_, 0, 0, effective_zoom_, 0, 0);
    }
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

  if (effective_zoom_ != 1) {
    transform(effective_zoom_, 0, 0, effective_zoom_, 0, 0);
  }
}

void Canvas2DRecorderContext::setTransform(double m11,
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

void Canvas2DRecorderContext::setTransform(DOMMatrixInit* transform,
                                           ExceptionState& exception_state) {
  DOMMatrixReadOnly* m =
      DOMMatrixReadOnly::fromMatrix(transform, exception_state);

  if (!m) {
    return;
  }

  setTransform(m->m11(), m->m12(), m->m21(), m->m22(), m->m41(), m->m42());
}

DOMMatrix* Canvas2DRecorderContext::getTransform() {
  const AffineTransform& t = GetState().GetTransform();
  DOMMatrix* m = DOMMatrix::Create();
  m->setA(t.A() / effective_zoom_);
  m->setB(t.B() / effective_zoom_);
  m->setC(t.C() / effective_zoom_);
  m->setD(t.D() / effective_zoom_);
  m->setE(t.E() / effective_zoom_);
  m->setF(t.F() / effective_zoom_);
  return m;
}

AffineTransform Canvas2DRecorderContext::GetTransform() const {
  return GetState().GetTransform();
}

void Canvas2DRecorderContext::beginPath() {
  Clear();
}

void Canvas2DRecorderContext::DrawPathInternal(
    const CanvasPath& path,
    CanvasRenderingContext2DState::PaintType paint_type,
    SkPathFillType fill_type,
    UsePaintCache use_paint_cache) {
  if (path.IsEmpty()) {
    return;
  }

  gfx::RectF bounds(path.BoundingRect());
  if (std::isnan(bounds.x()) || std::isnan(bounds.y()) ||
      std::isnan(bounds.width()) || std::isnan(bounds.height())) {
    return;
  }

  if (paint_type == CanvasRenderingContext2DState::kStrokePaintType) {
    InflateStrokeRect(bounds);
  }

  if (path.IsLine()) {
    if (paint_type == CanvasRenderingContext2DState::kFillPaintType)
        [[unlikely]] {
      // Filling a line is a no-op.
      // Also, SKCanvas::drawLine() ignores paint type and always strokes.
      return;
    }
    auto line = path.line();
    Draw<OverdrawOp::kNone>(
        /*draw_func=*/
        [line](MemoryManagedPaintCanvas* c, const cc::PaintFlags* flags) {
          c->drawLine(line.start.x(), line.start.y(), line.end.x(),
                      line.end.y(), *flags);
        },
        NoOverdraw, bounds, paint_type,
        GetState().HasPattern(paint_type)
            ? CanvasRenderingContext2DState::kNonOpaqueImage
            : CanvasRenderingContext2DState::kNoImage,
        CanvasPerformanceMonitor::DrawType::kPath);
    return;
  }

  HighEntropyCanvasOpType high_entropy_path_op_types =
      path.HighEntropyPathOpTypes();

  if (path.IsArc()) {
    const auto& arc = path.arc();
    const SkRect oval =
        SkRect::MakeLTRB(arc.x - arc.radius, arc.y - arc.radius,
                         arc.x + arc.radius, arc.y + arc.radius);
    const float start_degrees =
        ClampNonFiniteToZero(arc.start_angle_radians * 180 / kPiFloat);
    const float sweep_degrees =
        ClampNonFiniteToZero(arc.sweep_angle_radians * 180 / kPiFloat);
    const bool closed = arc.closed;
    Draw<OverdrawOp::kNone>(
        /*draw_func=*/
        [oval, start_degrees, sweep_degrees, closed,
         high_entropy_path_op_types](MemoryManagedPaintCanvas* c,
                                     const cc::PaintFlags* flags) {
          cc::PaintFlags arc_paint_flags(*flags);
          arc_paint_flags.setArcClosed(closed);
          c->drawArc(oval, start_degrees, sweep_degrees, arc_paint_flags);
          c->AddHighEntropyCanvasOpTypes(high_entropy_path_op_types);
        },
        NoOverdraw, bounds, paint_type,
        GetState().HasPattern(paint_type)
            ? CanvasRenderingContext2DState::kNonOpaqueImage
            : CanvasRenderingContext2DState::kNoImage,
        CanvasPerformanceMonitor::DrawType::kPath);
    return;
  }

  SkPath sk_path = path.GetPath().GetSkPath();
  sk_path.setFillType(fill_type);

  Draw<OverdrawOp::kNone>(
      /*draw_func=*/
      [sk_path, use_paint_cache, high_entropy_path_op_types](
          MemoryManagedPaintCanvas* c, const cc::PaintFlags* flags) {
        c->drawPath(sk_path, *flags, use_paint_cache);
        c->AddHighEntropyCanvasOpTypes(high_entropy_path_op_types);
      },
      NoOverdraw, bounds, paint_type,
      GetState().HasPattern(paint_type)
          ? CanvasRenderingContext2DState::kNonOpaqueImage
          : CanvasRenderingContext2DState::kNoImage,
      CanvasPerformanceMonitor::DrawType::kPath);
}

static SkPathFillType CanvasFillRuleToSkiaFillType(
    const V8CanvasFillRule& winding_rule) {
  switch (winding_rule.AsEnum()) {
    case V8CanvasFillRule::Enum::kNonzero:
      return SkPathFillType::kWinding;
    case V8CanvasFillRule::Enum::kEvenodd:
      return SkPathFillType::kEvenOdd;
  }
  NOTREACHED();
}

void Canvas2DRecorderContext::fill() {
  FillImpl(SkPathFillType::kWinding);
}

void Canvas2DRecorderContext::fill(const V8CanvasFillRule& winding) {
  FillImpl(CanvasFillRuleToSkiaFillType(winding));
}

void Canvas2DRecorderContext::FillImpl(SkPathFillType winding_rule) {
  DrawPathInternal(*this, CanvasRenderingContext2DState::kFillPaintType,
                   winding_rule, UsePaintCache::kDisabled);
}

void Canvas2DRecorderContext::fill(Path2D* dom_path) {
  FillPathImpl(dom_path, SkPathFillType::kWinding);
}

void Canvas2DRecorderContext::fill(Path2D* dom_path,
                                   const V8CanvasFillRule& winding) {
  FillPathImpl(dom_path, CanvasFillRuleToSkiaFillType(winding));
}

void Canvas2DRecorderContext::FillPathImpl(Path2D* dom_path,
                                           SkPathFillType winding_rule) {
  DrawPathInternal(*dom_path, CanvasRenderingContext2DState::kFillPaintType,
                   winding_rule, path2d_use_paint_cache_);
}

void Canvas2DRecorderContext::stroke() {
  DrawPathInternal(*this, CanvasRenderingContext2DState::kStrokePaintType,
                   SkPathFillType::kWinding, UsePaintCache::kDisabled);
}

void Canvas2DRecorderContext::stroke(Path2D* dom_path) {
  DrawPathInternal(*dom_path, CanvasRenderingContext2DState::kStrokePaintType,
                   SkPathFillType::kWinding, path2d_use_paint_cache_);
}

void Canvas2DRecorderContext::fillRect(double x,
                                       double y,
                                       double width,
                                       double height) {
  if (!ValidateRectForCanvas(x, y, width, height)) {
    return;
  }

  if (!GetOrCreatePaintCanvas()) {
    return;
  }

  // We are assuming that if the pattern is not accelerated and the current
  // canvas is accelerated, the texture of the pattern will not be able to be
  // moved to the texture of the canvas receiving the pattern (because if the
  // pattern was unaccelerated is because it was not possible to hold that image
  // in an accelerated texture - that is, into the GPU). That's why we disable
  // the acceleration to be sure that it will work.
  const CanvasRenderingContext2DState& state = GetState();
  const bool has_pattern =
      state.HasPattern(CanvasRenderingContext2DState::kFillPaintType);
  if (IsAccelerated() && has_pattern &&
      !state.PatternIsAccelerated(
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
      /*draw_func=*/
      [rect](MemoryManagedPaintCanvas* c, const cc::PaintFlags* flags) {
        c->drawRect(gfx::RectFToSkRect(rect), *flags);
      },
      NoOverdraw, /*bounds=*/rect,
      CanvasRenderingContext2DState::kFillPaintType,
      has_pattern ? CanvasRenderingContext2DState::kNonOpaqueImage
                  : CanvasRenderingContext2DState::kNoImage,
      CanvasPerformanceMonitor::DrawType::kRectangle);
}

static void StrokeRectOnCanvas(const gfx::RectF& rect,
                               cc::PaintCanvas* canvas,
                               const cc::PaintFlags* flags) {
  DCHECK_EQ(flags->getStyle(), cc::PaintFlags::kStroke_Style);
  if ((rect.width() > 0) != (rect.height() > 0)) {
    // When stroking, we must skip the zero-dimension segments
    const SkPath path = SkPathBuilder()
                            .moveTo(rect.x(), rect.y())
                            .lineTo(rect.right(), rect.bottom())
                            .close()
                            .detach();
    canvas->drawPath(path, *flags);
    return;
  }
  canvas->drawRect(gfx::RectFToSkRect(rect), *flags);
}

void Canvas2DRecorderContext::strokeRect(double x,
                                         double y,
                                         double width,
                                         double height) {
  if (!ValidateRectForCanvas(x, y, width, height)) {
    return;
  }

  if (!GetOrCreatePaintCanvas()) {
    return;
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
                             bounds.height())) {
    return;
  }

  Draw<OverdrawOp::kNone>(
      /*draw_func=*/
      [rect](MemoryManagedPaintCanvas* c, const cc::PaintFlags* flags) {
        StrokeRectOnCanvas(rect, c, flags);
      },
      NoOverdraw, bounds, CanvasRenderingContext2DState::kStrokePaintType,
      GetState().HasPattern(CanvasRenderingContext2DState::kStrokePaintType)
          ? CanvasRenderingContext2DState::kNonOpaqueImage
          : CanvasRenderingContext2DState::kNoImage,
      CanvasPerformanceMonitor::DrawType::kRectangle);
}

void Canvas2DRecorderContext::ClipInternal(const Path& path,
                                           const V8CanvasFillRule& winding_rule,
                                           UsePaintCache use_paint_cache) {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c) {
    return;
  }
  if (!IsTransformInvertible()) [[unlikely]] {
    return;
  }

  SkPath sk_path = path.GetSkPath();
  sk_path.setFillType(CanvasFillRuleToSkiaFillType(winding_rule));
  GetState().ClipPath(sk_path, clip_antialiasing_);
  c->clipPath(sk_path, SkClipOp::kIntersect, clip_antialiasing_ == kAntiAliased,
              use_paint_cache);
}

void Canvas2DRecorderContext::clip(const V8CanvasFillRule& winding_rule) {
  ClipInternal(GetPath(), winding_rule, UsePaintCache::kDisabled);
}

void Canvas2DRecorderContext::clip(Path2D* dom_path,
                                   const V8CanvasFillRule& winding_rule) {
  ClipInternal(dom_path->GetPath(), winding_rule, path2d_use_paint_cache_);
}

bool Canvas2DRecorderContext::isPointInPath(
    const double x,
    const double y,
    const V8CanvasFillRule& winding_rule) {
  return IsPointInPathInternal(GetPath(), x, y, winding_rule);
}

bool Canvas2DRecorderContext::isPointInPath(
    Path2D* dom_path,
    const double x,
    const double y,
    const V8CanvasFillRule& winding_rule) {
  return IsPointInPathInternal(dom_path->GetPath(), x, y, winding_rule);
}

bool Canvas2DRecorderContext::IsPointInPathInternal(
    const Path& path,
    const double x,
    const double y,
    const V8CanvasFillRule& winding_rule) {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c) {
    return false;
  }
  if (!IsTransformInvertible()) [[unlikely]] {
    return false;
  }

  if (!std::isfinite(x) || !std::isfinite(y)) {
    return false;
  }
  gfx::PointF point(ClampTo<float>(x), ClampTo<float>(y));
  AffineTransform ctm = GetState().GetTransform();
  gfx::PointF transformed_point = ctm.Inverse().MapPoint(point);

  return path.Contains(
      transformed_point,
      SkFillTypeToWindRule(CanvasFillRuleToSkiaFillType(winding_rule)));
}

bool Canvas2DRecorderContext::isPointInStroke(const double x, const double y) {
  return IsPointInStrokeInternal(GetPath(), x, y);
}

bool Canvas2DRecorderContext::isPointInStroke(Path2D* dom_path,
                                              const double x,
                                              const double y) {
  return IsPointInStrokeInternal(dom_path->GetPath(), x, y);
}

bool Canvas2DRecorderContext::IsPointInStrokeInternal(const Path& path,
                                                      const double x,
                                                      const double y) {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c) {
    return false;
  }
  if (!IsTransformInvertible()) [[unlikely]] {
    return false;
  }

  if (!std::isfinite(x) || !std::isfinite(y)) {
    return false;
  }
  gfx::PointF point(ClampTo<float>(x), ClampTo<float>(y));
  const CanvasRenderingContext2DState& state = GetState();
  const AffineTransform& ctm = state.GetTransform();
  gfx::PointF transformed_point = ctm.Inverse().MapPoint(point);

  StrokeData stroke_data;
  stroke_data.SetThickness(state.LineWidth());
  stroke_data.SetLineCap(state.GetLineCap());
  stroke_data.SetLineJoin(state.GetLineJoin());
  stroke_data.SetMiterLimit(state.MiterLimit());
  Vector<float> line_dash(state.LineDash().size());
  std::ranges::copy(state.LineDash(), line_dash.begin());
  stroke_data.SetLineDash(line_dash, state.LineDashOffset());
  return path.StrokeContains(transformed_point, stroke_data, ctm);
}

cc::PaintFlags Canvas2DRecorderContext::GetClearFlags() const {
  cc::PaintFlags clear_flags;
  clear_flags.setStyle(cc::PaintFlags::kFill_Style);
  if (HasAlpha()) {
    clear_flags.setBlendMode(SkBlendMode::kClear);
  } else {
    clear_flags.setColor(SK_ColorBLACK);
  }
  return clear_flags;
}

void Canvas2DRecorderContext::clearRect(double x,
                                        double y,
                                        double width,
                                        double height) {
  if (!ValidateRectForCanvas(x, y, width, height)) {
    return;
  }

  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c) {
    return;
  }
  if (!IsTransformInvertible()) [[unlikely]] {
    return;
  }

  SkIRect clip_bounds;
  if (!c->getDeviceClipBounds(&clip_bounds)) {
    return;
  }

  cc::PaintFlags clear_flags = GetClearFlags();

  // clamp to float to avoid float cast overflow when used as SkScalar
  AdjustRectForCanvas(x, y, width, height);
  float fx = ClampTo<float>(x);
  float fy = ClampTo<float>(y);
  float fwidth = ClampTo<float>(width);
  float fheight = ClampTo<float>(height);

  gfx::RectF rect(fx, fy, fwidth, fheight);
  if (RectContainsTransformedRect(rect, clip_bounds)) {
    CheckOverdraw(&clear_flags, CanvasRenderingContext2DState::kNoImage,
                  OverdrawOp::kClearRect);
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
  if (image_rect.Contains(*src_rect)) {
    return;
  }

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

void Canvas2DRecorderContext::drawImage(const V8CanvasImageSource* image_source,
                                        double x,
                                        double y,
                                        ExceptionState& exception_state) {
  CanvasImageSource* image_source_internal =
      ToCanvasImageSource(image_source, exception_state);
  if (!image_source_internal) {
    return;
  }
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

void Canvas2DRecorderContext::drawImage(const V8CanvasImageSource* image_source,
                                        double x,
                                        double y,
                                        double width,
                                        double height,
                                        ExceptionState& exception_state) {
  CanvasImageSource* image_source_internal =
      ToCanvasImageSource(image_source, exception_state);
  if (!image_source_internal) {
    return;
  }
  gfx::SizeF default_object_size(Width(), Height());
  gfx::SizeF source_rect_size = image_source_internal->ElementSize(
      default_object_size,
      RespectImageOrientationInternal(image_source_internal));
  drawImage(image_source_internal, 0, 0, source_rect_size.width(),
            source_rect_size.height(), x, y, width, height, exception_state);
}

void Canvas2DRecorderContext::drawImage(const V8CanvasImageSource* image_source,
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
  if (!image_source_internal) {
    return;
  }
  drawImage(image_source_internal, sx, sy, sw, sh, dx, dy, dw, dh,
            exception_state);
}

bool Canvas2DRecorderContext::ShouldDrawImageAntialiased(
    const gfx::RectF& dest_rect) const {
  if (!GetState().ShouldAntialias()) {
    return false;
  }
  const cc::PaintCanvas* c = GetPaintCanvas();
  DCHECK(c);

  const SkMatrix& ctm = c->getLocalToDevice().asM33();
  // Don't disable anti-aliasing if we're rotated or skewed.
  if (!ctm.rectStaysRect()) {
    return true;
  }
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

void Canvas2DRecorderContext::DrawImageInternal(
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

  // `image` is always present unless `image_source` is a video element or a
  // VideoFrame; in which case a fast path may exist for drawing directly from
  // the video into the canvas. The fast path is not always faster though (e.g.,
  // when scaling), so sometimes the `image` path may still be used by video.
  if (image) {
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
  } else if (image_source->IsVideoElement()) {
    c->save();
    c->clipRect(gfx::RectFToSkRect(dst_rect));
    c->translate(dst_rect.x(), dst_rect.y());
    c->scale(dst_rect.width() / src_rect.width(),
             dst_rect.height() / src_rect.height());
    c->translate(-src_rect.x(), -src_rect.y());
    HTMLVideoElement* video = static_cast<HTMLVideoElement*>(image_source);
    video->PaintCurrentFrame(
        c, gfx::Rect(video->videoWidth(), video->videoHeight()), image_flags);
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
      if (ImageOrientation(orientation_enum).UsesWidthAsHeight()) {
        corrected_src_rect = gfx::TransposeRect(src_rect);
      }
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
    NOTREACHED();
  }

  c->restoreToCount(initial_save_count);
}

void Canvas2DRecorderContext::SetOriginTaintedByContent() {
  SetOriginTainted();
  origin_tainted_by_content_ = true;
  for (auto& state : state_stack_) {
    state->ClearResolvedFilter();
  }
}

void Canvas2DRecorderContext::drawImage(CanvasImageSource* image_source,
                                        double sx,
                                        double sy,
                                        double sw,
                                        double sh,
                                        double dx,
                                        double dy,
                                        double dw,
                                        double dh,
                                        ExceptionState& exception_state) {
  if (!GetOrCreatePaintCanvas()) {
    return;
  }

  if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dw) ||
      !std::isfinite(dh) || !std::isfinite(sx) || !std::isfinite(sy) ||
      !std::isfinite(sw) || !std::isfinite(sh)) {
    return;
  }

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

  scoped_refptr<Image> image;
  gfx::SizeF default_object_size(Width(), Height());
  SourceImageStatus source_image_status = kInvalidSourceImageStatus;
  if (image_source->IsVideoElement()) {
    if (!static_cast<HTMLVideoElement*>(image_source)
             ->HasAvailableVideoFrame()) {
      return;
    }
  } else if (image_source->IsVideoFrame()) {
    auto frame = static_cast<VideoFrame*>(image_source)->frame();
    if (!frame) {
      return;
    }

    // When resizing CPU backed frames, prefer to first create an accelerated
    // image if possible since it's much faster to scale on the GPU.
    if (src_rect.size() != dst_rect.size() && image_source->IsAccelerated() &&
        !frame->HasSharedImage()) {
      image = image_source->GetSourceImageForCanvas(&source_image_status,
                                                    default_object_size);

      // No need to check `image` here since if it's nullptr, we'll just fall
      // back to drawing directly from the VideoFrame below.
    }
  } else {
    image = image_source->GetSourceImageForCanvas(&source_image_status,
                                                  default_object_size);
    if (source_image_status == kUndecodableSourceImageStatus) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "The HTMLImageElement provided is in the 'broken' state.");
    }
    if (source_image_status == kLayersOpenInCanvasSource) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "`drawImage()` with a canvas as a source cannot be called while "
          "layers are open in the the source canvas.");
      return;
    }
    if (!image || !image->width() || !image->height()) {
      return;
    }
  }

  // The dest rect is filled in as zero for invalid images if unspecified, but
  // the spec expects the code to throw during GetSourceImageForCanvas() above,
  // so this must be checked here and not above when constructing `dst_rect`.
  if (!dw || !dh || !sw || !sh) {
    return;
  }

  gfx::SizeF image_size = image_source->ElementSize(
      default_object_size, RespectImageOrientationInternal(image_source));

  ClipRectsToImageRect(gfx::RectF(image_size), &src_rect, &dst_rect);

  if (src_rect.IsEmpty()) {
    return;
  }

  ValidateStateStack();

  WillDrawImage(image_source, image && image->IsTextureBacked());

  if (!origin_tainted_by_content_ && WouldTaintCanvasOrigin(image_source)) {
    SetOriginTaintedByContent();
  }

  Draw<OverdrawOp::kDrawImage>(
      /*draw_func=*/
      [this, image_source, image, src_rect, dst_rect](
          MemoryManagedPaintCanvas* c, const cc::PaintFlags* flags) {
        SkSamplingOptions sampling =
            cc::PaintFlags::FilterQualityToSkSamplingOptions(
                flags ? flags->getFilterQuality()
                      : cc::PaintFlags::FilterQuality::kNone);
        DrawImageInternal(c, image_source, image.get(), src_rect, dst_rect,
                          sampling, flags);
      },
      /*draw_covers_clip_bounds=*/
      [this, dst_rect](const SkIRect& clip_bounds) {
        return RectContainsTransformedRect(dst_rect, clip_bounds);
      },
      /*bounds=*/dst_rect, CanvasRenderingContext2DState::kImagePaintType,
      image_source->IsOpaque() ? CanvasRenderingContext2DState::kOpaqueImage
                               : CanvasRenderingContext2DState::kNonOpaqueImage,
      CanvasPerformanceMonitor::DrawType::kImage);
}

bool Canvas2DRecorderContext::RectContainsTransformedRect(
    const gfx::RectF& rect,
    const SkIRect& transformed_rect) const {
  gfx::QuadF quad(rect);
  gfx::QuadF transformed_quad(
      gfx::RectF(transformed_rect.x(), transformed_rect.y(),
                 transformed_rect.width(), transformed_rect.height()));
  return GetState().GetTransform().MapQuad(quad).ContainsQuad(transformed_quad);
}

CanvasGradient* Canvas2DRecorderContext::createLinearGradient(double x0,
                                                              double y0,
                                                              double x1,
                                                              double y1) {
  if (!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(x1) ||
      !std::isfinite(y1)) {
    return nullptr;
  }

  // clamp to float to avoid float cast overflow
  float fx0 = ClampTo<float>(x0);
  float fy0 = ClampTo<float>(y0);
  float fx1 = ClampTo<float>(x1);
  float fy1 = ClampTo<float>(y1);

  return MakeGarbageCollected<CanvasGradient>(gfx::PointF(fx0, fy0),
                                              gfx::PointF(fx1, fy1));
}

CanvasGradient* Canvas2DRecorderContext::createRadialGradient(
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
      !std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(r1)) {
    return nullptr;
  }

  // clamp to float to avoid float cast overflow
  float fx0 = ClampTo<float>(x0);
  float fy0 = ClampTo<float>(y0);
  float fr0 = ClampTo<float>(r0);
  float fx1 = ClampTo<float>(x1);
  float fy1 = ClampTo<float>(y1);
  float fr1 = ClampTo<float>(r1);

  return MakeGarbageCollected<CanvasGradient>(gfx::PointF(fx0, fy0), fr0,
                                              gfx::PointF(fx1, fy1), fr1);
}

CanvasGradient* Canvas2DRecorderContext::createConicGradient(double startAngle,
                                                             double centerX,
                                                             double centerY) {
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DConicGradient);
  if (!std::isfinite(startAngle) || !std::isfinite(centerX) ||
      !std::isfinite(centerY)) {
    return nullptr;
  }

  // clamp to float to avoid float cast overflow
  float a = ClampTo<float>(startAngle);
  float x = ClampTo<float>(centerX);
  float y = ClampTo<float>(centerY);

  // convert |startAngle| from radians to degree and rotate 90 degree, so
  // |startAngle| at 0 starts from x-axis.
  a = Rad2deg(a) + 90;

  return MakeGarbageCollected<CanvasGradient>(a, gfx::PointF(x, y));
}

CanvasPattern* Canvas2DRecorderContext::createPattern(

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

CanvasPattern* Canvas2DRecorderContext::createPattern(
    CanvasImageSource* image_source,
    const String& repetition_type,
    ExceptionState& exception_state) {
  if (!image_source) {
    return nullptr;
  }

  Pattern::RepeatMode repeat_mode =
      CanvasPattern::ParseRepetitionType(repetition_type, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  SourceImageStatus status;

  gfx::SizeF default_object_size(Width(), Height());
  scoped_refptr<Image> image_for_rendering =
      image_source->GetSourceImageForCanvas(&status, default_object_size);

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
    case kLayersOpenInCanvasSource:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "`createPattern()` with a canvas as a source cannot be called while "
          "layers are open in the source canvas.");
      return nullptr;
    default:
      NOTREACHED();
  }

  if (!image_for_rendering) {
    return nullptr;
  }

  bool origin_clean = !WouldTaintCanvasOrigin(image_source);

  HighEntropyCanvasOpType source_high_entropy_canvas_op_types =
      HighEntropyCanvasOpType::kNone;
  if ((image_source->IsCanvasElement() || image_source->IsOffscreenCanvas()) &&
      image_for_rendering->IsStaticBitmapImage()) {
    source_high_entropy_canvas_op_types =
        static_cast<StaticBitmapImage*>(image_for_rendering.get())
            ->HighEntropyCanvasOpTypes();
  }

  auto* pattern = MakeGarbageCollected<CanvasPattern>(
      std::move(image_for_rendering), repeat_mode, origin_clean,
      source_high_entropy_canvas_op_types);
  return pattern;
}

namespace {

scoped_refptr<cc::RefCountedBuffer<SkPoint>> MakeSkPointBuffer(
    NotShared<DOMFloat32Array> array,
    ExceptionState& exception_state,
    const char* msg) {
  if ((array->length() == 0) || (array->length() % 2)) {
    exception_state.ThrowRangeError(msg);
    return nullptr;
  }

  static_assert(std::is_trivially_copyable<SkPoint>::value);
  static_assert(sizeof(SkPoint) == sizeof(float) * 2);

  std::vector<SkPoint> skpoints(array->length() / 2);
  base::as_writable_byte_span(base::allow_nonunique_obj, skpoints)
      .copy_from(array->ByteSpan());

  return base::MakeRefCounted<cc::RefCountedBuffer<SkPoint>>(
      std::move(skpoints));
}

}  // namespace

Mesh2DVertexBuffer* Canvas2DRecorderContext::createMesh2DVertexBuffer(
    NotShared<DOMFloat32Array> array,
    ExceptionState& exception_state) {
  scoped_refptr<cc::RefCountedBuffer<SkPoint>> buffer = MakeSkPointBuffer(
      array, exception_state,
      "The vertex buffer must contain a non-zero, even number of floats.");

  return buffer ? MakeGarbageCollected<Mesh2DVertexBuffer>(std::move(buffer))
                : nullptr;
}

Mesh2DUVBuffer* Canvas2DRecorderContext::createMesh2DUVBuffer(
    NotShared<DOMFloat32Array> array,
    ExceptionState& exception_state) {
  scoped_refptr<cc::RefCountedBuffer<SkPoint>> buffer = MakeSkPointBuffer(
      array, exception_state,
      "The UV buffer must contain a non-zero, even number of floats.");

  return buffer ? MakeGarbageCollected<Mesh2DUVBuffer>(std::move(buffer))
                : nullptr;
}

Mesh2DIndexBuffer* Canvas2DRecorderContext::createMesh2DIndexBuffer(
    NotShared<DOMUint16Array> array,
    ExceptionState& exception_state) {
  if ((array->length() == 0) || (array->length() % 3)) {
    exception_state.ThrowRangeError(
        "The index buffer must contain a non-zero, multiple of three number of "
        "uints.");
    return nullptr;
  }
  auto data = array->AsSpan();
  return MakeGarbageCollected<Mesh2DIndexBuffer>(
      base::MakeRefCounted<cc::RefCountedBuffer<uint16_t>>(
          std::vector<uint16_t>(data.begin(), data.end())));
}

void Canvas2DRecorderContext::drawMesh(
    const Mesh2DVertexBuffer* vertex_buffer,
    const Mesh2DUVBuffer* uv_buffer,
    const Mesh2DIndexBuffer* index_buffer,
    const V8CanvasImageSource* v8_image_source,
    ExceptionState& exception_state) {
  CanvasImageSource* image_source =
      ToCanvasImageSource(v8_image_source, exception_state);
  if (!image_source) {
    return;
  }

  SourceImageStatus source_image_status = kInvalidSourceImageStatus;
  scoped_refptr<Image> image = image_source->GetSourceImageForCanvas(
      &source_image_status, gfx::SizeF(Width(), Height()));
  switch (source_image_status) {
    case kUndecodableSourceImageStatus:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "The HTMLImageElement provided is in the 'broken' state.");
      return;
    case kLayersOpenInCanvasSource:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "`drawMesh()` with a canvas as a source cannot be called while "
          "layers are open in the the source canvas.");
      return;
    default:
      break;
  }

  if (!image || image->IsNull()) {
    return;
  }

  scoped_refptr<cc::RefCountedBuffer<SkPoint>> vertex_data =
      vertex_buffer->GetBuffer();
  CHECK_NE(vertex_data, nullptr);
  scoped_refptr<cc::RefCountedBuffer<SkPoint>> uv_data = uv_buffer->GetBuffer();
  CHECK_NE(uv_data, nullptr);
  scoped_refptr<cc::RefCountedBuffer<uint16_t>> index_data =
      index_buffer->GetBuffer();
  CHECK_NE(index_data, nullptr);

  WillDrawImage(image_source, image && image->IsTextureBacked());

  if (!origin_tainted_by_content_ && WouldTaintCanvasOrigin(image_source)) {
    SetOriginTaintedByContent();
  }

  SkRect bounds = SkRect::BoundsOrEmpty(vertex_data->data());

  Draw<OverdrawOp::kNone>(
      /*draw_func=*/
      [&image, &vertex_data, &uv_data, &index_data](
          MemoryManagedPaintCanvas* c, const cc::PaintFlags* flags) {
        const gfx::RectF src(image->width(), image->height());
        // UV coordinates are normalized, relative to the texture size.
        const SkMatrix local_matrix =
            SkMatrix::Scale(1.0f / image->width(), 1.0f / image->height());

        cc::PaintFlags scoped_flags(*flags);
        image->ApplyShader(scoped_flags, local_matrix, src, ImageDrawOptions());
        c->drawVertices(vertex_data, uv_data, index_data, scoped_flags);
      },
      NoOverdraw,
      gfx::RectF(bounds.x(), bounds.y(), bounds.width(), bounds.height()),
      CanvasRenderingContext2DState::PaintType::kFillPaintType,
      image_source->IsOpaque() ? CanvasRenderingContext2DState::kOpaqueImage
                               : CanvasRenderingContext2DState::kNonOpaqueImage,
      CanvasPerformanceMonitor::DrawType::kOther);
}

bool Canvas2DRecorderContext::ComputeDirtyRect(const gfx::RectF& local_rect,
                                               SkIRect* dirty_rect) {
  SkIRect clip_bounds;
  cc::PaintCanvas* paint_canvas = GetOrCreatePaintCanvas();
  if (!paint_canvas || !paint_canvas->getDeviceClipBounds(&clip_bounds)) {
    return false;
  }
  return ComputeDirtyRect(local_rect, clip_bounds, dirty_rect);
}

void Canvas2DRecorderContext::InflateStrokeRect(gfx::RectF& rect) const {
  // Fast approximation of the stroke's bounding rect.
  // This yields a slightly oversized rect but is very fast
  // compared to Path::strokeBoundingRect().
  static const double kRoot2 = sqrtf(2);
  const CanvasRenderingContext2DState& state = GetState();
  double delta = state.LineWidth() / 2;
  if (state.GetLineJoin() == kMiterJoin) {
    delta *= state.MiterLimit();
  } else if (state.GetLineCap() == kSquareCap) {
    delta *= kRoot2;
  }

  rect.Outset(ClampTo<float>(delta));
}

bool Canvas2DRecorderContext::imageSmoothingEnabled() const {
  return GetState().ImageSmoothingEnabled();
}

void Canvas2DRecorderContext::setImageSmoothingEnabled(bool enabled) {
  CanvasRenderingContext2DState& state = GetState();
  if (enabled == state.ImageSmoothingEnabled()) {
    return;
  }

  state.SetImageSmoothingEnabled(enabled);
}

V8ImageSmoothingQuality Canvas2DRecorderContext::imageSmoothingQuality() const {
  return GetState().ImageSmoothingQuality();
}

void Canvas2DRecorderContext::setImageSmoothingQuality(
    const V8ImageSmoothingQuality& quality) {
  CanvasRenderingContext2DState& state = GetState();
  if (quality == state.ImageSmoothingQuality()) {
    return;
  }

  state.SetImageSmoothingQuality(quality);
}

void Canvas2DRecorderContext::Trace(Visitor* visitor) const {
  visitor->Trace(state_stack_);
  visitor->Trace(color_cache_);
  CanvasPath::Trace(visitor);
}

Canvas2DRecorderContext::UsageCounters::UsageCounters()
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

void CanvasOverdrawHistogram(Canvas2DRecorderContext::OverdrawOp op) {
  UMA_HISTOGRAM_ENUMERATION("Blink.Canvas.OverdrawOp", op);
}

}  // unnamed namespace

void Canvas2DRecorderContext::WillOverwriteCanvas(
    Canvas2DRecorderContext::OverdrawOp op) {
  auto* host = GetCanvasRenderingContextHost();
  if (host) {  // CSS paint use cases not counted.
    UseCounter::Count(GetTopExecutionContext(),
                      WebFeature::kCanvasRenderingContext2DHasOverdraw);
    CanvasOverdrawHistogram(op);
    CanvasOverdrawHistogram(OverdrawOp::kTotal);
  }

  // We only hit the kHasTransform bucket if the op is affected by transforms.
  if (op == OverdrawOp::kClearRect || op == OverdrawOp::kDrawImage) {
    const CanvasRenderingContext2DState& state = GetState();
    bool has_clip = state.HasClip();
    bool has_transform = !state.GetTransform().IsIdentity();
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

  if (MemoryManagedPaintRecorder* recorder = Recorder(); recorder != nullptr) {
    recorder->RestartCurrentLayer();
  }
}

HTMLCanvasElement* Canvas2DRecorderContext::HostAsHTMLCanvasElement() const {
  return nullptr;
}

OffscreenCanvas* Canvas2DRecorderContext::HostAsOffscreenCanvas() const {
  return nullptr;
}

const Font* Canvas2DRecorderContext::AccessFont(HTMLCanvasElement* canvas) {
  const CanvasRenderingContext2DState& state = GetState();
  if (!state.HasRealizedFont()) {
    setFont(state.UnparsedFont());
  }
  if (canvas) {
    canvas->GetDocument().GetCanvasFontCache()->WillUseCurrentFont();
  }
  return state.GetFont();
}

void Canvas2DRecorderContext::SnapshotStateForFilter() {
  auto* canvas = HostAsHTMLCanvasElement();
  // The style resolution required for fonts is not available in frame-less
  // documents.
  if (canvas && !canvas->GetDocument().GetFrame()) {
    return;
  }

  GetState().SetFontForFilter(AccessFont(canvas));
}

bool Canvas2DRecorderContext::IsAccelerated() const {
  CanvasRenderingContextHost* host = GetCanvasRenderingContextHost();
  if (host) {
    return host->GetRasterModeForCanvas2D() == RasterMode::kGPU;
  }
  return false;
}

}  // namespace blink
