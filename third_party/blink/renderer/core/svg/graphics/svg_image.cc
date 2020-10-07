/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/animation/smil_time_container.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_chrome_client.h"
#include "third_party/blink/renderer/core/svg/svg_animated_preserve_aspect_ratio.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "third_party/blink/renderer/core/svg/svg_fe_image_element.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/image_observer.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

using TaskRunnerHandle = scheduler::WebResourceLoadingTaskRunnerHandle;

class FailingLoader final : public WebURLLoader {
 public:
  explicit FailingLoader(std::unique_ptr<TaskRunnerHandle> task_runner_handle)
      : task_runner_handle_(std::move(task_runner_handle)) {}
  ~FailingLoader() override = default;

  // WebURLLoader implementation:
  void LoadSynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequest::ExtraData> request_extra_data,
      int requestor_id,
      bool download_to_network_cache_only,
      bool pass_response_pipe_to_client,
      bool no_mime_sniffing,
      base::TimeDelta timeout_interval,
      WebURLLoaderClient*,
      WebURLResponse&,
      base::Optional<WebURLError>& error,
      WebData&,
      int64_t& encoded_data_length,
      int64_t& encoded_body_length,
      WebBlobInfo& downloaded_blob,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper) override {
    NOTREACHED();
  }
  void LoadAsynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequest::ExtraData> request_extra_data,
      int requestor_id,
      bool download_to_network_cache_only,
      bool no_mime_sniffing,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      WebURLLoaderClient* client) override {
    NOTREACHED();
  }
  void SetDefersLoading(bool) override {}
  void DidChangePriority(WebURLRequest::Priority, int) override {}
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override {
    return task_runner_handle_->GetTaskRunner();
  }

 private:
  const std::unique_ptr<TaskRunnerHandle> task_runner_handle_;
};

class FailingLoaderFactory final : public WebURLLoaderFactory {
 public:
  // WebURLLoaderFactory implementation:
  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const WebURLRequest&,
      std::unique_ptr<TaskRunnerHandle> task_runner_handle) override {
    return std::make_unique<FailingLoader>(std::move(task_runner_handle));
  }
};

}  // namespace

// SVGImageLocalFrameClient is used to wait until SVG document's load event
// in the case where there are subresources asynchronously loaded.
//
// Reference cycle: SVGImage -(Persistent)-> Page -(Member)-> Frame -(Member)->
// FrameClient == SVGImageLocalFrameClient -(raw)-> SVGImage.
class SVGImage::SVGImageLocalFrameClient : public EmptyLocalFrameClient {
 public:
  SVGImageLocalFrameClient(SVGImage* image) : image_(image) {}

  void ClearImage() { image_ = nullptr; }

 private:
  std::unique_ptr<WebURLLoaderFactory> CreateURLLoaderFactory() override {
    // SVG Images have unique security rules that prevent all subresource
    // requests except for data urls.
    return std::make_unique<FailingLoaderFactory>();
  }

  void DispatchDidHandleOnloadEvents() override {
    // The SVGImage was destructed before SVG load completion.
    if (!image_)
      return;

    image_->LoadCompleted();
  }

  // Cleared manually by SVGImage's destructor when |image_| is destructed.
  SVGImage* image_;
};

SVGImage::SVGImage(ImageObserver* observer, bool is_multipart)
    : Image(observer, is_multipart),
      paint_controller_(std::make_unique<PaintController>()),
      has_pending_timeline_rewind_(false) {}

SVGImage::~SVGImage() {
  AllowDestroyingLayoutObjectInFinalizerScope scope;

  if (frame_client_)
    frame_client_->ClearImage();

  if (page_) {
    // It is safe to allow UA events within this scope, because event
    // dispatching inside the SVG image's document doesn't trigger JavaScript
    // execution. All script execution is forbidden when an SVG is loaded as an
    // image subresource - see SetScriptEnabled in SVGImage::DataChanged().
    EventDispatchForbiddenScope::AllowUserAgentEvents allow_events;
    // Store m_page in a local variable, clearing m_page, so that
    // SVGImageChromeClient knows we're destructed.
    Page* current_page = page_.Release();
    // Break both the loader and view references to the frame
    current_page->WillBeDestroyed();
  }

  // Verify that page teardown destroyed the Chrome
  DCHECK(!chrome_client_ || !chrome_client_->GetImage());
}

