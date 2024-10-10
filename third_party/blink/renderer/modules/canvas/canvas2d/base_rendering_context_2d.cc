// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
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
#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/record_paint_canvas.h"
#include "cc/paint/refcounted_buffer.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_transformation.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/color_scheme.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_object_objectarray_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_begin_layer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_2d_gpu_transfer_option.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_font_stretch.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_text_rendering.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_canvasfilter_string.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/text_link_colors.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix_read_only.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_performance_monitor.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
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
#include "third_party/blink/renderer/modules/canvas/canvas2d/identifiability_study_helper.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/mesh_2d_index_buffer.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/mesh_2d_uv_buffer.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/mesh_2d_vertex_buffer.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/path_2d.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/v8_canvas_style.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_enum_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_cpp.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/image_data_buffer.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/interpolation_space.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_canvas.h"  // IWYU pragma: keep (https://github.com/clangd/clangd/issues/2044)
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_filter.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/graphics/pattern.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/text/unicode_bidi.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "third_party/perfetto/include/perfetto/tracing/event_context.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_args.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkClipOp.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathTypes.h"
#include "third_party/skia/include/core/SkPixmap.h"
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
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "v8/include/v8-local-handle.h"

// Including "base/time/time.h" triggers a bug in IWYU.
// https://github.com/include-what-you-use/include-what-you-use/issues/1122
// IWYU pragma: no_include "base/numerics/clamped_math.h"

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
class FontSelector;
class ImageDataSettings;
class ScriptState;
class SimpleFontData;

using ::cc::UsePaintCache;

BASE_FEATURE(kDisableCanvasOverdrawOptimization,
             "DisableCanvasOverdrawOptimization",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Maximum number of colors in the color cache
// (`BaseRenderingContext2D::color_cache_`).
constexpr size_t kColorCacheMaxSize = 8;

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
      path2d_use_paint_cache_(
          base::FeatureList::IsEnabled(features::kPath2DPaintCache)
              ? UsePaintCache::kEnabled
              : UsePaintCache::kDisabled) {
  state_stack_.push_back(MakeGarbageCollected<CanvasRenderingContext2DState>());
}

BaseRenderingContext2D::~BaseRenderingContext2D() {
  UMA_HISTOGRAM_CUSTOM_COUNTS("Blink.Canvas.MaximumStateStackDepth",
                              max_state_stack_depth_, 1, 33, 32);
}

void BaseRenderingContext2D::save() {
  if (isContextLost()) [[unlikely]] {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSave);
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

  if (canvas)
    canvas->save();

  ValidateStateStack();
}

void BaseRenderingContext2D::restore(ExceptionState& exception_state) {
  if (isContextLost()) [[unlikely]] {
    return;
  }

  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kRestore);
  }
  ValidateStateStack();
  if (state_stack_.size() <= 1)
    // State stack is empty. Extra `restore()` are silently ignored.
    return;

  // Verify that the top of the stack was pushed with Save.
  if (GetState().GetSaveType() !=
      CanvasRenderingContext2DState::SaveType::kSaveRestore) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Called `restore()` with no matching `save()` inside layer.");
    return;
  }

  cc::PaintCanvas* canvas = GetOrCreatePaintCanvas();
  if (!canvas) {
    return;
  }

  PopAndRestore(*canvas);
  ValidateStateStack();
}

void BaseRenderingContext2D::beginLayerImpl(ScriptState* script_state,
                                            const BeginLayerOptions* options,
                                            ExceptionState* exception_state) {
  if (isContextLost()) [[unlikely]] {
    return;
  }
  // TODO(crbug.com/1234113): Instrument new canvas APIs.
  identifiability_study_helper_.set_encountered_skipped_ops();

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

void BaseRenderingContext2D::AddLayerFilterUserCount(
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
                 cc::PaintCanvas& canvas) : canvas_(canvas) {
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
BaseRenderingContext2D::SaveLayerForState(
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
          std::array{
              std::move(shadow_filter),   // Shadow.
              std::move(context_filter),  // Foreground.
          },
          flags);
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

void BaseRenderingContext2D::endLayer(ExceptionState& exception_state) {
  if (isContextLost()) [[unlikely]] {
    return;
  }
  // TODO(crbug.com/1234113): Instrument new canvas APIs.
  identifiability_study_helper_.set_encountered_skipped_ops();

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
  PopAndRestore(layer_canvas);

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

void BaseRenderingContext2D::PopAndRestore(cc::PaintCanvas& canvas) {
  if (IsTransformInvertible() && !GetState().GetTransform().IsIdentity()) {
    GetModifiablePath().Transform(GetState().GetTransform());
  }

  for (int i = 0, to_restore = state_stack_.back()->LayerSaveCount() - 1;
       i < to_restore; ++i) {
    canvas.restore();
  }

  canvas.restore();
  state_stack_.pop_back();
  CanvasRenderingContext2DState& state = GetState();
  state.ClearResolvedFilter();

  SetIsTransformInvertible(state.IsTransformInvertible());
  if (IsTransformInvertible() && !GetState().GetTransform().IsIdentity()) {
    GetModifiablePath().Transform(state.GetTransform().Inverse());
  }
}

void BaseRenderingContext2D::ValidateStateStackImpl(
    const cc::PaintCanvas* canvas) const {
  DCHECK_GE(state_stack_.size(), 1u);
  DCHECK_GT(state_stack_.size(),
            base::checked_cast<WTF::wtf_size_t>(layer_count_));

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
      DCHECK_EQ(base::checked_cast<WTF::wtf_size_t>(main_saves + layer_saves),
                state_stack_.size() + extra_layer_saves);
    }
  }
}

void BaseRenderingContext2D::RestoreMatrixClipStack(cc::PaintCanvas* c) const {
  if (!c)
    return;
  AffineTransform prev_transform;
  for (Member<CanvasRenderingContext2DState> curr_state : state_stack_) {
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
      c->setMatrix(AffineTransformToSkM44(curr_transform));
      prev_transform = curr_transform;
    }
  }
  ValidateStateStack(c);
}

void BaseRenderingContext2D::ResetInternal() {
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kReset);
  }
  ValidateStateStack();
  state_stack_.resize(1);
  state_stack_.front() = MakeGarbageCollected<CanvasRenderingContext2DState>();
  layer_count_ = 0;
  SetIsTransformInvertible(true);
  CanvasPath::Clear();
  if (MemoryManagedPaintRecorder* recorder = Recorder(); recorder != nullptr) {
    recorder->RestartRecording();
  }

  // If a WebGPU transfer texture exists, we must destroy it immediately. We
  // can't allow it to continue to exist, as it would be subject to Javascript
  // garbage-collection and could vanish any time Oilpan runs a sweep. Normally
  // it's okay for Oilpan to delete GPUTextures, since Dawn maintains its own
  // ownership graph of GPU resources, but in our case, destruction of the
  // GPUTexture will also result in destruction of the associated SharedImage.
  if (webgpu_access_texture_) {
    webgpu_access_texture_->destroy();
    webgpu_access_texture_ = nullptr;
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
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(op);
    IdentifiabilityUpdateForStyleUnion(v8_style);
  }
}

void BaseRenderingContext2D::
    UpdateIdentifiabilityStudyBeforeSettingStrokeOrFill(
        v8::Local<v8::String> v8_string,
        CanvasOps op) {
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(op);
    identifiability_study_helper_.UpdateBuilder(v8_string->GetIdentityHash());
  }
}

