/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/paint/link_highlight_impl.h"

#include <memory>
#include <utility>

#include "base/debug/stack_trace.h"
#include "base/memory/ptr_util.h"
#include "cc/layers/picture_layer.h"
#include "cc/paint/display_item_list.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_float_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_keyframe_model.h"
#include "third_party/blink/renderer/platform/animation/compositor_target_property.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_display_item_fragment.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

static constexpr float kStartOpacity = 1;

namespace {

EffectPaintPropertyNode::State LinkHighlightEffectNodeState(
    float opacity,
    CompositorElementId element_id) {
  EffectPaintPropertyNode::State state;
  state.opacity = opacity;
  state.local_transform_space = &TransformPaintPropertyNode::Root();
  state.compositor_element_id = element_id;
  state.direct_compositing_reasons = CompositingReason::kActiveOpacityAnimation;
  // EffectPaintPropertyNode::Update does not pay attention to changes in
  // has_active_opacity_animation so we assume that the effect node is
  // always animating.
  state.has_active_opacity_animation = true;
  return state;
}

}  // namespace

static CompositorElementId NewElementId() {
  return CompositorElementIdFromUniqueObjectId(
      NewUniqueObjectId(), CompositorElementIdNamespace::kPrimaryEffect);
}

LinkHighlightImpl::LinkHighlightImpl(Node* node)
    : node_(node),
      is_animating_(false),
      start_time_(base::TimeTicks::Now()),
      element_id_(NewElementId()) {
  DCHECK(node_);
  fragments_.emplace_back();

  compositor_animation_ = CompositorAnimation::Create();
  DCHECK(compositor_animation_);
  compositor_animation_->SetAnimationDelegate(this);
  compositor_animation_->AttachElement(element_id_);

  effect_ = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(),
      LinkHighlightEffectNodeState(kStartOpacity, element_id_));

  DCHECK(GetLayoutObject());
  GetLayoutObject()->SetNeedsPaintPropertyUpdate();
  SetPaintArtifactCompositorNeedsUpdate();

#if DCHECK_IS_ON()
  effect_->SetDebugName("LinkHighlightEffect");
#endif
}

LinkHighlightImpl::~LinkHighlightImpl() {
  if (compositor_animation_->IsElementAttached())
    compositor_animation_->DetachElement();
  compositor_animation_->SetAnimationDelegate(nullptr);
  compositor_animation_.reset();

  ReleaseResources();
}

void LinkHighlightImpl::ReleaseResources() {
  if (!node_)
    return;

  if (auto* layout_object = GetLayoutObject())
    layout_object->SetNeedsPaintPropertyUpdate();

  SetPaintArtifactCompositorNeedsUpdate();

  node_.Clear();
}

LinkHighlightImpl::LinkHighlightFragment::LinkHighlightFragment() {
  layer_ = cc::PictureLayer::Create(this);
  layer_->SetIsDrawable(true);
  layer_->SetOpacity(kStartOpacity);
}

LinkHighlightImpl::LinkHighlightFragment::~LinkHighlightFragment() {
  layer_->ClearClient();
}

gfx::Rect LinkHighlightImpl::LinkHighlightFragment::PaintableRegion() {
  return gfx::Rect(layer_->bounds());
}