bool SVGImage::IsInSVGImage(const Node* node) {
  DCHECK(node);

  Page* page = node->GetDocument().GetPage();
  if (!page)
    return false;

  return page->GetChromeClient().IsSVGImageChromeClient();
}

void SVGImage::CheckLoaded() const {
  CHECK(page_);

  auto* frame = To<LocalFrame>(page_->MainFrame());

  // Failures of this assertion might result in wrong origin tainting checks,
  // because CurrentFrameHasSingleSecurityOrigin() assumes all subresources of
  // the SVG are loaded and thus ready for origin checks.
  CHECK(frame->GetDocument()->LoadEventFinished());
}

bool SVGImage::CurrentFrameHasSingleSecurityOrigin() const {
  if (!page_)
    return true;

  auto* frame = To<LocalFrame>(page_->MainFrame());

  CheckLoaded();

  SVGSVGElement* root_element =
      frame->GetDocument()->AccessSVGExtensions().rootElement();
  if (!root_element)
    return true;

  // Don't allow foreignObject elements or images that are not known to be
  // single-origin since these can leak cross-origin information.
  for (Node* node = root_element; node; node = FlatTreeTraversal::Next(*node)) {
    if (IsA<SVGForeignObjectElement>(*node))
      return false;
    if (auto* image = DynamicTo<SVGImageElement>(*node)) {
      if (!image->CurrentFrameHasSingleSecurityOrigin())
        return false;
    } else if (auto* fe_image = DynamicTo<SVGFEImageElement>(*node)) {
      if (!fe_image->CurrentFrameHasSingleSecurityOrigin())
        return false;
    }
  }

  // Because SVG image rendering disallows external resources and links, these
  // images effectively are restricted to a single security origin.
  return true;
}

static SVGSVGElement* SvgRootElement(Page* page) {
  if (!page)
    return nullptr;
  auto* frame = To<LocalFrame>(page->MainFrame());
  return frame->GetDocument()->AccessSVGExtensions().rootElement();
}

LayoutSize SVGImage::ContainerSize() const {
  SVGSVGElement* root_element = SvgRootElement(page_.Get());
  if (!root_element)
    return LayoutSize();

  LayoutSVGRoot* layout_object =
      ToLayoutSVGRoot(root_element->GetLayoutObject());
  if (!layout_object)
    return LayoutSize();

  // If a container size is available it has precedence.
  LayoutSize container_size = layout_object->ContainerSize();
  if (!container_size.IsEmpty())
    return container_size;

  // Assure that a container size is always given for a non-identity zoom level.
  DCHECK_EQ(layout_object->StyleRef().EffectiveZoom(), 1);

  // No set container size; use concrete object size.
  return intrinsic_size_;
}

IntSize SVGImage::Size() const {
  return RoundedIntSize(intrinsic_size_);
}

static float ResolveWidthForRatio(float height,
                                  const FloatSize& intrinsic_ratio) {
  return height * intrinsic_ratio.Width() / intrinsic_ratio.Height();
}

static float ResolveHeightForRatio(float width,
                                   const FloatSize& intrinsic_ratio) {
  return width * intrinsic_ratio.Height() / intrinsic_ratio.Width();
}

bool SVGImage::HasIntrinsicDimensions() const {
  return !ConcreteObjectSize(FloatSize()).IsEmpty();
}

bool SVGImage::HasIntrinsicSizingInfo() const {
  SVGSVGElement* svg = SvgRootElement(page_.Get());
  return svg && svg->GetLayoutObject();
}