bool BaseRenderingContext2D::ExtractColorFromV8StringAndUpdateCache(
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

void BaseRenderingContext2D::setStrokeStyle(v8::Isolate* isolate,
                                            v8::Local<v8::Value> value,
                                            ExceptionState& exception_state) {
  CanvasRenderingContext2DState& state = GetState();
  // Use of a string for the stroke is very common (and parsing the color
  // from the string is expensive) so we keep a map of string to color.
  if (value->IsString()) {
    v8::Local<v8::String> v8_string = value.As<v8::String>();
    UpdateIdentifiabilityStudyBeforeSettingStrokeOrFill(
        v8_string, CanvasOps::kSetStrokeStyle);
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
  if (!ExtractV8CanvasStyle(isolate, value, v8_style, exception_state))
    return;

  UpdateIdentifiabilityStudyBeforeSettingStrokeOrFill(
      v8_style, CanvasOps::kSetStrokeStyle);

  switch (v8_style.type) {
    case V8CanvasStyleType::kCSSColorValue:
      state.SetStrokeColor(v8_style.css_color_value);
      break;
    case V8CanvasStyleType::kGradient:
      state.SetStrokeGradient(v8_style.gradient);
      break;
    case V8CanvasStyleType::kPattern:
      if (!origin_tainted_by_content_ && !v8_style.pattern->OriginClean())
        SetOriginTaintedByContent();
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

ColorParseResult BaseRenderingContext2D::ParseColorOrCurrentColor(
    const String& color_string,
    Color& color) const {
  const ColorParseResult parse_result =
      ParseCanvasColorString(color_string, color_scheme_, color,
                             GetColorProvider(), IsInWebAppScope());
  if (parse_result == ColorParseResult::kCurrentColor) {
    color = GetCurrentColor();
  }

  if (parse_result == ColorParseResult::kColorFunction) {
    const CSSValue* color_mix_value = CSSParser::ParseSingleValue(
        CSSPropertyID::kColor, color_string,
        StrictCSSParserContext(SecureContextMode::kInsecureContext));

    static const TextLinkColors kDefaultTextLinkColors{};
    auto* window = DynamicTo<LocalDOMWindow>(GetTopExecutionContext());
    const TextLinkColors& text_link_colors =
        window ? window->document()->GetTextLinkColors()
               : kDefaultTextLinkColors;
    // TODO(40946458): Don't use default length resolver here!
    const ResolveColorValueContext context{
        .length_resolver = CSSToLengthConversionData(),
        .text_link_colors = text_link_colors,
        .used_color_scheme = color_scheme_,
        .color_provider = GetColorProvider(),
        .is_in_web_app_scope = IsInWebAppScope()};
    const StyleColor style_color = ResolveColorValue(*color_mix_value, context);
    color = style_color.Resolve(GetCurrentColor(), color_scheme_);
    return ColorParseResult::kColor;
  }
  return parse_result;
}

const ui::ColorProvider* BaseRenderingContext2D::GetColorProvider() const {
  if (HTMLCanvasElement* canvas = HostAsHTMLCanvasElement()) {
    return canvas->GetDocument().GetColorProviderForPainting(color_scheme_);
  }

  return nullptr;
}

bool BaseRenderingContext2D::IsInWebAppScope() const {
  if (HTMLCanvasElement* canvas = HostAsHTMLCanvasElement()) {
    return canvas->GetDocument().IsInWebAppScope();
  }
  return false;
}

v8::Local<v8::Value> BaseRenderingContext2D::fillStyle(
    ScriptState* script_state) const {
  return CanvasStyleToV8(script_state, GetState().FillStyle());
}

void BaseRenderingContext2D::setFillStyle(v8::Isolate* isolate,
                                          v8::Local<v8::Value> value,
                                          ExceptionState& exception_state) {
  ValidateStateStack();

  CanvasRenderingContext2DState& state = GetState();
  // This block is similar to that in setStrokeStyle(), see comments there for
  // details on this.
  if (value->IsString()) {
    v8::Local<v8::String> v8_string = value.As<v8::String>();
    UpdateIdentifiabilityStudyBeforeSettingStrokeOrFill(
        v8_string, CanvasOps::kSetFillStyle);
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
  if (!ExtractV8CanvasStyle(isolate, value, v8_style, exception_state))
    return;

  UpdateIdentifiabilityStudyBeforeSettingStrokeOrFill(v8_style,
                                                      CanvasOps::kSetFillStyle);

  switch (v8_style.type) {
    case V8CanvasStyleType::kCSSColorValue:
      state.SetFillColor(v8_style.css_color_value);
      break;
    case V8CanvasStyleType::kGradient:
      state.SetFillGradient(v8_style.gradient);
      break;
    case V8CanvasStyleType::kPattern:
      if (!origin_tainted_by_content_ && !v8_style.pattern->OriginClean())
        SetOriginTaintedByContent();
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

double BaseRenderingContext2D::lineWidth() const {
  return GetState().LineWidth();
}

void BaseRenderingContext2D::setLineWidth(double width) {
  if (!std::isfinite(width) || width <= 0)
    return;
  CanvasRenderingContext2DState& state = GetState();
  if (state.LineWidth() == width) {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetLineWidth,
                                                width);
  }
  state.SetLineWidth(ClampTo<float>(width));
}

String BaseRenderingContext2D::lineCap() const {
  return LineCapName(GetState().GetLineCap());
}

void BaseRenderingContext2D::setLineCap(const String& s) {
  LineCap cap;
  if (!ParseLineCap(s, cap))
    return;
  CanvasRenderingContext2DState& state = GetState();
  if (state.GetLineCap() == cap) {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetLineCap, cap);
  }
  state.SetLineCap(cap);
}

String BaseRenderingContext2D::lineJoin() const {
  return LineJoinName(GetState().GetLineJoin());
}

void BaseRenderingContext2D::setLineJoin(const String& s) {
  LineJoin join;
  if (!ParseLineJoin(s, join))
    return;
  CanvasRenderingContext2DState& state = GetState();
  if (state.GetLineJoin() == join) {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetLineJoin, join);
  }
  state.SetLineJoin(join);
}

double BaseRenderingContext2D::miterLimit() const {
  return GetState().MiterLimit();
}

void BaseRenderingContext2D::setMiterLimit(double limit) {
  if (!std::isfinite(limit) || limit <= 0)
    return;
  CanvasRenderingContext2DState& state = GetState();
  if (state.MiterLimit() == limit) {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetMiterLimit,
                                                limit);
  }
  state.SetMiterLimit(ClampTo<float>(limit));
}

double BaseRenderingContext2D::shadowOffsetX() const {
  return GetState().ShadowOffset().x();
}

void BaseRenderingContext2D::setShadowOffsetX(double x) {
  if (!std::isfinite(x))
    return;
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

double BaseRenderingContext2D::shadowOffsetY() const {
  return GetState().ShadowOffset().y();
}

void BaseRenderingContext2D::setShadowOffsetY(double y) {
  if (!std::isfinite(y))
    return;
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

double BaseRenderingContext2D::shadowBlur() const {
  return GetState().ShadowBlur();
}

void BaseRenderingContext2D::setShadowBlur(double blur) {
  if (!std::isfinite(blur) || blur < 0)
    return;
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
  CanvasRenderingContext2DState& state = GetState();
  if (state.ShadowColor() == color) {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetShadowColor,
                                                color.Rgb());
  }
  state.SetShadowColor(color);
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
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetLineDash,
                                                base::make_span(dash));
  }
  GetState().SetLineDash(dash);
}

double BaseRenderingContext2D::lineDashOffset() const {
  return GetState().LineDashOffset();
}

void BaseRenderingContext2D::setLineDashOffset(double offset) {
  CanvasRenderingContext2DState& state = GetState();
  if (!std::isfinite(offset) || state.LineDashOffset() == offset) {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetLineDashOffset,
                                                offset);
  }
  state.SetLineDashOffset(ClampTo<float>(offset));
}

double BaseRenderingContext2D::globalAlpha() const {
  return GetState().GlobalAlpha();
}

void BaseRenderingContext2D::setGlobalAlpha(double alpha) {
  if (!(alpha >= 0 && alpha <= 1))
    return;
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

const V8UnionCanvasFilterOrString* BaseRenderingContext2D::filter() const {
  const CanvasRenderingContext2DState& state = GetState();
  if (CanvasFilter* filter = state.GetCanvasFilter()) {
    return MakeGarbageCollected<V8UnionCanvasFilterOrString>(filter);
  }
  return MakeGarbageCollected<V8UnionCanvasFilterOrString>(
      state.UnparsedCSSFilter());
}

void BaseRenderingContext2D::setFilter(
    ScriptState* script_state,
    const V8UnionCanvasFilterOrString* input) {
  if (!input)
    return;

  CanvasRenderingContext2DState& state = GetState();
  switch (input->GetContentType()) {
    case V8UnionCanvasFilterOrString::ContentType::kCanvasFilter:
      UseCounter::Count(GetTopExecutionContext(),
                        WebFeature::kCanvasRenderingContext2DCanvasFilter);
      state.SetCanvasFilter(input->GetAsCanvasFilter());
      SnapshotStateForFilter();
      // TODO(crbug.com/1234113): Instrument new canvas APIs.
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
      if (!css_value || css_value->IsCSSWideKeyword())
        return;
      state.SetUnparsedCSSFilter(filter_string);
      state.SetCSSFilter(css_value);
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

void BaseRenderingContext2D::rotate(double angle_in_radians) {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;

  if (!std::isfinite(angle_in_radians))
    return;
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

void BaseRenderingContext2D::translate(double tx, double ty) {
  // TODO(crbug.com/1140535): Investigate the performance impact of simply
  // calling the 3d version of this function
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;

  if (!IsTransformInvertible()) [[unlikely]] {
    return;
  }

  if (!std::isfinite(tx) || !std::isfinite(ty))
    return;
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

void BaseRenderingContext2D::resetTransform() {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kResetTransform);
  }

  CanvasRenderingContext2DState& state = GetState();
  AffineTransform ctm = state.GetTransform();
  bool invertible_ctm = IsTransformInvertible();
  // It is possible that CTM is identity while CTM is not invertible.
  // When CTM becomes non-invertible, realizeSaves() can make CTM identity.
  if (ctm.IsIdentity() && invertible_ctm)
    return;

  // resetTransform() resolves the non-invertible CTM state.
  state.ResetTransform();
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
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kBeginPath);
  }
}

void BaseRenderingContext2D::DrawPathInternal(
    const CanvasPath& path,
    CanvasRenderingContext2DState::PaintType paint_type,
    SkPathFillType fill_type,
    UsePaintCache use_paint_cache) {
  if (path.IsEmpty()) {
    return;
  }

  gfx::RectF bounds(path.BoundingRect());
  if (std::isnan(bounds.x()) || std::isnan(bounds.y()) ||
      std::isnan(bounds.width()) || std::isnan(bounds.height()))
    return;

  if (paint_type == CanvasRenderingContext2DState::kStrokePaintType)
    InflateStrokeRect(bounds);

  if (path.IsLine()) {
    if (paint_type == CanvasRenderingContext2DState::kFillPaintType)
        [[unlikely]] {
      // Filling a line is a no-op.
      // Also, SKCanvas::drawLine() ignores paint type and always strokes.
      return;
    }
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

  if (path.IsArc()) {
    const auto& arc = path.arc();
    const SkScalar x = WebCoreFloatToSkScalar(arc.x);
    const SkScalar y = WebCoreFloatToSkScalar(arc.y);
    const SkScalar radius = WebCoreFloatToSkScalar(arc.radius);
    const SkScalar diameter = radius + radius;
    const SkRect oval =
        SkRect::MakeXYWH(x - radius, y - radius, diameter, diameter);
    const SkScalar start_degrees =
        WebCoreFloatToSkScalar(arc.start_angle_radians * 180 / kPiFloat);
    const SkScalar sweep_degrees =
        WebCoreFloatToSkScalar(arc.sweep_angle_radians * 180 / kPiFloat);
    const bool closed = arc.closed;
    Draw<OverdrawOp::kNone>(
        [oval, start_degrees, sweep_degrees, closed](
            cc::PaintCanvas* c,
            const cc::PaintFlags* flags)  // draw lambda
        {
          cc::PaintFlags arc_paint_flags(*flags);
          arc_paint_flags.setArcClosed(closed);
          c->drawArc(oval, start_degrees, sweep_degrees, arc_paint_flags);
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

void BaseRenderingContext2D::fill() {
  FillImpl(SkPathFillType::kWinding);
}

void BaseRenderingContext2D::fill(const V8CanvasFillRule& winding) {
  FillImpl(CanvasFillRuleToSkiaFillType(winding));
}

void BaseRenderingContext2D::FillImpl(SkPathFillType winding_rule) {
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kFill, winding_rule);
  }
  DrawPathInternal(*this, CanvasRenderingContext2DState::kFillPaintType,
                   winding_rule, UsePaintCache::kDisabled);
}

void BaseRenderingContext2D::fill(Path2D* dom_path) {
  FillPathImpl(dom_path, SkPathFillType::kWinding);
}

void BaseRenderingContext2D::fill(Path2D* dom_path,
                                  const V8CanvasFillRule& winding) {
  FillPathImpl(dom_path, CanvasFillRuleToSkiaFillType(winding));
}

void BaseRenderingContext2D::FillPathImpl(Path2D* dom_path,
                                          SkPathFillType winding_rule) {
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kFill__Path, dom_path->GetIdentifiableToken(), winding_rule);
  }
  DrawPathInternal(*dom_path, CanvasRenderingContext2DState::kFillPaintType,
                   winding_rule, path2d_use_paint_cache_);
}

void BaseRenderingContext2D::stroke() {
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kStroke);
  }
  DrawPathInternal(*this, CanvasRenderingContext2DState::kStrokePaintType,
                   SkPathFillType::kWinding, UsePaintCache::kDisabled);
}

void BaseRenderingContext2D::stroke(Path2D* dom_path) {
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
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
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kFillRect, x, y,
                                                width, height);
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
      [rect](cc::PaintCanvas* c, const cc::PaintFlags* flags)  // draw lambda
      { c->drawRect(gfx::RectFToSkRect(rect), *flags); },
      [rect, this](const SkIRect& clip_bounds)  // overdraw test lambda
      { return RectContainsTransformedRect(rect, clip_bounds); },
      rect, CanvasRenderingContext2DState::kFillPaintType,
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
    SkPath path;
    path.moveTo(rect.x(), rect.y());
    path.lineTo(rect.right(), rect.bottom());
    path.close();
    canvas->drawPath(path, *flags);
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
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
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

void BaseRenderingContext2D::clip(const V8CanvasFillRule& winding_rule) {
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kClip,
        IdentifiabilitySensitiveStringToken(winding_rule.AsString()));
  }
  ClipInternal(GetPath(), winding_rule, UsePaintCache::kDisabled);
}

