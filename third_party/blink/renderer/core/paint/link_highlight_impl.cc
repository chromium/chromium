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
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/keyframe_model.h"
#include "cc/layers/picture_layer.h"
#include "cc/paint/display_item_list.h"
#include "cc/trees/target_property.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/fragment_data_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_display_item_fragment.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

static constexpr float kStartOpacity = 1;

namespace {

float GetTargetOpacity() {
  return WebTestSupport::IsRunningWebTest() ? kStartOpacity : 0;
}

EffectPaintPropertyNode::State LinkHighlightEffectNodeState(
    float opacity,
    CompositorElementId element_id) {
  EffectPaintPropertyNode::State state;
  state.opacity = opacity;
  state.local_transform_space = &TransformPaintPropertyNode::Root();
  state.compositor_element_id = element_id;
  // EffectPaintPropertyNode::Update does not pay attention to changes in
  // direct_compositing_reasons so we assume that the effect node is always
  // animating.
  state.direct_compositing_reasons = CompositingReason::kActiveOpacityAnimation;
  return state;
}

}  // namespace

static CompositorElementId NewElementId() {
  return CompositorElementIdFromUniqueObjectId(
      NewUniqueObjectId(), CompositorElementIdNamespace::kPrimaryEffect);
}

LinkHighlightImpl::LinkHighlightImpl(Node* node)
    : node_(node),
      start_time_(base::TimeTicks::Now()),
      element_id_(NewElementId()) {
  DCHECK(node_);
  fragments_.push_back(std::make_unique<LinkHighlightFragment>());

  compositor_animation_ = CompositorAnimation::Create();
  DCHECK(compositor_animation_);
  compositor_animation_->SetAnimationDelegate(this);
  compositor_animation_->AttachElement(element_id_);

  effect_ = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(),
      LinkHighlightEffectNodeState(kStartOpacity, element_id_));

  DCHECK(GetLayoutObject());
  GetLayoutObject()->SetNeedsPaintPropertyUpdate();
  SetNeedsRepaintAndCompositingUpdate();

#if DCHECK_IS_ON()
  effect_->SetDebugName("LinkHighlightEffect");
#endif
}

LinkHighlightImpl::~LinkHighlightImpl() {
  ReleaseResources();

  if (compositor_animation_->IsElementAttached())
    compositor_animation_->DetachElement();
  compositor_animation_->SetAnimationDelegate(nullptr);
  compositor_animation_.reset();
}

void LinkHighlightImpl::ReleaseResources() {
  StopCompositorAnimation();

  if (!node_)
    return;

  if (auto* layout_object = GetLayoutObject())
    layout_object->SetNeedsPaintPropertyUpdate();

  SetNeedsRepaintAndCompositingUpdate();

  node_.Clear();
}

void LinkHighlightImpl::StartCompositorAnimation() {
  is_animating_on_compositor_ = true;
  // FIXME: Should duration be configurable?
  constexpr auto kFadeDuration = base::Milliseconds(100);
  constexpr auto kMinPreFadeDuration = base::Milliseconds(100);

  auto curve = gfx::KeyframedFloatAnimationCurve::Create();

  const auto& timing_function = *CubicBezierTimingFunction::Preset(
      CubicBezierTimingFunction::EaseType::EASE);

  curve->AddKeyframe(gfx::FloatKeyframe::Create(base::Seconds(0), kStartOpacity,
                                                timing_function.CloneToCC()));
  // Make sure we have displayed for at least minPreFadeDuration before starting
  // to fade out.
  base::TimeDelta extra_duration_required =
      std::max(base::TimeDelta(),
               kMinPreFadeDuration - (base::TimeTicks::Now() - start_time_));
  if (!extra_duration_required.is_zero()) {
    curve->AddKeyframe(gfx::FloatKeyframe::Create(
        extra_duration_required, kStartOpacity, timing_function.CloneToCC()));
  }
  curve->AddKeyframe(gfx::FloatKeyframe::Create(
      kFadeDuration + extra_duration_required, GetTargetOpacity(),
      timing_function.CloneToCC()));

  auto keyframe_model = cc::KeyframeModel::Create(
      std::move(curve), cc::AnimationIdProvider::NextKeyframeModelId(),
      cc::AnimationIdProvider::NextGroupId(),
      cc::KeyframeModel::TargetPropertyId(cc::TargetProperty::OPACITY));

  compositor_keyframe_model_id_ = keyframe_model->id();
  compositor_animation_->AddKeyframeModel(std::move(keyframe_model));
}