bool SVGImage::GetIntrinsicSizingInfo(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  SVGSVGElement* svg = SvgRootElement(page_.Get());
  if (!svg)
    return false;

  LayoutSVGRoot* layout_object = ToLayoutSVGRoot(svg->GetLayoutObject());
  if (!layout_object)
    return false;

  layout_object->UnscaledIntrinsicSizingInfo(intrinsic_sizing_info);
  return true;
}

FloatSize SVGImage::ConcreteObjectSize(
    const FloatSize& default_object_size) const {
  IntrinsicSizingInfo intrinsic_sizing_info;
  if (!GetIntrinsicSizingInfo(intrinsic_sizing_info))
    return FloatSize();

  // https://www.w3.org/TR/css3-images/#default-sizing
  if (intrinsic_sizing_info.has_width && intrinsic_sizing_info.has_height)
    return intrinsic_sizing_info.size;

  // We're not using an intrinsic aspect ratio to resolve a missing
  // intrinsic width or height when preserveAspectRatio is none.
  // (Ref: crbug.com/584172)
  SVGSVGElement* svg = SvgRootElement(page_.Get());
  if (svg->preserveAspectRatio()->CurrentValue()->Align() ==
      SVGPreserveAspectRatio::kSvgPreserveaspectratioNone)
    return default_object_size;

  if (intrinsic_sizing_info.has_width) {
    if (intrinsic_sizing_info.aspect_ratio.IsEmpty())
      return FloatSize(intrinsic_sizing_info.size.Width(),
                       default_object_size.Height());

    return FloatSize(intrinsic_sizing_info.size.Width(),
                     ResolveHeightForRatio(intrinsic_sizing_info.size.Width(),
                                           intrinsic_sizing_info.aspect_ratio));
  }

  if (intrinsic_sizing_info.has_height) {
    if (intrinsic_sizing_info.aspect_ratio.IsEmpty())
      return FloatSize(default_object_size.Width(),
                       intrinsic_sizing_info.size.Height());

    return FloatSize(ResolveWidthForRatio(intrinsic_sizing_info.size.Height(),
                                          intrinsic_sizing_info.aspect_ratio),
                     intrinsic_sizing_info.size.Height());
  }

  if (!intrinsic_sizing_info.aspect_ratio.IsEmpty()) {
    // "A contain constraint is resolved by setting the concrete object size to
    //  the largest rectangle that has the object's intrinsic aspect ratio and
    //  additionally has neither width nor height larger than the constraint
    //  rectangle's width and height, respectively."
    float solution_width = ResolveWidthForRatio(
        default_object_size.Height(), intrinsic_sizing_info.aspect_ratio);
    if (solution_width <= default_object_size.Width())
      return FloatSize(solution_width, default_object_size.Height());

    float solution_height = ResolveHeightForRatio(
        default_object_size.Width(), intrinsic_sizing_info.aspect_ratio);
    return FloatSize(default_object_size.Width(), solution_height);
  }

  return default_object_size;
}

template <typename Func>
void SVGImage::ForContainer(const FloatSize& container_size, Func&& func) {
  if (!page_)
    return;

  // Temporarily disable the image observer to prevent changeInRect() calls due
  // re-laying out the image.
  ImageObserverDisabler image_observer_disabler(this);

  LayoutSize rounded_container_size = RoundedLayoutSize(container_size);

  if (SVGSVGElement* root_element = SvgRootElement(page_.Get())) {
    if (LayoutSVGRoot* layout_object =
            ToLayoutSVGRoot(root_element->GetLayoutObject()))
      layout_object->SetContainerSize(rounded_container_size);
  }

  func(FloatSize(rounded_container_size.Width() / container_size.Width(),
                 rounded_container_size.Height() / container_size.Height()));
}