void BaseRenderingContext2D::clip(Path2D* dom_path,
                                  const V8CanvasFillRule& winding_rule) {
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kClip__Path, dom_path->GetIdentifiableToken(),
        IdentifiabilitySensitiveStringToken(winding_rule.AsString()));
  }
  ClipInternal(dom_path->GetPath(), winding_rule, path2d_use_paint_cache_);
}

bool BaseRenderingContext2D::isPointInPath(
    const double x,
    const double y,
    const V8CanvasFillRule& winding_rule) {
  return IsPointInPathInternal(GetPath(), x, y, winding_rule);
}

bool BaseRenderingContext2D::isPointInPath(
    Path2D* dom_path,
    const double x,
    const double y,
    const V8CanvasFillRule& winding_rule) {
  return IsPointInPathInternal(dom_path->GetPath(), x, y, winding_rule);
}

bool BaseRenderingContext2D::IsPointInPathInternal(
    const Path& path,
    const double x,
    const double y,
    const V8CanvasFillRule& winding_rule) {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return false;
  if (!IsTransformInvertible()) [[unlikely]] {
    return false;
  }

  if (!std::isfinite(x) || !std::isfinite(y))
    return false;
  gfx::PointF point(ClampTo<float>(x), ClampTo<float>(y));
  AffineTransform ctm = GetState().GetTransform();
  gfx::PointF transformed_point = ctm.Inverse().MapPoint(point);

  return path.Contains(
      transformed_point,
      SkFillTypeToWindRule(CanvasFillRuleToSkiaFillType(winding_rule)));
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
  if (!IsTransformInvertible()) [[unlikely]] {
    return false;
  }

  if (!std::isfinite(x) || !std::isfinite(y))
    return false;
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
  base::ranges::copy(state.LineDash(), line_dash.begin());
  stroke_data.SetLineDash(line_dash, state.LineDashOffset());
  return path.StrokeContains(transformed_point, stroke_data, ctm);
}

cc::PaintFlags BaseRenderingContext2D::GetClearFlags() const {
  cc::PaintFlags clear_flags;
  clear_flags.setStyle(cc::PaintFlags::kFill_Style);
  if (HasAlpha()) {
    clear_flags.setBlendMode(SkBlendMode::kClear);
  } else {
    clear_flags.setColor(SK_ColorBLACK);
  }
  return clear_flags;
}