scoped_refptr<cc::DisplayItemList>
LinkHighlightImpl::LinkHighlightFragment::PaintContentsToDisplayList() {
  auto display_list = base::MakeRefCounted<cc::DisplayItemList>();

  PaintRecorder recorder;
  gfx::Rect record_bounds = PaintableRegion();
  cc::PaintCanvas* canvas =
      recorder.beginRecording(record_bounds.width(), record_bounds.height());

  PaintFlags flags;
  flags.setStyle(PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(color_.Rgb());
  canvas->drawPath(path_.GetSkPath(), flags);

  display_list->StartPaint();
  display_list->push<cc::DrawRecordOp>(recorder.finishRecordingAsPicture());
  display_list->EndPaintOfUnpaired(record_bounds);

  display_list->Finalize();
  return display_list;
}

void LinkHighlightImpl::StartHighlightAnimationIfNeeded() {
  if (is_animating_)
    return;

  is_animating_ = true;
  // FIXME: Should duration be configurable?
  constexpr auto kFadeDuration = base::TimeDelta::FromMilliseconds(100);
  constexpr auto kMinPreFadeDuration = base::TimeDelta::FromMilliseconds(100);

  auto curve = std::make_unique<CompositorFloatAnimationCurve>();

  const auto& timing_function = *CubicBezierTimingFunction::Preset(
      CubicBezierTimingFunction::EaseType::EASE);

  float target_opacity = WebTestSupport::IsRunningWebTest() ? kStartOpacity : 0;

  // Since the notification about the animation finishing may not arrive in
  // time to remove the link highlight before it's drawn without an animation
  // we set the opacity to the final target opacity to avoid a flash of the
  // initial opacity. https://crbug.com/974160
  UpdateOpacity(target_opacity);

  curve->AddKeyframe(
      CompositorFloatKeyframe(0, kStartOpacity, timing_function));
  // Make sure we have displayed for at least minPreFadeDuration before starting
  // to fade out.
  base::TimeDelta extra_duration_required =
      std::max(base::TimeDelta(),
               kMinPreFadeDuration - (base::TimeTicks::Now() - start_time_));
  if (!extra_duration_required.is_zero()) {
    curve->AddKeyframe(CompositorFloatKeyframe(
        extra_duration_required.InSecondsF(), kStartOpacity, timing_function));
  }
  curve->AddKeyframe(CompositorFloatKeyframe(
      (kFadeDuration + extra_duration_required).InSecondsF(), target_opacity,
      timing_function));

  auto keyframe_model = std::make_unique<CompositorKeyframeModel>(
      *curve, compositor_target_property::OPACITY, 0, 0);

  compositor_animation_->AddKeyframeModel(std::move(keyframe_model));
}

void LinkHighlightImpl::NotifyAnimationFinished(double, int) {
  // Since WebViewImpl may hang on to us for a while, make sure we
  // release resources as soon as possible.
  ReleaseResources();

  // Reset the link highlight opacity to clean up after the animation now that
  // we have removed the node and it won't be displayed.
  UpdateOpacity(kStartOpacity);
}

void LinkHighlightImpl::UpdateBeforePrePaint() {
  auto* object = GetLayoutObject();
  if (!object || object->GetFrameView()->ShouldThrottleRendering())
    ReleaseResources();
}

void LinkHighlightImpl::UpdateAfterPrePaint() {
  auto* object = GetLayoutObject();
  if (!object)
    return;
  DCHECK(!object->GetFrameView()->ShouldThrottleRendering());

  size_t fragment_count = 0;
  for (const auto* fragment = &object->FirstFragment(); fragment;
       fragment = fragment->NextFragment())
    ++fragment_count;

  if (fragment_count != fragments_.size()) {
    fragments_.resize(fragment_count);
    SetPaintArtifactCompositorNeedsUpdate();
  }
}

CompositorAnimation* LinkHighlightImpl::GetCompositorAnimation() const {
  return compositor_animation_.get();
}

void LinkHighlightImpl::Paint(GraphicsContext& context) {
  auto* object = GetLayoutObject();
  if (!object)
    return;

  DCHECK(object->GetFrameView());
  DCHECK(!object->GetFrameView()->ShouldThrottleRendering());

  static const FloatSize rect_rounding_radii(3, 3);
  auto color = object->StyleRef().VisitedDependentColor(
      GetCSSPropertyWebkitTapHighlightColor());

  // For now, we'll only use rounded rects if we have a single rect because
  // otherwise we may sometimes get a chain of adjacent boxes (e.g. for text
  // nodes) which end up looking like sausage links: these should ideally be
  // merged into a single rect before creating the path.
  bool use_rounded_rects = !node_->GetDocument()
                                .GetSettings()
                                ->GetMockGestureTapHighlightsEnabled() &&
                           !object->FirstFragment().NextFragment();

  wtf_size_t index = 0;
  for (const auto* fragment = &object->FirstFragment(); fragment;
       fragment = fragment->NextFragment(), ++index) {
    ScopedDisplayItemFragment scoped_fragment(context, index);
    auto rects = object->OutlineRects(
        fragment->PaintOffset(), NGOutlineType::kIncludeBlockVisualOverflow);
    if (rects.size() > 1)
      use_rounded_rects = false;

    // TODO(yosin): We should remove following if-statement once we release
    // NGFragmentItem to renderer rounded rect even if nested inline, e.g.
    // <a>ABC<b>DEF</b>GHI</a>.
    // See gesture-tapHighlight-simple-nested.html
    if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled() &&
        use_rounded_rects && object->IsLayoutInline() &&
        object->IsInLayoutNGInlineFormattingContext()) {
      NGInlineCursor cursor;
      cursor.MoveTo(*object);
      // When |LayoutInline| has more than one children, we render square
      // rectangle as |NGPaintFragment|.
      if (cursor && cursor.CurrentItem()->DescendantsCount() > 2)
        use_rounded_rects = false;
    }

    Path new_path;
    for (auto& rect : rects) {
      FloatRect snapped_rect(PixelSnappedIntRect(rect));
      if (use_rounded_rects)
        new_path.AddRoundedRect(snapped_rect, rect_rounding_radii);
      else
        new_path.AddRect(snapped_rect);
    }

    DCHECK_LT(index, fragments_.size());
    auto& link_highlight_fragment = fragments_[index];
    link_highlight_fragment.SetColor(color);

    auto bounding_rect = EnclosingIntRect(new_path.BoundingRect());
    new_path.Translate(-FloatSize(ToIntSize(bounding_rect.Location())));

    cc::Layer* layer = link_highlight_fragment.Layer();
    DCHECK(layer);
    if (link_highlight_fragment.GetPath() != new_path) {
      link_highlight_fragment.SetPath(new_path);
      layer->SetBounds(gfx::Size(bounding_rect.Size()));
      layer->SetNeedsDisplay();
    }

    DEFINE_STATIC_LOCAL(LiteralDebugNameClient, debug_name_client,
                        ("LinkHighlight"));

    auto property_tree_state = fragment->LocalBorderBoxProperties().Unalias();
    property_tree_state.SetEffect(Effect());
    RecordForeignLayer(context, debug_name_client,
                       DisplayItem::kForeignLayerLinkHighlight, layer,
                       bounding_rect.Location(), &property_tree_state);
  }

  DCHECK_EQ(index, fragments_.size());
}

void LinkHighlightImpl::SetPaintArtifactCompositorNeedsUpdate() {
  DCHECK(node_);
  if (auto* frame_view = node_->GetDocument().View())
    frame_view->SetPaintArtifactCompositorNeedsUpdate();
}

void LinkHighlightImpl::UpdateOpacity(float opacity) {
  effect_->Update(EffectPaintPropertyNode::Root(),
                  LinkHighlightEffectNodeState(opacity, element_id_));
}

}  // namespace blink