void SVGImage::DrawForContainer(cc::PaintCanvas* canvas,
                                const PaintFlags& flags,
                                const FloatSize& container_size,
                                float zoom,
                                const FloatRect& dst_rect,
                                const FloatRect& src_rect,
                                const KURL& url) {
  ForContainer(container_size, [&](const FloatSize& residual_scale) {
    FloatRect scaled_src = src_rect;
    scaled_src.Scale(1 / zoom);

    // Compensate for the container size rounding by adjusting the source rect.
    FloatSize adjusted_src_size = scaled_src.Size();
    adjusted_src_size.Scale(residual_scale.Width(), residual_scale.Height());
    scaled_src.SetSize(adjusted_src_size);

    DrawInternal(canvas, flags, dst_rect, scaled_src, kRespectImageOrientation,
                 kClampImageToSourceRect, url);
  });
}

PaintImage SVGImage::PaintImageForCurrentFrame() {
  auto builder =
      CreatePaintImageBuilder().set_completion_state(completion_state());
  PopulatePaintRecordForCurrentFrameForContainer(builder, Size(), 1, NullURL());
  return builder.TakePaintImage();
}

void SVGImage::DrawPatternForContainer(GraphicsContext& context,
                                       const FloatSize container_size,
                                       float zoom,
                                       const FloatRect& src_rect,
                                       const FloatSize& tile_scale,
                                       const FloatPoint& phase,
                                       SkBlendMode composite_op,
                                       const FloatRect& dst_rect,
                                       const FloatSize& repeat_spacing,
                                       const KURL& url) {
  // Tile adjusted for scaling/stretch.
  FloatRect tile(src_rect);
  tile.Scale(tile_scale.Width(), tile_scale.Height());

  // Expand the tile to account for repeat spacing.
  FloatRect spaced_tile(tile);
  spaced_tile.Expand(FloatSize(repeat_spacing));

  PaintRecordBuilder builder(nullptr, &context);
  {
    DrawingRecorder recorder(builder.Context(), builder,
                             DisplayItem::Type::kSVGImage);
    // When generating an expanded tile, make sure we don't draw into the
    // spacing area.
    if (tile != spaced_tile)
      builder.Context().Clip(tile);
    PaintFlags flags;
    DrawForContainer(builder.Context().Canvas(), flags, container_size, zoom,
                     tile, src_rect, url);
  }
  sk_sp<PaintRecord> record = builder.EndRecording();

  SkMatrix pattern_transform;
  pattern_transform.setTranslate(phase.X() + spaced_tile.X(),
                                 phase.Y() + spaced_tile.Y());

  PaintFlags flags;
  flags.setShader(
      PaintShader::MakePaintRecord(record, spaced_tile, SkTileMode::kRepeat,
                                   SkTileMode::kRepeat, &pattern_transform));
  // If the shader could not be instantiated (e.g. non-invertible matrix),
  // draw transparent.
  // Note: we can't simply bail, because of arbitrary blend mode.
  if (!flags.HasShader())
    flags.setColor(SK_ColorTRANSPARENT);

  flags.setBlendMode(composite_op);
  flags.setColorFilter(sk_ref_sp(context.GetColorFilter()));
  context.DrawRect(dst_rect, flags);

  StartAnimation();
}

void SVGImage::PopulatePaintRecordForCurrentFrameForContainer(
    PaintImageBuilder& builder,
    const IntSize& zoomed_container_size,
    float zoom,
    const KURL& url) {
  if (!page_)
    return;

  const IntRect container_rect(IntPoint(), zoomed_container_size);
  // Compute a new container size based on the zoomed (and potentially
  // rounded) size.
  FloatSize container_size(zoomed_container_size);
  container_size.Scale(1 / zoom);

  PaintRecorder recorder;
  cc::PaintCanvas* canvas = recorder.beginRecording(container_rect);
  DrawForContainer(canvas, PaintFlags(), container_size, zoom,
                   FloatRect(container_rect), FloatRect(container_rect), url);
  builder.set_paint_record(recorder.finishRecordingAsPicture(), container_rect,
                           PaintImage::GetNextContentId());
}

static bool DrawNeedsLayer(const PaintFlags& flags) {
  if (SkColorGetA(flags.getColor()) < 255)
    return true;

  // This is needed to preserve the dark mode filter that
  // has been set in GraphicsContext.
  if (flags.getColorFilter())
    return true;

  return flags.getBlendMode() != SkBlendMode::kSrcOver;
}