void BaseRenderingContext2D::clearRect(double x,
                                       double y,
                                       double width,
                                       double height) {
  if (!ValidateRectForCanvas(x, y, width, height))
    return;

  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;
  if (!IsTransformInvertible()) [[unlikely]] {
    return;
  }

  SkIRect clip_bounds;
  if (!c->getDeviceClipBounds(&clip_bounds))
    return;
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kClearRect, x, y,
                                                width, height);
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
    try_restore_context_event_timer_.StartRepeating(kTryRestoreContextInterval,
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
  if (image_source->IsVideoElement()) {
    if (!static_cast<HTMLVideoElement*>(image_source)
             ->HasAvailableVideoFrame()) {
      return;
    }
  } else if (image_source->IsVideoFrame()) {
    if (!static_cast<VideoFrame*>(image_source)->frame()) {
      return;
    }
  } else {
    image = image_source->GetSourceImageForCanvas(
        FlushReason::kDrawImage, &source_image_status, default_object_size,
        kPremultiplyAlpha);
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
    if (!image || !image->width() || !image->height())
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
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kDrawImage, fsx, fsy, fsw, fsh, fdx, fdy, fdw, fdh,
        image ? image->width() : 0, image ? image->height() : 0);
    identifiability_study_helper_.set_encountered_partially_digested_image();
  }

  ValidateStateStack();

  WillDrawImage(image_source);

  if (!origin_tainted_by_content_ && WouldTaintCanvasOrigin(image_source)) {
    SetOriginTaintedByContent();
  }

  Draw<OverdrawOp::kDrawImage>(
      [this, image_source, image, src_rect, dst_rect](
          cc::PaintCanvas* c, const cc::PaintFlags* flags)  // draw lambda
      {
        // TODO: investigate relocating this adjustment to paint_op.cc.
        cc::PaintFlags::ScalingOperation scaling_option =
            cc::PaintFlags::ScalingOperation::kUnknown;
        SkSize scale;
        if (c->getLocalToDevice().asM33().decomposeScale(&scale)) {
          scaling_option = (scale.width() > 1 && scale.height() > 1)
                               ? cc::PaintFlags::ScalingOperation::kUpscale
                               : cc::PaintFlags::ScalingOperation::kUnknown;
        }
        SkSamplingOptions sampling =
            cc::PaintFlags::FilterQualityToSkSamplingOptions(
                flags ? flags->getFilterQuality()
                      : cc::PaintFlags::FilterQuality::kNone,
                scaling_option);
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

// TODO(b/349835587): This is just a stub for now.
void BaseRenderingContext2D::placeElement(Element* element,
                                          double x,
                                          double y,
                                          ExceptionState& exception_state) {
  HTMLCanvasElement* canvas = HostAsHTMLCanvasElement();
  if (!element->IsDescendantOf(canvas)) {
    exception_state.ThrowTypeError(
        "Only elements that are part of the canvas fallback content subtree "
        "(i.e. children of the <canvas> element) can be used with "
        "placeElement().");
  }

  canvas->SetHasPlacedElements();
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
      image_source->GetSourceImageForCanvas(FlushReason::kCreatePattern,
                                            &status, default_object_size,
                                            kPremultiplyAlpha);

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
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }

  if (!image_for_rendering)
    return nullptr;

  bool origin_clean = !WouldTaintCanvasOrigin(image_source);

  auto* pattern = MakeGarbageCollected<CanvasPattern>(
      std::move(image_for_rendering), repeat_mode, origin_clean);
  pattern->SetExecutionContext(
      identifiability_study_helper_.execution_context());
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

  const size_t size = array->length() / 2;
  std::vector<SkPoint> skpoints(size);
  std::memcpy(skpoints.data(), array->Data(), size * sizeof(SkPoint));

  return base::MakeRefCounted<cc::RefCountedBuffer<SkPoint>>(
      std::move(skpoints));
}

}  // namespace

Mesh2DVertexBuffer* BaseRenderingContext2D::createMesh2DVertexBuffer(
    NotShared<DOMFloat32Array> array,
    ExceptionState& exception_state) {
  scoped_refptr<cc::RefCountedBuffer<SkPoint>> buffer = MakeSkPointBuffer(
      array, exception_state,
      "The vertex buffer must contain a non-zero, even number of floats.");

  return buffer ? MakeGarbageCollected<Mesh2DVertexBuffer>(std::move(buffer))
                : nullptr;
}

Mesh2DUVBuffer* BaseRenderingContext2D::createMesh2DUVBuffer(
    NotShared<DOMFloat32Array> array,
    ExceptionState& exception_state) {
  scoped_refptr<cc::RefCountedBuffer<SkPoint>> buffer = MakeSkPointBuffer(
      array, exception_state,
      "The UV buffer must contain a non-zero, even number of floats.");

  return buffer ? MakeGarbageCollected<Mesh2DUVBuffer>(std::move(buffer))
                : nullptr;
}

Mesh2DIndexBuffer* BaseRenderingContext2D::createMesh2DIndexBuffer(
    NotShared<DOMUint16Array> array,
    ExceptionState& exception_state) {
  if ((array->length() == 0) || (array->length() % 3)) {
    exception_state.ThrowRangeError(
        "The index buffer must contain a non-zero, multiple of three number of "
        "uints.");
    return nullptr;
  }

  return MakeGarbageCollected<Mesh2DIndexBuffer>(
      base::MakeRefCounted<cc::RefCountedBuffer<uint16_t>>(
          std::vector<uint16_t>(array->Data(),
                                array->Data() + array->length())));
}

void BaseRenderingContext2D::drawMesh(
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
      FlushReason::kDrawMesh, &source_image_status,
      gfx::SizeF(Width(), Height()), kPremultiplyAlpha);
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

  WillDrawImage(image_source);

  if (!origin_tainted_by_content_ && WouldTaintCanvasOrigin(image_source)) {
    SetOriginTaintedByContent();
  }

  SkRect bounds;
  bounds.setBounds(vertex_data->data().data(),
                   SkToInt(vertex_data->data().size()));

  Draw<OverdrawOp::kNone>(
      /*draw_func=*/
      [&image, &vertex_data, &uv_data, &index_data](
          cc::PaintCanvas* c, const cc::PaintFlags* flags) {
        const gfx::RectF src(image->width(), image->height());
        // UV coordinates are normalized, relative to the texture size.
        const SkMatrix local_matrix =
            SkMatrix::Scale(1.0f / image->width(), 1.0f / image->height());

        cc::PaintFlags scoped_flags(*flags);
        image->ApplyShader(scoped_flags, local_matrix, src, ImageDrawOptions());
        c->drawVertices(vertex_data, uv_data, index_data, scoped_flags);
      },
      kNoOverdraw,
      gfx::RectF(bounds.x(), bounds.y(), bounds.width(), bounds.height()),
      CanvasRenderingContext2DState::PaintType::kFillPaintType,
      image_source->IsOpaque() ? CanvasRenderingContext2DState::kOpaqueImage
                               : CanvasRenderingContext2DState::kNonOpaqueImage,
      CanvasPerformanceMonitor::DrawType::kOther);
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
      image_data->Size().width(), image_data->Size().height(), std::nullopt,
      image_data->getSettings(), params, exception_state);
}