void LinkHighlightImpl::StopCompositorAnimation() {
  if (!is_animating_on_compositor_)
    return;

  is_animating_on_compositor_ = false;
  compositor_animation_->RemoveKeyframeModel(compositor_keyframe_model_id_);
  compositor_keyframe_model_id_ = 0;
}

LinkHighlightImpl::LinkHighlightFragment::LinkHighlightFragment() {
  layer_ = cc::PictureLayer::Create(this);
  layer_->SetIsDrawable(true);
  layer_->SetOpacity(kStartOpacity);
}

LinkHighlightImpl::LinkHighlightFragment::~LinkHighlightFragment() {
  layer_->ClearClient();
}

scoped_refptr<cc::DisplayItemList>
LinkHighlightImpl::LinkHighlightFragment::PaintContentsToDisplayList() {
  auto display_list = base::MakeRefCounted<cc::DisplayItemList>();

  PaintRecorder recorder;
  gfx::Rect record_bounds(layer_->bounds());
  cc::PaintCanvas* canvas = recorder.beginRecording();

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(color_.Rgb());
  canvas->drawPath(path_.GetSkPath(), flags);

  display_list->StartPaint();
  display_list->push<cc::DrawRecordOp>(recorder.finishRecordingAsPicture());
  display_list->EndPaintOfUnpaired(record_bounds);

  display_list->Finalize();
  return display_list;
}

void LinkHighlightImpl::UpdateOpacityAndRequestAnimation() {
  if (!node_ || is_animating_on_compositor_ || start_compositor_animation_)
    return;

  // Since the notification about the animation finishing may not arrive in
  // time to remove the link highlight before it's drawn without an animation
  // we set the opacity to the final target opacity to avoid a flash of the
  // initial opacity. https://crbug.com/974160.
  // Note it's also possible we may skip the animation if the property node
  // has not been composited in which case we immediately use the target
  // opacity.
  UpdateOpacity(GetTargetOpacity());

  // We request a compositing update after which UpdateAfterPaint will start
  // the composited animation at the same time as PendingAnimations::Update
  // starts composited web animations.
  SetNeedsRepaintAndCompositingUpdate();
  start_compositor_animation_ = true;
}

