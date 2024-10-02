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
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/svg/animation/smil_time_container.h"
#include "third_party/blink/renderer/core/svg/graphics/isolated_svg_document_host.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_chrome_client.h"
#include "third_party/blink/renderer/core/svg/svg_animated_preserve_aspect_ratio.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "third_party/blink/renderer/core/svg/svg_fe_image_element.h"
#include "third_party/blink/renderer/core/svg/svg_foreign_object_element.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_view_spec.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/image_observer.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_shader.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

namespace {

bool HasSmilAnimations(const Document& document) {
  const SVGDocumentExtensions* extensions = document.SvgExtensions();
  return extensions && extensions->HasSmilAnimations();
}

}  // namespace

void SVGImageViewInfo::Trace(Visitor* visitor) const {
  visitor->Trace(view_spec_);
  visitor->Trace(target_);
}

SVGImage::SVGImage(ImageObserver* observer, bool is_multipart)
    : Image(observer, is_multipart),
      has_pending_timeline_rewind_(false) {}

SVGImage::~SVGImage() {
  if (document_host_) {
    // Store `document_host_` in a local variable and clear it so that
    // SVGImageChromeClient knows we're destructed.
    auto* document_host = document_host_.Release();
    document_host->Shutdown();
  }

  // Verify that page teardown destroyed the Chrome
  DCHECK(!chrome_client_ || !chrome_client_->GetImage());
}

bool SVGImage::IsInSVGImage(const Node* node) {
  DCHECK(node);

  Page* page = node->GetDocument().GetPage();
  if (!page)
    return false;

  return page->GetChromeClient().IsIsolatedSVGChromeClient();
}

LocalFrame* SVGImage::GetFrame() const {
  DCHECK(document_host_);
  return document_host_->GetFrame();
}

SVGSVGElement* SVGImage::RootElement() const {
  if (!document_host_) {
    return nullptr;
  }
  return document_host_->RootElement();
}

LayoutSVGRoot* SVGImage::LayoutRoot() const {
  if (SVGSVGElement* root_element = RootElement())
    return To<LayoutSVGRoot>(root_element->GetLayoutObject());
  return nullptr;
}

Page* SVGImage::GetPageForTesting() {
  return GetFrame()->GetPage();
}

void SVGImage::CheckLoaded() const {
  CHECK(document_host_);
  // Failures of this assertion might result in wrong origin tainting checks,
  // because CurrentFrameHasSingleSecurityOrigin() assumes all subresources of
  // the SVG are loaded and thus ready for origin checks.
  CHECK(GetFrame()->GetDocument()->LoadEventFinished());
}