ImageData* BaseRenderingContext2D::createImageData(
    int sw,
    int sh,
    ExceptionState& exception_state) const {
  ImageData::ValidateAndCreateParams params;
  params.context_2d_error_mode = true;
  params.default_color_space = GetDefaultImageDataColorSpace();
  return ImageData::ValidateAndCreate(std::abs(sw), std::abs(sh), std::nullopt,
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
  return ImageData::ValidateAndCreate(std::abs(sw), std::abs(sh), std::nullopt,
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
perfetto::EventContext GetEventContext();

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

  if (layer_count_ != 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "`getImageData()` cannot be called with open layers.");
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

  if (isContextLost() || !CanCreateCanvas2dResourceProvider()) [[unlikely]] {
    return ImageData::ValidateAndCreate(
        sw, sh, std::nullopt, image_data_settings, validate_and_create_params,
        exception_state);
  }

  // Deferred offscreen canvases might have recorded commands, make sure
  // that those get drawn here
  FinalizeFrame(FlushReason::kGetImageData);

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
      if (auto* execution_context = GetTopExecutionContext()) {
        const String& message =
            "Canvas2D: Multiple readback operations using getImageData are "
            "faster with the willReadFrequently attribute set to true. See: "
            "https://html.spec.whatwg.org/multipage/"
            "canvas.html#concept-canvas-will-read-frequently";
        execution_context->AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kRendering,
                mojom::blink::ConsoleMessageLevel::kWarning, message));
      }
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
      GetImage(FlushReason::kGetImageData);

  TRACE_EVENT_INSTANT(
      TRACE_DISABLED_BY_DEFAULT("identifiability.high_entropy_api"),
      "CanvasReadback", perfetto::Flow::FromPointer(this),
      [&](perfetto::EventContext ctx) {
        String data = "data:,";
        if (snapshot) {
          std::unique_ptr<ImageDataBuffer> data_buffer =
              ImageDataBuffer::Create(snapshot);
          if (data_buffer) {
            data = data_buffer->ToDataURL(ImageEncodingMimeType::kMimeTypePng,
                                          -1.0);
          }
        }
        ctx.AddDebugAnnotation("data_url", data.Utf8());
      });

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
      ImageData::ValidateAndCreate(sw, sh, std::nullopt, image_data_settings,
                                   validate_and_create_params, exception_state);
  if (!image_data)
    return nullptr;

  // Read pixels into |image_data|.
  if (snapshot) {
    gfx::Rect snapshot_rect{snapshot->Size()};
    if (!snapshot_rect.Intersects(image_data_rect)) {
      // If the readback area is completely out of bounds just return a zero
      // initialized buffer. No point in trying to perform out of bounds read.
      CHECK(validate_and_create_params.zero_initialize);
      return image_data;
    }

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

  if (layer_count_ != 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "`putImageData()` cannot be called with open layers.");
    return;
  }

  bool hasResourceProvider = CanCreateCanvas2dResourceProvider();
  if (!hasResourceProvider)
    return;

  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
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
        NOTREACHED_IN_MIGRATION()
            << "Failed to convert ImageData with writePixels.";

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
  const CanvasRenderingContext2DState& state = GetState();
  double delta = state.LineWidth() / 2;
  if (state.GetLineJoin() == kMiterJoin) {
    delta *= state.MiterLimit();
  } else if (state.GetLineCap() == kSquareCap) {
    delta *= kRoot2;
  }

  rect.Outset(ClampTo<float>(delta));
}

bool BaseRenderingContext2D::imageSmoothingEnabled() const {
  return GetState().ImageSmoothingEnabled();
}

void BaseRenderingContext2D::setImageSmoothingEnabled(bool enabled) {
  CanvasRenderingContext2DState& state = GetState();
  if (enabled == state.ImageSmoothingEnabled()) {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kSetImageSmoothingEnabled, enabled);
  }

  state.SetImageSmoothingEnabled(enabled);
}

V8ImageSmoothingQuality BaseRenderingContext2D::imageSmoothingQuality() const {
  return GetState().ImageSmoothingQuality();
}

void BaseRenderingContext2D::setImageSmoothingQuality(
    const V8ImageSmoothingQuality& quality) {
  CanvasRenderingContext2DState& state = GetState();
  if (quality == state.ImageSmoothingQuality()) {
    return;
  }

  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kSetImageSmoothingQuality,
        IdentifiabilitySensitiveStringToken(quality.AsString()));
  }
  state.SetImageSmoothingQuality(quality);
}

String BaseRenderingContext2D::letterSpacing() const {
  return GetState().GetLetterSpacing();
}

String BaseRenderingContext2D::wordSpacing() const {
  return GetState().GetWordSpacing();
}

String BaseRenderingContext2D::textRenderingAsString() const {
  return GetState().GetTextRendering().AsString();
}

V8CanvasTextRendering BaseRenderingContext2D::textRendering() const {
  return GetState().GetTextRendering();
}

float BaseRenderingContext2D::GetFontBaseline(
    const SimpleFontData& font_data) const {
  return TextMetrics::GetFontBaseline(GetState().GetTextBaseline(), font_data);
}

String BaseRenderingContext2D::textAlign() const {
  return TextAlignName(GetState().GetTextAlign());
}

void BaseRenderingContext2D::setTextAlign(const String& s) {
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kSetTextAlign, IdentifiabilityBenignStringToken(s));
  }
  TextAlign align;
  if (!ParseTextAlign(s, align))
    return;
  CanvasRenderingContext2DState& state = GetState();
  if (state.GetTextAlign() == align) {
    return;
  }
  state.SetTextAlign(align);
}

String BaseRenderingContext2D::textBaseline() const {
  return TextBaselineName(GetState().GetTextBaseline());
}

void BaseRenderingContext2D::setTextBaseline(const String& s) {
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kSetTextBaseline, IdentifiabilityBenignStringToken(s));
  }
  TextBaseline baseline;
  if (!ParseTextBaseline(s, baseline))
    return;
  CanvasRenderingContext2DState& state = GetState();
  if (state.GetTextBaseline() == baseline) {
    return;
  }
  state.SetTextBaseline(baseline);
}

String BaseRenderingContext2D::fontKerning() const {
  return FontDescription::ToString(GetState().GetFontKerning()).LowerASCII();
}

V8CanvasFontStretch BaseRenderingContext2D::fontStretch() const {
  return GetState().GetFontStretch();
}

String BaseRenderingContext2D::fontStretchAsString() const {
  return GetState().GetFontStretch().AsString();
}

String BaseRenderingContext2D::fontVariantCaps() const {
  return FontDescription::ToStringForIdl(GetState().GetFontVariantCaps());
}

void BaseRenderingContext2D::Trace(Visitor* visitor) const {
  visitor->Trace(state_stack_);
  visitor->Trace(dispatch_context_lost_event_timer_);
  visitor->Trace(dispatch_context_restored_event_timer_);
  visitor->Trace(try_restore_context_event_timer_);
  visitor->Trace(color_cache_);
  visitor->Trace(webgpu_access_texture_);
  visitor->Trace(placed_elements_);
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

void BaseRenderingContext2D::WillUseCurrentFont() const {
  if (HTMLCanvasElement* canvas = HostAsHTMLCanvasElement();
      canvas != nullptr) {
    canvas->GetDocument().GetCanvasFontCache()->WillUseCurrentFont();
  }
}

String BaseRenderingContext2D::font() const {
  const CanvasRenderingContext2DState& state = GetState();
  if (!state.HasRealizedFont()) {
    return kDefaultFont;
  }

  WillUseCurrentFont();
  StringBuilder serialized_font;
  const FontDescription& font_description = state.GetFontDescription();

  if (font_description.Style() == kItalicSlopeValue) {
    serialized_font.Append("italic ");
  }
  if (font_description.Weight() == kBoldWeightValue) {
    serialized_font.Append("bold ");
  } else if (font_description.Weight() != kNormalWeightValue) {
    int weight_as_int = static_cast<int>((float)font_description.Weight());
    serialized_font.AppendNumber(weight_as_int);
    serialized_font.Append(" ");
  }
  if (font_description.VariantCaps() == FontDescription::kSmallCaps) {
    serialized_font.Append("small-caps ");
  }

  serialized_font.AppendNumber(font_description.ComputedSize());
  serialized_font.Append("px ");

  serialized_font.Append(
      ComputedStyleUtils::ValueForFontFamily(font_description.Family())
          ->CssText());

  return serialized_font.ToString();
}

bool BaseRenderingContext2D::WillSetFont() const {
  return true;
}

bool BaseRenderingContext2D::CurrentFontResolvedAndUpToDate() const {
  return GetState().HasRealizedFont();
}

void BaseRenderingContext2D::setFont(const String& new_font) {
  if (!WillSetFont()) [[unlikely]] {
    return;
  }

  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(
        CanvasOps::kSetFont, IdentifiabilityBenignStringToken(new_font));
  }

  CanvasRenderingContext2DState& state = GetState();
  if (new_font == state.UnparsedFont() && CurrentFontResolvedAndUpToDate()) {
    return;
  }

  if (!ResolveFont(new_font)) {
    return;
  }

  // The parse succeeded.
  state.SetUnparsedFont(new_font);
}