bool SVGImage::ApplyShaderInternal(PaintFlags& flags,
                                   const SkMatrix& local_matrix,
                                   const KURL& url) {
  const FloatSize size(ContainerSize());
  if (size.IsEmpty())
    return false;

  FloatRect bounds(FloatPoint(), size);
  flags.setShader(PaintShader::MakePaintRecord(
      PaintRecordForCurrentFrame(url), bounds, SkTileMode::kRepeat,
      SkTileMode::kRepeat, &local_matrix));

  // Animation is normally refreshed in draw() impls, which we don't reach when
  // painting via shaders.
  StartAnimation();

  return true;
}

bool SVGImage::ApplyShader(PaintFlags& flags, const SkMatrix& local_matrix) {
  return ApplyShaderInternal(flags, local_matrix, NullURL());
}

bool SVGImage::ApplyShaderForContainer(const FloatSize& container_size,
                                       float zoom,
                                       const KURL& url,
                                       PaintFlags& flags,
                                       const SkMatrix& local_matrix) {
  bool result = false;
  ForContainer(container_size, [&](const FloatSize& residual_scale) {
    // Compensate for the container size rounding.
    auto adjusted_local_matrix = local_matrix;
    adjusted_local_matrix.preScale(zoom * residual_scale.Width(),
                                   zoom * residual_scale.Height());

    result = ApplyShaderInternal(flags, adjusted_local_matrix, url);
  });

  return result;
}

void SVGImage::Draw(
    cc::PaintCanvas* canvas,
    const PaintFlags& flags,
    const FloatRect& dst_rect,
    const FloatRect& src_rect,
    RespectImageOrientationEnum should_respect_image_orientation,
    ImageClampingMode clamp_mode,
    ImageDecodingMode) {
  if (!page_)
    return;

  DrawInternal(canvas, flags, dst_rect, src_rect,
               should_respect_image_orientation, clamp_mode, NullURL());
}

sk_sp<PaintRecord> SVGImage::PaintRecordForCurrentFrame(const KURL& url) {
  DCHECK(page_);
  LocalFrameView* view = To<LocalFrame>(page_->MainFrame())->View();
  IntSize rounded_container_size = RoundedIntSize(ContainerSize());
  view->Resize(rounded_container_size);
  page_->GetVisualViewport().SetSize(rounded_container_size);

  // Always call processUrlFragment, even if the url is empty, because
  // there may have been a previous url/fragment that needs to be reset.
  view->ProcessUrlFragment(url, /*same_document_navigation=*/false);

  // If the image was reset, we need to rewind the timeline back to 0. This
  // needs to be done before painting, or else we wouldn't get the correct
  // reset semantics (we'd paint the "last" frame rather than the one at
  // time=0.) The reason we do this here and not in resetAnimation() is to
  // avoid setting timers from the latter.
  FlushPendingTimelineRewind();

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    view->UpdateAllLifecyclePhases(DocumentUpdateReason::kSVGImage);
    return view->GetPaintRecord();
  }

  view->UpdateAllLifecyclePhasesExceptPaint(DocumentUpdateReason::kSVGImage);
  PaintRecordBuilder builder(nullptr, nullptr, paint_controller_.get());
  view->PaintOutsideOfLifecycle(builder.Context(), kGlobalPaintNormalPhase);
  return builder.EndRecording();
}