void LinkHighlightImpl::NotifyAnimationFinished(base::TimeDelta, int) {
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

  wtf_size_t fragment_count = object->FragmentList().size();
  if (fragment_count != fragments_.size()) {
    wtf_size_t i = fragments_.size();
    fragments_.resize(fragment_count);
    for (; i < fragment_count; ++i) {
      fragments_[i] = std::make_unique<LinkHighlightFragment>();
    }
    SetNeedsRepaintAndCompositingUpdate();
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

  auto color = object->StyleRef().VisitedDependentColor(
      GetCSSPropertyWebkitTapHighlightColor());

  // For now, we'll only use rounded rects if we have a single rect because
  // otherwise we may sometimes get a chain of adjacent boxes (e.g. for text
  // nodes) which end up looking like sausage links: these should ideally be
  // merged into a single rect before creating the path.
  bool use_rounded_rects = !node_->GetDocument()
                                .GetSettings()
                                ->GetMockGestureTapHighlightsEnabled() &&
                           !object->IsFragmented();

  wtf_size_t index = 0;
  for (AccompaniedFragmentIterator iterator(*object); !iterator.IsDone();
       index++) {
    const auto* fragment = iterator.GetFragmentData();
    ScopedDisplayItemFragment scoped_fragment(context, index);
    Vector<PhysicalRect> rects = object->CollectOutlineRectsAndAdvance(
        OutlineType::kIncludeBlockInkOverflow, iterator);
    if (rects.size() > 1)
      use_rounded_rects = false;

    // TODO(yosin): We should remove following if-statement once we release
    // FragmentItem to renderer rounded rect even if nested inline, e.g.
    // <a>ABC<b>DEF</b>GHI</a>.
    // See gesture-tapHighlight-simple-nested.html
    if (use_rounded_rects && object->IsLayoutInline() &&
        object->IsInLayoutNGInlineFormattingContext()) {
      InlineCursor cursor;
      cursor.MoveTo(*object);
      // When |LayoutInline| has more than one children, we render square
      // rectangle as |NGPaintFragment|.
      if (cursor && cursor.CurrentItem()->DescendantsCount() > 2)
        use_rounded_rects = false;
    }

    Path new_path;
    for (auto& rect : rects) {
      gfx::RectF snapped_rect(ToPixelSnappedRect(rect));
      if (use_rounded_rects) {
        constexpr float kRadius = 3;
        new_path.AddRoundedRect(FloatRoundedRect(snapped_rect, kRadius));
      } else {
        new_path.AddRect(snapped_rect);
      }
    }

    DCHECK_LT(index, fragments_.size());
    auto& link_highlight_fragment = *fragments_[index];
    link_highlight_fragment.SetColor(color);

    auto bounding_rect = gfx::ToEnclosingRect(new_path.BoundingRect());
    new_path.Translate(-gfx::Vector2dF(bounding_rect.OffsetFromOrigin()));

    cc::PictureLayer* layer = link_highlight_fragment.Layer();
    CHECK(layer);
    CHECK_EQ(&link_highlight_fragment, layer->client());
    if (link_highlight_fragment.GetPath() != new_path) {
      link_highlight_fragment.SetPath(new_path);
      layer->SetBounds(bounding_rect.size());
      layer->SetNeedsDisplay();
    }

    DEFINE_STATIC_DISPLAY_ITEM_CLIENT(client, "LinkHighlight");
    auto property_tree_state = fragment->LocalBorderBoxProperties().Unalias();
    property_tree_state.SetEffect(Effect());
    RecordForeignLayer(context, *client,
                       DisplayItem::kForeignLayerLinkHighlight, layer,
                       bounding_rect.origin(), &property_tree_state);
  }

  DCHECK_EQ(index, fragments_.size());
}

void LinkHighlightImpl::UpdateAfterPaint(
    const PaintArtifactCompositor* paint_artifact_compositor) {
  bool should_start_animation =
      !is_animating_on_compositor_ && start_compositor_animation_;
  start_compositor_animation_ = false;
  if (!is_animating_on_compositor_ && !should_start_animation)
    return;

  bool is_composited = paint_artifact_compositor->HasComposited(element_id_);
  // If the animating node has not been composited, remove the highlight
  // animation.
  if (is_animating_on_compositor_ && !is_composited)
    StopCompositorAnimation();

  // Skip starting the link highlight animation if the target effect node has
  // not been composited.
  if (!should_start_animation || !is_composited)
    return;

  StartCompositorAnimation();
}

void LinkHighlightImpl::SetNeedsRepaintAndCompositingUpdate() {
  DCHECK(node_);
  if (auto* frame_view = node_->GetDocument().View()) {
    frame_view->SetVisualViewportOrOverlayNeedsRepaint();
    frame_view->SetPaintArtifactCompositorNeedsUpdate();
  }
}

void LinkHighlightImpl::UpdateOpacity(float opacity) {
  auto change =
      effect_->Update(EffectPaintPropertyNode::Root(),
                      LinkHighlightEffectNodeState(opacity, element_id_));
  // If there is no |node_|, |ReleaseResources| has already handled the call to
  // |SetNeedsRepaintAndCompositingUpdate|.
  if (!node_)
    return;
  if (change > PaintPropertyChangeType::kChangedOnlyCompositedValues)
    SetNeedsRepaintAndCompositingUpdate();
}

}  // namespace blink