static inline TextDirection ToTextDirection(
    CanvasRenderingContext2DState::Direction direction,
    HTMLCanvasElement* canvas,
    const ComputedStyle** computed_style = nullptr) {
  const ComputedStyle* style =
      (canvas &&
       (computed_style ||
        direction == CanvasRenderingContext2DState::kDirectionInherit))
          ? canvas->EnsureComputedStyle()
          : nullptr;
  if (computed_style) {
    *computed_style = style;
  }
  switch (direction) {
    case CanvasRenderingContext2DState::kDirectionInherit:
      return style ? style->Direction() : TextDirection::kLtr;
    case CanvasRenderingContext2DState::kDirectionRTL:
      return TextDirection::kRtl;
    case CanvasRenderingContext2DState::kDirectionLTR:
      return TextDirection::kLtr;
  }
  NOTREACHED_IN_MIGRATION();
  return TextDirection::kLtr;
}

HTMLCanvasElement* BaseRenderingContext2D::HostAsHTMLCanvasElement() const {
  return nullptr;
}

OffscreenCanvas* BaseRenderingContext2D::HostAsOffscreenCanvas() const {
  return nullptr;
}

String BaseRenderingContext2D::direction() const {
  HTMLCanvasElement* canvas = HostAsHTMLCanvasElement();
  const CanvasRenderingContext2DState& state = GetState();
  if (state.GetDirection() ==
          CanvasRenderingContext2DState::kDirectionInherit &&
      canvas) {
    canvas->GetDocument().UpdateStyleAndLayoutTreeForElement(
        canvas, DocumentUpdateReason::kCanvas);
  }
  return ToTextDirection(state.GetDirection(), canvas) == TextDirection::kRtl
             ? kRtlDirectionString
             : kLtrDirectionString;
}

void BaseRenderingContext2D::setDirection(const String& direction_string) {
  CanvasRenderingContext2DState::Direction direction;
  if (direction_string == kInheritDirectionString) {
    direction = CanvasRenderingContext2DState::kDirectionInherit;
  } else if (direction_string == kRtlDirectionString) {
    direction = CanvasRenderingContext2DState::kDirectionRTL;
  } else if (direction_string == kLtrDirectionString) {
    direction = CanvasRenderingContext2DState::kDirectionLTR;
  } else {
    return;
  }

  CanvasRenderingContext2DState& state = GetState();
  if (state.GetDirection() == direction) {
    return;
  }

  state.SetDirection(direction);
}

void BaseRenderingContext2D::fillText(const String& text, double x, double y) {
  DrawTextInternal(text, x, y, CanvasRenderingContext2DState::kFillPaintType);
}

void BaseRenderingContext2D::fillText(const String& text,
                                      double x,
                                      double y,
                                      double max_width) {
  DrawTextInternal(text, x, y, CanvasRenderingContext2DState::kFillPaintType,
                   &max_width);
}

void BaseRenderingContext2D::strokeText(const String& text,
                                        double x,
                                        double y) {
  DrawTextInternal(text, x, y, CanvasRenderingContext2DState::kStrokePaintType);
}

void BaseRenderingContext2D::strokeText(const String& text,
                                        double x,
                                        double y,
                                        double max_width) {
  DrawTextInternal(text, x, y, CanvasRenderingContext2DState::kStrokePaintType,
                   &max_width);
}

const Font& BaseRenderingContext2D::AccessFont(HTMLCanvasElement* canvas) {
  const CanvasRenderingContext2DState& state = GetState();
  if (!state.HasRealizedFont()) {
    setFont(state.UnparsedFont());
  }
  if (canvas) {
    canvas->GetDocument().GetCanvasFontCache()->WillUseCurrentFont();
  }
  return state.GetFont();
}

void BaseRenderingContext2D::DrawTextInternal(
    const String& text,
    double x,
    double y,
    CanvasRenderingContext2DState::PaintType paint_type,
    double* max_width) {
  HTMLCanvasElement* canvas = HostAsHTMLCanvasElement();
  if (canvas) {
    // The style resolution required for fonts is not available in frame-less
    // documents.
    if (!canvas->GetDocument().GetFrame()) {
      return;
    }

    // accessFont needs the style to be up to date, but updating style can cause
    // script to run, (e.g. due to autofocus) which can free the canvas (set
    // size to 0, for example), so update style before grabbing the PaintCanvas.
    canvas->GetDocument().UpdateStyleAndLayoutTreeForElement(
        canvas, DocumentUpdateReason::kCanvas);
  }

  // Abort if we don't have a paint canvas (e.g. the context was lost).
  cc::PaintCanvas* paint_canvas = GetOrCreatePaintCanvas();
  if (!paint_canvas) {
    return;
  }

  if (!std::isfinite(x) || !std::isfinite(y)) {
    return;
  }
  if (max_width && (!std::isfinite(*max_width) || *max_width <= 0)) {
    return;
  }

  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(
        paint_type == CanvasRenderingContext2DState::kFillPaintType
            ? CanvasOps::kFillText
            : CanvasOps::kStrokeText,
        IdentifiabilitySensitiveStringToken(text), x, y,
        max_width ? *max_width : -1);
    identifiability_study_helper_.set_encountered_sensitive_ops();
  }

  const Font& font = AccessFont(canvas);
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);
  if (!font_data) {
    return;
  }

  // FIXME: Need to turn off font smoothing.

  const ComputedStyle* computed_style = nullptr;
  const CanvasRenderingContext2DState& state = GetState();
  TextDirection direction =
      ToTextDirection(state.GetDirection(), canvas, &computed_style);
  bool is_rtl = direction == TextDirection::kRtl;
  bool bidi_override =
      computed_style ? IsOverride(computed_style->GetUnicodeBidi()) : false;

  TextRun text_run(text, direction, bidi_override);
  text_run.SetNormalizeSpace(true);
  // Draw the item text at the correct point.
  gfx::PointF location(ClampTo<float>(x),
                       ClampTo<float>(y + GetFontBaseline(*font_data)));
  gfx::RectF bounds;
  double font_width = font.Width(text_run, &bounds);

  bool use_max_width = (max_width && *max_width < font_width);
  double width = use_max_width ? *max_width : font_width;

  TextAlign align = state.GetTextAlign();
  if (align == kStartTextAlign) {
    align = is_rtl ? kRightTextAlign : kLeftTextAlign;
  } else if (align == kEndTextAlign) {
    align = is_rtl ? kLeftTextAlign : kRightTextAlign;
  }

  switch (align) {
    case kCenterTextAlign:
      location.set_x(location.x() - width / 2);
      break;
    case kRightTextAlign:
      location.set_x(location.x() - width);
      break;
    default:
      break;
  }

  bounds.Offset(location.x(), location.y());
  if (paint_type == CanvasRenderingContext2DState::kStrokePaintType) {
    InflateStrokeRect(bounds);
  }

  if (use_max_width) {
    paint_canvas->save();
    // We draw when fontWidth is 0 so compositing operations (eg, a "copy" op)
    // still work. As the width of canvas is scaled, so text can be scaled to
    // match the given maxwidth, update text location so it appears on desired
    // place.
    paint_canvas->scale(ClampTo<float>(width / font_width), 1);
    location.set_x(location.x() / ClampTo<float>(width / font_width));
  }

  Draw<OverdrawOp::kNone>(
      [this, text = std::move(text), direction, bidi_override, location,
       canvas](cc::PaintCanvas* c, const cc::PaintFlags* flags)  // draw lambda
      {
        TextRun text_run(text, direction, bidi_override);
        text_run.SetNormalizeSpace(true);
        TextRunPaintInfo text_run_paint_info(text_run);
        // Font::DrawType::kGlyphsAndClusters is required for printing to PDF,
        // otherwise the character to glyph mapping will not be reversible,
        // which prevents text data from being extracted from PDF files or
        // from the print preview. This is only needed in vector printing mode
        // (i.e. when rendering inside the beforeprint event listener),
        // because in all other cases the canvas is just a rectangle of pixels.
        // Note: Test coverage for this is assured by manual (non-automated)
        // web test printing/manual/canvas2d-vector-text.html
        // That test should be run manually against CLs that touch this code.
        Font::DrawType draw_type = (canvas && canvas->IsPrinting())
                                       ? Font::DrawType::kGlyphsAndClusters
                                       : Font::DrawType::kGlyphsOnly;
        this->AccessFont(canvas).DrawBidiText(c, text_run_paint_info, location,
                                              Font::kUseFallbackIfFontNotReady,
                                              *flags, draw_type);
      },
      [](const SkIRect& rect)  // overdraw test lambda
      { return false; },
      bounds, paint_type, CanvasRenderingContext2DState::kNoImage,
      CanvasPerformanceMonitor::DrawType::kText);

  if (use_max_width) {
    // Make sure that `paint_canvas` is still valid and active. Calling `Draw`
    // might reset `paint_canvas`. If that happens, `GetOrCreatePaintCanvas`
    // will create a new `paint_canvas` and return a new address. This new
    // canvas won't have the `save()` added above, so it would be invalid to
    // call `restore()` here.
    if (paint_canvas == GetOrCreatePaintCanvas()) {
      paint_canvas->restore();
    }
  }
  ValidateStateStack();
}