void SVGImage::DrawInternal(cc::PaintCanvas* canvas,
                            const PaintFlags& flags,
                            const FloatRect& dst_rect,
                            const FloatRect& src_rect,
                            RespectImageOrientationEnum,
                            ImageClampingMode,
                            const KURL& url) {
  {
    PaintCanvasAutoRestore ar(canvas, false);
    if (DrawNeedsLayer(flags)) {
      SkRect layer_rect = dst_rect;
      canvas->saveLayer(&layer_rect, &flags);
    }
    // We can only draw the entire frame, clipped to the rect we want. So
    // compute where the top left of the image would be if we were drawing
    // without clipping, and translate accordingly.
    canvas->save();
    canvas->clipRect(EnclosingIntRect(dst_rect));
    canvas->concat(SkMatrix::MakeRectToRect(src_rect, dst_rect,
                                            SkMatrix::kFill_ScaleToFit));
    canvas->drawPicture(PaintRecordForCurrentFrame(url));
    canvas->restore();
  }

  // Start any (SMIL) animations if needed. This will restart or continue
  // animations if preceded by calls to resetAnimation or stopAnimation
  // respectively.
  StartAnimation();
}

void SVGImage::ScheduleTimelineRewind() {
  has_pending_timeline_rewind_ = true;
}

void SVGImage::FlushPendingTimelineRewind() {
  if (!has_pending_timeline_rewind_)
    return;
  if (SVGSVGElement* root_element = SvgRootElement(page_.Get()))
    root_element->setCurrentTime(0);
  has_pending_timeline_rewind_ = false;
}

void SVGImage::StartAnimation() {
  SVGSVGElement* root_element = SvgRootElement(page_.Get());
  if (!root_element)
    return;
  chrome_client_->ResumeAnimation();
  if (root_element->animationsPaused())
    root_element->unpauseAnimations();
}

void SVGImage::StopAnimation() {
  SVGSVGElement* root_element = SvgRootElement(page_.Get());
  if (!root_element)
    return;
  chrome_client_->SuspendAnimation();
  root_element->pauseAnimations();
}

void SVGImage::ResetAnimation() {
  SVGSVGElement* root_element = SvgRootElement(page_.Get());
  if (!root_element)
    return;
  chrome_client_->SuspendAnimation();
  root_element->pauseAnimations();
  ScheduleTimelineRewind();
}

void SVGImage::RestoreAnimation() {
  // If the image has no animations then do nothing.
  if (!MaybeAnimated())
    return;
  // If there are no clients, or no client is going to render, then do nothing.
  ImageObserver* image_observer = GetImageObserver();
  if (!image_observer || image_observer->ShouldPauseAnimation(this))
    return;
  StartAnimation();
}

bool SVGImage::MaybeAnimated() {
  SVGSVGElement* root_element = SvgRootElement(page_.Get());
  if (!root_element)
    return false;
  return root_element->TimeContainer()->HasAnimations() ||
         To<LocalFrame>(page_->MainFrame())
             ->GetDocument()
             ->Timeline()
             .HasPendingUpdates();
}

void SVGImage::ServiceAnimations(
    base::TimeTicks monotonic_animation_start_time) {
  if (!GetImageObserver())
    return;

  // If none of our observers (sic!) are visible, or for some other reason
  // does not want us to keep running animations, stop them until further
  // notice (next paint.)
  if (GetImageObserver()->ShouldPauseAnimation(this)) {
    StopAnimation();
    return;
  }

  // serviceScriptedAnimations runs requestAnimationFrame callbacks, but SVG
  // images can't have any so we assert there's no script.
  ScriptForbiddenScope forbid_script;

  // The calls below may trigger GCs, so set up the required persistent
  // reference on the ImageResourceContent which owns this SVGImage. By
  // transitivity, that will keep the associated SVGImageChromeClient object
  // alive.
  Persistent<ImageObserver> protect(GetImageObserver());
  page_->Animator().ServiceScriptedAnimations(monotonic_animation_start_time);

  // Do *not* update the paint phase. It's critical to paint only when
  // actually generating painted output, not only for performance reasons,
  // but to preserve correct coherence of the cache of the output with
  // the needsRepaint bits of the PaintLayers in the image.
  LocalFrameView* frame_view = To<LocalFrame>(page_->MainFrame())->View();
  frame_view->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kSVGImage);

  // We run UpdateAnimations after the paint phase, but per the above comment,
  // we don't want to run lifecycle through to paint for SVG images. Since we
  // know SVG images never have composited animations, we can update animations
  // directly without worrying about including PaintArtifactCompositor's
  // analysis of whether animations should be composited.
  frame_view->GetLayoutView()
      ->GetDocument()
      .GetDocumentAnimations()
      .UpdateAnimations(DocumentLifecycle::kLayoutClean, nullptr);
}