bool SVGImage::CurrentFrameHasSingleSecurityOrigin() const {
  if (!document_host_) {
    return true;
  }

  SVGSVGElement* root_element = RootElement();
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

gfx::Size SVGImage::SizeWithConfig(SizeConfig) const {
  return ToRoundedSize(intrinsic_size_);
}

const SVGImageViewInfo* SVGImage::CreateViewInfo(const String& fragment) const {
  if (fragment.empty()) {
    return nullptr;
  }
  const SVGSVGElement* root_element = RootElement();
  if (!root_element) {
    return nullptr;
  }
  String decoded_fragment =
      DecodeURLEscapeSequences(fragment, DecodeURLMode::kUTF8);
  Element* target = DynamicTo<Element>(
      root_element->GetDocument().FindAnchor(decoded_fragment));
  const SVGViewSpec* view_spec =
      root_element->ParseViewSpec(decoded_fragment, target);
  if (!view_spec && !target) {
    return nullptr;
  }
  return MakeGarbageCollected<SVGImageViewInfo>(view_spec, target);
}

void SVGImage::ApplyViewInfo(const SVGImageViewInfo* viewinfo) {
  SVGSVGElement* root_element = RootElement();
  if (!root_element) {
    return;
  }
  Element* target = viewinfo ? viewinfo->Target() : nullptr;
  root_element->GetDocument().SetCSSTarget(target);
  const SVGViewSpec* viewspec = viewinfo ? viewinfo->ViewSpec() : nullptr;
  root_element->SetViewSpec(viewspec);
}

bool SVGImage::GetIntrinsicSizingInfo(
    const SVGViewSpec* override_viewspec,
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  const LayoutSVGRoot* layout_root = LayoutRoot();
  if (!layout_root)
    return false;
  layout_root->UnscaledIntrinsicSizingInfo(
      override_viewspec ? override_viewspec->ViewBox() : nullptr,
      intrinsic_sizing_info);

  if (!intrinsic_sizing_info.has_width || !intrinsic_sizing_info.has_height) {
    // We're not using an intrinsic aspect ratio to resolve a missing
    // intrinsic width or height when preserveAspectRatio is none.
    // (Ref: crbug.com/584172)
    SVGSVGElement* svg = RootElement();
    if (svg->preserveAspectRatio()->CurrentValue()->Align() ==
        SVGPreserveAspectRatio::kSvgPreserveaspectratioNone) {
      // Clear all the fields so that the concrete object size will equal the
      // default object size.
      intrinsic_sizing_info = IntrinsicSizingInfo();
      intrinsic_sizing_info.has_width = false;
      intrinsic_sizing_info.has_height = false;
    }
  }
  return true;
}

SVGImage::DrawInfo::DrawInfo(const gfx::SizeF& container_size,
                             float zoom,
                             const SVGImageViewInfo* viewinfo,
                             bool is_dark_mode_enabled)
    : container_size_(container_size),
      rounded_container_size_(gfx::ToRoundedSize(container_size)),
      zoom_(zoom),
      viewinfo_(viewinfo),
      is_dark_mode_enabled_(is_dark_mode_enabled) {}

gfx::SizeF SVGImage::DrawInfo::CalculateResidualScale() const {
  return gfx::SizeF(
      rounded_container_size_.width() / container_size_.width(),
      rounded_container_size_.height() / container_size_.height());
}

void SVGImage::DrawForContainer(const DrawInfo& draw_info,
                                cc::PaintCanvas* canvas,
                                const cc::PaintFlags& flags,
                                const gfx::RectF& dst_rect,
                                const gfx::RectF& src_rect) {
  gfx::RectF unzoomed_src = src_rect;
  unzoomed_src.InvScale(draw_info.Zoom());

  // Compensate for the container size rounding by adjusting the source rect.
  gfx::SizeF residual_scale = draw_info.CalculateResidualScale();
  unzoomed_src.set_size(gfx::ScaleSize(
      unzoomed_src.size(), residual_scale.width(), residual_scale.height()));

  DrawInternal(draw_info, canvas, flags, dst_rect, unzoomed_src);
}

PaintImage SVGImage::PaintImageForCurrentFrame() {
  const DrawInfo draw_info(gfx::SizeF(intrinsic_size_), 1, nullptr, false);
  auto builder = CreatePaintImageBuilder();
  PopulatePaintRecordForCurrentFrameForContainer(draw_info, builder);
  return builder.TakePaintImage();
}

void SVGImage::SetPreferredColorScheme(
    mojom::blink::PreferredColorScheme preferred_color_scheme) {
  if (document_host_) {
    GetFrame()->GetPage()->GetSettings().SetPreferredColorScheme(
        preferred_color_scheme);
  }
}

void SVGImage::DrawPatternForContainer(const DrawInfo& draw_info,
                                       GraphicsContext& context,
                                       const cc::PaintFlags& base_flags,
                                       const gfx::RectF& dst_rect,
                                       const ImageTilingInfo& tiling_info) {
  // Tile adjusted for scaling/stretch.
  gfx::RectF tile = tiling_info.image_rect;
  tile.Scale(tiling_info.scale.x(), tiling_info.scale.y());

  // Expand the tile to account for repeat spacing.
  gfx::RectF spaced_tile(tile.origin(), tile.size() + tiling_info.spacing);

  SkMatrix pattern_transform;
  pattern_transform.setTranslate(tiling_info.phase.x() + spaced_tile.x(),
                                 tiling_info.phase.y() + spaced_tile.y());

  PaintRecorder recorder;
  cc::PaintCanvas* tile_canvas = recorder.beginRecording();
  // When generating an expanded tile, make sure we don't draw into the
  // spacing area.
  if (!tiling_info.spacing.IsZero()) {
    tile_canvas->clipRect(gfx::RectFToSkRect(tile));
  }
  DrawForContainer(draw_info, tile_canvas, cc::PaintFlags(), tile,
                   tiling_info.image_rect);
  sk_sp<PaintShader> tile_shader = PaintShader::MakePaintRecord(
      recorder.finishRecordingAsPicture(), gfx::RectFToSkRect(spaced_tile),
      SkTileMode::kRepeat, SkTileMode::kRepeat, &pattern_transform);

  // If the shader could not be instantiated (e.g. non-invertible matrix),
  // draw transparent.
  // Note: we can't simply bail, because of arbitrary blend mode.
  cc::PaintFlags flags = base_flags;
  flags.setColor(tile_shader ? SK_ColorBLACK : SK_ColorTRANSPARENT);
  flags.setShader(std::move(tile_shader));
  // Reset filter quality.
  flags.setFilterQuality(cc::PaintFlags::FilterQuality::kNone);

  context.DrawRect(gfx::RectFToSkRect(dst_rect), flags,
                   PaintAutoDarkMode(DarkModeFilter::ElementRole::kSVG,
                                     draw_info.IsDarkModeEnabled()));

  StartAnimation();
}

void SVGImage::PopulatePaintRecordForCurrentFrameForContainer(
    const DrawInfo& draw_info,
    PaintImageBuilder& builder) {
  PaintRecorder recorder;
  const gfx::SizeF size =
      gfx::ScaleSize(draw_info.ContainerSize(), draw_info.Zoom());
  const gfx::Rect dest_rect(gfx::ToRoundedSize(size));
  cc::PaintCanvas* canvas = recorder.beginRecording();
  DrawForContainer(draw_info, canvas, cc::PaintFlags(), gfx::RectF(dest_rect),
                   gfx::RectF(size));
  builder.set_paint_record(recorder.finishRecordingAsPicture(), dest_rect,
                           PaintImage::GetNextContentId());

  builder.set_completion_state(
      document_host_ && document_host_->IsLoaded()
          ? PaintImage::CompletionState::kDone
          : PaintImage::CompletionState::kPartiallyDone);
}

bool SVGImage::ApplyShaderInternal(const DrawInfo& draw_info,
                                   cc::PaintFlags& flags,
                                   const gfx::RectF& unzoomed_src_rect,
                                   const SkMatrix& local_matrix) {
  if (draw_info.ContainerSize().IsEmpty())
    return false;
  const gfx::Rect cull_rect(gfx::ToEnclosingRect(unzoomed_src_rect));
  std::optional<PaintRecord> record =
      PaintRecordForCurrentFrame(draw_info, &cull_rect);
  if (!record)
    return false;

  const SkRect bounds =
      SkRect::MakeSize(gfx::SizeFToSkSize(draw_info.ContainerSize()));
  flags.setShader(PaintShader::MakePaintRecord(
      std::move(*record), bounds, SkTileMode::kClamp, SkTileMode::kClamp,
      &local_matrix));

  // Animation is normally refreshed in Draw() impls, which we don't reach when
  // painting via shaders.
  StartAnimation();
  return true;
}

bool SVGImage::ApplyShader(cc::PaintFlags& flags,
                           const SkMatrix& local_matrix,
                           const gfx::RectF& src_rect,
                           const ImageDrawOptions& draw_options) {
  const DrawInfo draw_info(gfx::SizeF(intrinsic_size_), 1, nullptr,
                           draw_options.apply_dark_mode);
  return ApplyShaderInternal(draw_info, flags, src_rect, local_matrix);
}

bool SVGImage::ApplyShaderForContainer(const DrawInfo& draw_info,
                                       cc::PaintFlags& flags,
                                       const gfx::RectF& src_rect,
                                       const SkMatrix& local_matrix) {
  gfx::RectF unzoomed_src = src_rect;
  unzoomed_src.InvScale(draw_info.Zoom());

  // Compensate for the container size rounding by adjusting the source rect.
  const gfx::SizeF residual_scale = draw_info.CalculateResidualScale();
  unzoomed_src.set_size(gfx::ScaleSize(
      unzoomed_src.size(), residual_scale.width(), residual_scale.height()));

  // Compensate for the container size rounding.
  const gfx::SizeF zoomed_residual_scale =
      gfx::ScaleSize(residual_scale, draw_info.Zoom());
  auto adjusted_local_matrix = local_matrix;
  adjusted_local_matrix.preScale(zoomed_residual_scale.width(),
                                 zoomed_residual_scale.height());
  return ApplyShaderInternal(draw_info, flags, unzoomed_src,
                             adjusted_local_matrix);
}

void SVGImage::Draw(cc::PaintCanvas* canvas,
                    const cc::PaintFlags& flags,
                    const gfx::RectF& dst_rect,
                    const gfx::RectF& src_rect,
                    const ImageDrawOptions& draw_options) {
  const DrawInfo draw_info(gfx::SizeF(intrinsic_size_), 1, nullptr,
                           draw_options.apply_dark_mode);
  DrawInternal(draw_info, canvas, flags, dst_rect, src_rect);
}

std::optional<PaintRecord> SVGImage::PaintRecordForCurrentFrame(
    const DrawInfo& draw_info,
    const gfx::Rect* cull_rect) {
  if (!document_host_) {
    return std::nullopt;
  }
  // Temporarily disable the image observer to prevent ChangeInRect() calls due
  // re-laying out the image.
  ImageObserverDisabler disable_image_observer(this);

  if (LayoutSVGRoot* layout_root = LayoutRoot()) {
    layout_root->SetContainerSize(
        PhysicalSize::FromSizeFFloor(draw_info.ContainerSize()));
  }
  LocalFrame* frame = GetFrame();
  LocalFrameView* view = frame->View();
  const gfx::Size rounded_container_size = draw_info.RoundedContainerSize();
  view->Resize(rounded_container_size);
  frame->GetPage()->GetVisualViewport().SetSize(rounded_container_size);

  // Always call ApplyViewInfo, even if there's no view specification, because
  // there may have been a previous view info that needs to be reset.
  ApplyViewInfo(draw_info.View());

  // If the image was reset, we need to rewind the timeline back to 0. This
  // needs to be done before painting, or else we wouldn't get the correct
  // reset semantics (we'd paint the "last" frame rather than the one at
  // time=0.) The reason we do this here and not in resetAnimation() is to
  // avoid setting timers from the latter.
  FlushPendingTimelineRewind();

  frame->GetPage()->GetSettings().SetForceDarkModeEnabled(
      draw_info.IsDarkModeEnabled());

  view->UpdateAllLifecyclePhases(DocumentUpdateReason::kSVGImage);

  return view->GetPaintRecord(cull_rect);
}

static bool DrawNeedsLayer(const cc::PaintFlags& flags) {
  if (SkColorGetA(flags.getColor()) < 255)
    return true;

  // This is needed to preserve the dark mode filter that
  // has been set in GraphicsContext.
  if (flags.getColorFilter())
    return true;

  return flags.getBlendMode() != SkBlendMode::kSrcOver;
}

void SVGImage::DrawInternal(const DrawInfo& draw_info,
                            cc::PaintCanvas* canvas,
                            const cc::PaintFlags& flags,
                            const gfx::RectF& dst_rect,
                            const gfx::RectF& unzoomed_src_rect) {
  const gfx::Rect cull_rect(gfx::ToEnclosingRect(unzoomed_src_rect));
  std::optional<PaintRecord> record =
      PaintRecordForCurrentFrame(draw_info, &cull_rect);
  if (!record)
    return;

  {
    PaintCanvasAutoRestore ar(canvas, false);
    if (DrawNeedsLayer(flags)) {
      SkRect layer_rect = gfx::RectFToSkRect(dst_rect);
      canvas->saveLayer(layer_rect, flags);
    }
    // We can only draw the entire frame, clipped to the rect we want. So
    // compute where the top left of the image would be if we were drawing
    // without clipping, and translate accordingly.
    canvas->save();
    canvas->clipRect(gfx::RectToSkRect(gfx::ToEnclosingRect(dst_rect)));
    canvas->concat(SkM44::RectToRect(gfx::RectFToSkRect(unzoomed_src_rect),
                                     gfx::RectFToSkRect(dst_rect)));
    canvas->drawPicture(std::move(*record));
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
  if (SVGSVGElement* root_element = RootElement())
    root_element->setCurrentTime(0);
  has_pending_timeline_rewind_ = false;
}

void SVGImage::StartAnimation() {
  SVGSVGElement* root_element = RootElement();
  if (!root_element)
    return;
  chrome_client_->ResumeAnimation();
  if (root_element->animationsPaused())
    root_element->unpauseAnimations();
}

void SVGImage::StopAnimation() {
  SVGSVGElement* root_element = RootElement();
  if (!root_element)
    return;
  chrome_client_->SuspendAnimation();
  root_element->pauseAnimations();
}

void SVGImage::ResetAnimation() {
  SVGSVGElement* root_element = RootElement();
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
  SVGSVGElement* root_element = RootElement();
  if (!root_element)
    return false;
  const Document& document = root_element->GetDocument();
  return HasSmilAnimations(document) || document.Timeline().HasPendingUpdates();
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

  LocalFrame* frame = GetFrame();

  // The calls below may trigger GCs, so set up the required persistent
  // reference on the ImageResourceContent which owns this SVGImage. By
  // transitivity, that will keep the associated SVGImageChromeClient object
  // alive.
  Persistent<ImageObserver> protect(GetImageObserver());
  frame->GetPage()->Animator().ServiceScriptedAnimations(
      monotonic_animation_start_time);

  // Do *not* update the paint phase. It's critical to paint only when
  // actually generating painted output, not only for performance reasons,
  // but to preserve correct coherence of the cache of the output with
  // the needsRepaint bits of the PaintLayers in the image.
  LocalFrameView* frame_view = frame->View();
  frame_view->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kSVGImage);

  // We run UpdateAnimations after the paint phase, but per the above comment,
  // we don't want to run lifecycle through to paint for SVG images. Since we
  // know SVG images never have composited animations, we can update animations
  // directly without worrying about including PaintArtifactCompositor's
  // analysis of whether animations should be composited.
  frame->GetDocument()->GetDocumentAnimations().UpdateAnimations(
      DocumentLifecycle::kLayoutClean, nullptr, false);
}

void SVGImage::AdvanceAnimationForTesting() {
  if (SVGSVGElement* root_element = RootElement()) {
    root_element->TimeContainer()->AdvanceFrameForTesting();

    // The following triggers animation updates which can issue a new draw
    // and temporarily change the animation timeline. It's necessary to call
    // reset before changing to a time value as animation clock does not
    // expect to go backwards.
    PageAnimator& animator = root_element->GetDocument().GetPage()->Animator();
    base::TimeTicks current_animation_time = animator.Clock().CurrentTime();
    animator.Clock().ResetTimeForTesting();
    if (root_element->TimeContainer()->IsStarted())
      root_element->TimeContainer()->ResetDocumentTime();
    animator.ServiceScriptedAnimations(
        root_element->GetDocument().Timeline().CalculateZeroTime() +
        base::Seconds(root_element->getCurrentTime()));
    GetImageObserver()->Changed(this);
    animator.Clock().ResetTimeForTesting();
    animator.Clock().UpdateTime(current_animation_time);
  }
}

SVGImageChromeClient& SVGImage::ChromeClientForTesting() {
  return *chrome_client_;
}

void SVGImage::UpdateUseCounters(const Document& document) const {
  if (SVGSVGElement* root_element = RootElement()) {
    if (HasSmilAnimations(root_element->GetDocument())) {
      document.CountUse(WebFeature::kSVGSMILAnimationInImageRegardlessOfCache);
    }
  }
}

void SVGImage::MaybeRecordSvgImageProcessingTime(const Document& document) {
  if (data_change_count_ > 0) {
    document.MaybeRecordSvgImageProcessingTime(data_change_count_,
                                               data_change_elapsed_time_);
    data_change_count_ = 0;
    data_change_elapsed_time_ = base::TimeDelta();
  }
}

Element* SVGImage::GetResourceElement(const AtomicString& id) const {
  if (!document_host_) {
    return nullptr;
  }
  return GetFrame()->GetDocument()->getElementById(id);
}

void SVGImage::NotifyAsyncLoadCompleted() {
  if (GetImageObserver())
    GetImageObserver()->AsyncLoadCompleted(this);
}

Image::SizeAvailability SVGImage::DataChanged(bool all_data_received) {
  TRACE_EVENT("blink", "SVGImage::DataChanged");

  // Don't do anything if is an empty image.
  if (!DataSize())
    return kSizeAvailable;

  if (!all_data_received)
    return document_host_ ? kSizeAvailable : kSizeUnavailable;

  SCOPED_BLINK_UMA_HISTOGRAM_TIMER_HIGHRES("Blink.SVGImage.DataChanged");
  base::ElapsedTimer elapsed_timer;

  // Because an SVGImage has no relation to a normal Page, it can't get default
  // font settings from the embedder. Copy settings for fonts and other things
  // so we have sensible defaults. These settings are fixed and will not update
  // if changed.
  const auto& pages = Page::OrdinaryPages();
  const Settings* settings_to_use =
      !pages.empty() ? &(*pages.begin())->GetSettings() : nullptr;

  // FIXME: If this SVG ends up loading itself, we might leak the world.
  // The Cache code does not know about ImageResources holding Frames and
  // won't know to break the cycle.
  // This will become an issue when SVGImage will be able to load other
  // SVGImage objects, but we're safe now, because SVGImage can only be
  // loaded by a top-level document.
  CHECK(!document_host_);
  std::tie(chrome_client_, document_host_) =
      IsolatedSVGDocumentHostInitializer::Get()->GetOrCreate();
  chrome_client_->SetImage(this);
  document_host_->InstallDocument(
      Data(),
      WTF::BindOnce(&SVGImage::NotifyAsyncLoadCompleted,
                    weak_ptr_factory_.GetWeakPtr()),
      settings_to_use, IsolatedSVGDocumentHost::ProcessingMode::kAnimated);

  if (!RootElement())
    return kSizeUnavailable;

  // Set the concrete object size before a container size is available.
  // TODO(fs): Make this just set/copy width and height directly. See
  // crbug.com/789511.
  IntrinsicSizingInfo sizing_info;
  if (GetIntrinsicSizingInfo(nullptr, sizing_info)) {
    intrinsic_size_ = PhysicalSize::FromSizeFFloor(blink::ConcreteObjectSize(
        sizing_info, gfx::SizeF(LayoutReplaced::kDefaultWidth,
                                LayoutReplaced::kDefaultHeight)));
  }

  ++data_change_count_;
  data_change_elapsed_time_ += elapsed_timer.Elapsed();

  if (!document_host_->IsLoaded()) {
    return kSizeAvailableAndLoadingAsynchronously;
  }
  return kSizeAvailable;
}

bool SVGImage::IsSizeAvailable() {
  return RootElement();
}

String SVGImage::FilenameExtension() const {
  return "svg";
}

const AtomicString& SVGImage::MimeType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, svg_mime_type, ("image/svg+xml"));
  return svg_mime_type;
}

}  // namespace blink