TextMetrics* BaseRenderingContext2D::measureText(const String& text) {
  // The style resolution required for fonts is not available in frame-less
  // documents.
  HTMLCanvasElement* canvas = HostAsHTMLCanvasElement();

  if (canvas) {
    if (!canvas->GetDocument().GetFrame()) {
      return MakeGarbageCollected<TextMetrics>();
    }

    canvas->GetDocument().UpdateStyleAndLayoutTreeForElement(
        canvas, DocumentUpdateReason::kCanvas);
  }

  const Font& font = AccessFont(canvas);

  const CanvasRenderingContext2DState& state = GetState();
  TextDirection direction = ToTextDirection(state.GetDirection(), canvas);

  return MakeGarbageCollected<TextMetrics>(
      font, direction, state.GetTextBaseline(), state.GetTextAlign(), text);
}

void BaseRenderingContext2D::SnapshotStateForFilter() {
  auto* canvas = HostAsHTMLCanvasElement();
  // The style resolution required for fonts is not available in frame-less
  // documents.
  if (canvas && !canvas->GetDocument().GetFrame()) {
    return;
  }

  GetState().SetFontForFilter(AccessFont(canvas));
}

void BaseRenderingContext2D::setLetterSpacing(const String& letter_spacing) {
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DLetterSpacing);
  // TODO(crbug.com/1234113): Instrument new canvas APIs.
  identifiability_study_helper_.set_encountered_skipped_ops();
  CanvasRenderingContext2DState& state = GetState();
  if (!state.HasRealizedFont()) {
    setFont(font());
  }

  state.SetLetterSpacing(letter_spacing);
}

void BaseRenderingContext2D::setWordSpacing(const String& word_spacing) {
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DWordSpacing);
  // TODO(crbug.com/1234113): Instrument new canvas APIs.
  identifiability_study_helper_.set_encountered_skipped_ops();

  CanvasRenderingContext2DState& state = GetState();
  if (!state.HasRealizedFont()) {
    setFont(font());
  }

  state.SetWordSpacing(word_spacing);
}

void BaseRenderingContext2D::setTextRenderingAsString(
    const String& text_rendering_string) {
  std::optional<blink::V8CanvasTextRendering> text_value =
      V8CanvasTextRendering::Create(text_rendering_string);

  if (!text_value.has_value()) {
    return;
  }
  setTextRendering(text_value.value());
}

void BaseRenderingContext2D::setTextRendering(
    const V8CanvasTextRendering& text_rendering) {
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DTextRendering);
  // TODO(crbug.com/1234113): Instrument new canvas APIs.
  identifiability_study_helper_.set_encountered_skipped_ops();
  CanvasRenderingContext2DState& state = GetState();
  if (!state.HasRealizedFont()) {
    setFont(font());
  }

  if (state.GetTextRendering() == text_rendering) {
    return;
  }
  state.SetTextRendering(text_rendering, GetFontSelector());
}

void BaseRenderingContext2D::setFontKerning(const String& font_kerning_string) {
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DFontKerning);
  // TODO(crbug.com/1234113): Instrument new canvas APIs.
  identifiability_study_helper_.set_encountered_skipped_ops();
  CanvasRenderingContext2DState& state = GetState();
  if (!state.HasRealizedFont()) {
    setFont(font());
  }
  FontDescription::Kerning kerning;
  if (font_kerning_string == kAutoKerningString) {
    kerning = FontDescription::kAutoKerning;
  } else if (font_kerning_string == kNoneKerningString) {
    kerning = FontDescription::kNoneKerning;
  } else if (font_kerning_string == kNormalKerningString) {
    kerning = FontDescription::kNormalKerning;
  } else {
    return;
  }

  if (state.GetFontKerning() == kerning) {
    return;
  }

  state.SetFontKerning(kerning, GetFontSelector());
}

void BaseRenderingContext2D::setFontStretchAsString(
    const String& font_stretch) {
  std::optional<V8CanvasFontStretch> font_value =
      V8CanvasFontStretch::Create(font_stretch);

  if (!font_value.has_value()) {
    return;
  }
  setFontStretch(font_value.value());
}

void BaseRenderingContext2D::setFontStretch(
    const V8CanvasFontStretch& font_stretch) {
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DFontStretch);
  // TODO(crbug.com/1234113): Instrument new canvas APIs.
  identifiability_study_helper_.set_encountered_skipped_ops();
  CanvasRenderingContext2DState& state = GetState();
  if (!state.HasRealizedFont()) {
    setFont(font());
  }

  if (state.GetFontStretch() == font_stretch) {
    return;
  }
  state.SetFontStretch(font_stretch, GetFontSelector());
}

void BaseRenderingContext2D::setFontVariantCaps(
    const String& font_variant_caps_string) {
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DFontVariantCaps);
  // TODO(crbug.com/1234113): Instrument new canvas APIs.
  identifiability_study_helper_.set_encountered_skipped_ops();
  CanvasRenderingContext2DState& state = GetState();
  if (!state.HasRealizedFont()) {
    setFont(font());
  }
  FontDescription::FontVariantCaps variant_caps;
  if (font_variant_caps_string == kNormalVariantString) {
    variant_caps = FontDescription::kCapsNormal;
  } else if (font_variant_caps_string == kSmallCapsVariantString) {
    variant_caps = FontDescription::kSmallCaps;
  } else if (font_variant_caps_string == kAllSmallCapsVariantString) {
    variant_caps = FontDescription::kAllSmallCaps;
  } else if (font_variant_caps_string == kPetiteVariantString) {
    variant_caps = FontDescription::kPetiteCaps;
  } else if (font_variant_caps_string == kAllPetiteVariantString) {
    variant_caps = FontDescription::kAllPetiteCaps;
  } else if (font_variant_caps_string == kUnicaseVariantString) {
    variant_caps = FontDescription::kUnicase;
  } else if (font_variant_caps_string == kTitlingCapsVariantString) {
    variant_caps = FontDescription::kTitlingCaps;
  } else {
    return;
  }

  if (state.GetFontVariantCaps() == variant_caps) {
    return;
  }

  state.SetFontVariantCaps(variant_caps, GetFontSelector());
}

FontSelector* BaseRenderingContext2D::GetFontSelector() const {
  return nullptr;
}

bool BaseRenderingContext2D::IsAccelerated() const {
  CanvasRenderingContextHost* host = GetCanvasRenderingContextHost();
  if (host) {
    return host->GetRasterMode() == RasterMode::kGPU;
  }
  return false;
}

V8GPUTextureFormat BaseRenderingContext2D::getTextureFormat() const {
  // Query the canvas and return its actual texture format.
  std::optional<V8GPUTextureFormat> format;
  if (const CanvasRenderingContextHost* host =
          GetCanvasRenderingContextHost()) {
    format = FromDawnEnum(
        AsDawnType(host->GetRenderingContextSkColorInfo().colorType()));
  }

  // If that did not work (e.g., the canvas host does not yet exist), we can
  // return the preferred canvas format.
  if (!format.has_value()) {
    format = FromDawnEnum(GPU::preferred_canvas_format());
  }

  // If the preferred canvas format cannot be represented as a GPUTextureFormat,
  // something is wrong; we need to investigate.
  CHECK(format.has_value()) << "GPU::preferred_canvas_format() returned an "
                               "unrecognized texture format";
  return *format;
}