void SVGImage::AdvanceAnimationForTesting() {
  if (SVGSVGElement* root_element = SvgRootElement(page_.Get())) {
    root_element->TimeContainer()->AdvanceFrameForTesting();

    // The following triggers animation updates which can issue a new draw
    // and temporarily change the animation timeline. It's necessary to call
    // reset before changing to a time value as animation clock does not
    // expect to go backwards.
    base::TimeTicks current_animation_time =
        page_->Animator().Clock().CurrentTime();
    page_->Animator().Clock().ResetTimeForTesting();
    if (root_element->TimeContainer()->IsStarted())
      root_element->TimeContainer()->ResetDocumentTime();
    page_->Animator().ServiceScriptedAnimations(
        root_element->GetDocument().Timeline().ZeroTime() +
        base::TimeDelta::FromSecondsD(root_element->getCurrentTime()));
    GetImageObserver()->Changed(this);
    page_->Animator().Clock().ResetTimeForTesting();
    page_->Animator().Clock().UpdateTime(current_animation_time);
  }
}

SVGImageChromeClient& SVGImage::ChromeClientForTesting() {
  return *chrome_client_;
}

void SVGImage::UpdateUseCounters(const Document& document) const {
  if (SVGSVGElement* root_element = SvgRootElement(page_.Get())) {
    if (root_element->TimeContainer()->HasAnimations()) {
      document.CountUse(WebFeature::kSVGSMILAnimationInImageRegardlessOfCache);
    }
  }
}

void SVGImage::LoadCompleted() {
  switch (load_state_) {
    case kInDataChanged:
      load_state_ = kLoadCompleted;
      break;

    case kWaitingForAsyncLoadCompletion:
      load_state_ = kLoadCompleted;

      // Because LoadCompleted() is called synchronously from
      // Document::ImplicitClose(), we defer AsyncLoadCompleted() to avoid
      // potential bugs and timing dependencies around ImplicitClose() and
      // to make LoadEventFinished() true when AsyncLoadCompleted() is called.
      To<LocalFrame>(page_->MainFrame())
          ->GetTaskRunner(TaskType::kInternalLoading)
          ->PostTask(FROM_HERE, WTF::Bind(&SVGImage::NotifyAsyncLoadCompleted,
                                          scoped_refptr<SVGImage>(this)));
      break;

    case kDataChangedNotStarted:
    case kLoadCompleted:
      CHECK(false);
      break;
  }
}

void SVGImage::NotifyAsyncLoadCompleted() {
  if (GetImageObserver())
    GetImageObserver()->AsyncLoadCompleted(this);
}