GPUTexture* BaseRenderingContext2D::transferToGPUTexture(
    const Canvas2dGPUTransferOption* access_options,
    ExceptionState& exception_state) {
  if (!OriginClean()) {
    exception_state.ThrowSecurityError(
        "The canvas has been tainted by cross-origin data.");
    return nullptr;
  }

  blink::GPUDevice* blink_device = access_options->getDeviceOr(nullptr);
  if (!blink_device) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "GPUDevice cannot be null.");
    return nullptr;
  }

  // Verify that we are not inside a canvas layer.
  if (layer_count_ > 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "A layer is currently active.");
    return nullptr;
  }

  // Verify that the usage flags are supported.
  constexpr wgpu::TextureUsage kSupportedUsageFlags =
      wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
      wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::RenderAttachment;

  // If `transferToGPUTexture` is called twice without an intervening call to
  // `transferBackFromGPUTexture`, the semantics are that the current ongoing
  // transfer should be discarded and the new transfer given the 2D canvas in
  // its current state (defined to be blank post-initiation of the first
  // transfer but then incorporating any canvas 2D operations that have
  // subsequently occurred on the canvas). Implement that semantics here.
  // Note that the canvas will have been made blank by the removal of the
  // CanvasResourceProvider at the initiation of the first transfer but will
  // then incorporate any canvas 2D operations that have subsequently occurred
  // on the canvas via the usage of the CanvasResourceProvider that those
  // operations would have caused to be created as the source for the new
  // transfer below.
  if (webgpu_access_texture_) {
    webgpu_access_texture_->destroy();
    webgpu_access_texture_ = nullptr;
    resource_provider_from_webgpu_access_.reset();
  }

  wgpu::TextureUsage tex_usage =
      AsDawnFlags<wgpu::TextureUsage>(access_options->usage());
  if (tex_usage & ~kSupportedUsageFlags) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Usage flags are not supported.");
    return nullptr;
  }

  // Prepare to flush the canvas to a WebGPU texture.
  FinalizeFrame(FlushReason::kWebGPUTexture);

  // We will need to access the canvas' resource provider.
  CanvasRenderingContextHost* host = GetCanvasRenderingContextHost();
  if (!host) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Unable to access canvas image.");
    return nullptr;
  }
  host->SetTransferToGPUTextureWasInvoked();

  // Ensure that the canvas host lives on the GPU. This call is a no-op if the
  // host is already accelerated.
  // TODO(crbug.com/340911120): if the user requested WillReadFrequently, do we
  // want to behave differently here?
  const bool host_is_accelerated = host->EnableAcceleration();

  // A texture needs to exist on the GPU. If we aren't able to enable
  // acceleration, the canvas pixels live on the CPU and we weren't able to
  // transfer them; in that case, WebGPU access is not possible.
  CanvasResourceProvider* provider =
      host->GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU);
  if (!host_is_accelerated || !provider || !provider->IsAccelerated()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Unable to transfer canvas to GPU.");
    return nullptr;
  }

  // Get the SharedImage backing this canvas resource, signaling that an
  // external write will occur. This call will ensure that a copy occurs if
  // needed for CopyOnWrite or for creation of a SharedImage with WebGPU usage
  // and will end the canvas access.
  gpu::SyncToken canvas_access_sync_token;
  bool performed_copy = false;
  scoped_refptr<gpu::ClientSharedImage> client_si =
      host->ResourceProvider()->GetBackingClientSharedImageForExternalWrite(
          &canvas_access_sync_token,
          gpu::SHARED_IMAGE_USAGE_WEBGPU_READ |
              gpu::SHARED_IMAGE_USAGE_WEBGPU_WRITE,
          &performed_copy);
  if (access_options->requireZeroCopy() && performed_copy) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Transferring canvas to GPU was not zero-copy.");
  }

  // If the backing SharedImage is not available (e.g., because the GPU context
  // has been lost), zero-copy transfer is not possible.
  if (!client_si) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Unable to transfer canvas to GPU.");
  }

  wgpu::TextureFormat dawn_format =
      AsDawnType(viz::ToClosestSkColorType(true, client_si->format()));
  wgpu::TextureDescriptor desc = {
      .usage = tex_usage,
      .size = {base::checked_cast<uint32_t>(client_si->size().width()),
               base::checked_cast<uint32_t>(client_si->size().height())},
      .format = dawn_format,
  };

  // Create a WebGPU texture backed by the resource's SharedImage.
  scoped_refptr<WebGPUMailboxTexture> texture =
      WebGPUMailboxTexture::FromExistingMailbox(
          blink_device->GetDawnControlClient(), blink_device->GetHandle(), desc,
          client_si->mailbox(),
          // Ensure that WebGPU waits for the 2D canvas service-side operations
          // on this resource to complete.
          canvas_access_sync_token);

  webgpu_access_texture_ = MakeGarbageCollected<GPUTexture>(
      blink_device, dawn_format, tex_usage, std::move(texture),
      access_options->getLabelOr(String()));

  // We take away the canvas' resource provider here, which will cause the
  // canvas to be treated as a brand new surface if additional draws occur.
  // It also gives us a mechanism to detect post-transfer-out draws, which is
  // used in `transferBackFromWebGPU` to raise an exception.
  resource_provider_from_webgpu_access_ =
      host->ReplaceResourceProvider(nullptr);

  // The user isn't obligated to ever transfer back, which means this resource
  // provider might stick around for while. Jettison any unnecessary resources.
  resource_provider_from_webgpu_access_->ClearRecycledResources();

  return webgpu_access_texture_;
}

void BaseRenderingContext2D::transferBackFromGPUTexture(
    ExceptionState& exception_state) {
  // If the context is lost or doesn't exist, this call should be a no-op.
  // We don't want to throw an exception or attempt any changes if
  // `transferBackFromWebGPU` is called during teardown.
  CanvasRenderingContextHost* host = GetCanvasRenderingContextHost();
  if (!host || isContextLost()) [[unlikely]] {
    return;
  }

  // Prevent unbalanced calls to transferBackFromGPUTexture without an earlier
  // call to transferToGPUTexture.
  if (!webgpu_access_texture_ || !resource_provider_from_webgpu_access_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "This canvas is not currently in use by WebGPU.");
    webgpu_access_texture_ = nullptr;
    resource_provider_from_webgpu_access_ = nullptr;
    return;
  }

  // If this canvas already has a resource provider, this means that drawing has
  // occurred after `transferToWebGPU`. We disallow transferring back in this
  // case, and raise an exception instead.
  if (host->ResourceProvider()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The canvas was touched after transferToGPUTexture.");
    webgpu_access_texture_ = nullptr;
    resource_provider_from_webgpu_access_ = nullptr;
    return;
  }

  // If the caller explicitly destroyed the WebGPU access texture, there is
  // nothing to transfer.
  if (webgpu_access_texture_->Destroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The texture has been destroyed.");
    webgpu_access_texture_ = nullptr;
    resource_provider_from_webgpu_access_ = nullptr;
    return;
  }

  // Restore the canvas' resource provider back onto the canvas host,
  // surrendering our temporary ownership of the provider.
  CanvasResourceProvider* resource_provider =
      resource_provider_from_webgpu_access_.get();
  host->ReplaceResourceProvider(
      std::move(resource_provider_from_webgpu_access_));
  resource_provider->SetCanvasResourceHost(host);

  // Disassociate the WebGPU texture from the SharedImage to end its
  // SharedImage access.
  gpu::SyncToken webgpu_completion_sync_token =
      webgpu_access_texture_->GetMailboxTexture()->Dissociate();

  // Signal to the resource provider that the external write to the resource has
  // completed to ensure that it waits on the WebGPU service-side operations to
  // complete before any further canvas operations occur.
  resource_provider->EndExternalWrite(webgpu_completion_sync_token);

  // Destroy the WebGPU texture to prevent it from being used after
  // `transferBackFromGPUTexture`.
  webgpu_access_texture_->destroy();

  // We are finished with the WebGPU texture and its associated device.
  webgpu_access_texture_ = nullptr;
}

}  // namespace blink