Image::SizeAvailability SVGImage::DataChanged(bool all_data_received) {
  TRACE_EVENT0("blink", "SVGImage::dataChanged");

  // Don't do anything if is an empty image.
  if (!Data()->size())
    return kSizeAvailable;

  if (!all_data_received)
    return page_ ? kSizeAvailable : kSizeUnavailable;

  CHECK(!page_);

  // SVGImage will fire events (and the default C++ handlers run) but doesn't
  // actually allow script to run so it's fine to call into it. We allow this
  // since it means an SVG data url can synchronously load like other image
  // types.
  EventDispatchForbiddenScope::AllowUserAgentEvents allow_user_agent_events;

  CHECK_EQ(load_state_, kDataChangedNotStarted);
  load_state_ = kInDataChanged;

  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  chrome_client_ = MakeGarbageCollected<SVGImageChromeClient>(this);
  page_clients.chrome_client = chrome_client_.Get();

  // FIXME: If this SVG ends up loading itself, we might leak the world.
  // The Cache code does not know about ImageResources holding Frames and
  // won't know to break the cycle.
  // This will become an issue when SVGImage will be able to load other
  // SVGImage objects, but we're safe now, because SVGImage can only be
  // loaded by a top-level document.
  Page* page;
  {
    TRACE_EVENT0("blink", "SVGImage::dataChanged::createPage");
    page = Page::CreateNonOrdinary(page_clients);
    page->GetSettings().SetScriptEnabled(false);
    page->GetSettings().SetPluginsEnabled(false);

    // Because this page is detached, it can't get default font settings
    // from the embedder. Copy over font settings so we have sensible
    // defaults. These settings are fixed and will not update if changed.
    if (!Page::OrdinaryPages().IsEmpty()) {
      Settings& default_settings =
          (*Page::OrdinaryPages().begin())->GetSettings();
      page->GetSettings().GetGenericFontFamilySettings() =
          default_settings.GetGenericFontFamilySettings();
      page->GetSettings().SetMinimumFontSize(
          default_settings.GetMinimumFontSize());
      page->GetSettings().SetMinimumLogicalFontSize(
          default_settings.GetMinimumLogicalFontSize());
      page->GetSettings().SetDefaultFontSize(
          default_settings.GetDefaultFontSize());
      page->GetSettings().SetDefaultFixedFontSize(
          default_settings.GetDefaultFixedFontSize());

      // Also copy the preferred-color-scheme to ensure a responsiveness to
      // dark/light color schemes.
      page->GetSettings().SetPreferredColorScheme(
          default_settings.GetPreferredColorScheme());
    }
  }

  LocalFrame* frame = nullptr;
  {
    TRACE_EVENT0("blink", "SVGImage::dataChanged::createFrame");
    DCHECK(!frame_client_);
    frame_client_ = MakeGarbageCollected<SVGImageLocalFrameClient>(this);
    frame = MakeGarbageCollected<LocalFrame>(
        frame_client_, *page, nullptr, nullptr, nullptr,
        FrameInsertType::kInsertInConstructor, base::UnguessableToken::Create(),
        nullptr, nullptr);
    frame->SetView(MakeGarbageCollected<LocalFrameView>(*frame));
    frame->Init(nullptr);
  }

  FrameLoader& loader = frame->Loader();
  loader.ForceSandboxFlags(network::mojom::blink::WebSandboxFlags::kAll);

  // SVG Images will always synthesize a viewBox, if it's not available, and
  // thus never see scrollbars.
  frame->View()->SetCanHaveScrollbars(false);
  // SVG Images are transparent.
  frame->View()->SetBaseBackgroundColor(Color::kTransparent);

  page_ = page;

  TRACE_EVENT0("blink", "SVGImage::dataChanged::load");

  frame->ForceSynchronousDocumentInstall("image/svg+xml", Data());

  // Intrinsic sizing relies on computed style (e.g. font-size and
  // writing-mode).
  frame->GetDocument()->UpdateStyleAndLayoutTree();

  // Set the concrete object size before a container size is available.
  intrinsic_size_ = RoundedLayoutSize(ConcreteObjectSize(FloatSize(
      LayoutReplaced::kDefaultWidth, LayoutReplaced::kDefaultHeight)));

  DCHECK(page_);
  switch (load_state_) {
    case kInDataChanged:
      load_state_ = kWaitingForAsyncLoadCompletion;
      return SvgRootElement(page_.Get())
                 ? kSizeAvailableAndLoadingAsynchronously
                 : kSizeUnavailable;

    case kLoadCompleted:
      return SvgRootElement(page_.Get()) ? kSizeAvailable : kSizeUnavailable;

    case kDataChangedNotStarted:
    case kWaitingForAsyncLoadCompletion:
      CHECK(false);
      break;
  }

  NOTREACHED();
  return kSizeAvailable;
}

bool SVGImage::IsSizeAvailable() {
  return SvgRootElement(page_.Get());
}

String SVGImage::FilenameExtension() const {
  return "svg";
}

}  // namespace blink
