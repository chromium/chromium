// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"

#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/numerics/safe_conversions.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/layers/layer.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/render_surface_filters.h"
#include "third_party/blink/renderer/platform/graphics/compositing/chunk_to_layer_mapper.h"
#include "third_party/blink/renderer/platform/graphics/compositing/property_tree_manager.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidation_tracking.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/scrollbar_display_item.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

namespace {

// Adapts cc::PaintOpBuffer to provide cc::DisplayItemList API with empty
// implementations.
class PaintOpBufferExt : public cc::PaintOpBuffer {
 public:
  void StartPaint() {}
  void EndPaintOfUnpaired(const gfx::Rect&) {}
  void EndPaintOfPairedBegin() {}
  void EndPaintOfPairedEnd() {}
  template <typename T, typename... Args>
  size_t push(Args&&... args) {
    size_t offset = next_op_offset();
    PaintOpBuffer::push<T>(std::forward<Args>(args)...);
    return offset;
  }
};

// In a ConversionContext's property state switching function (e.g.
// SwitchToClip), if a scroll translation switch is needed to finish the switch,
// the function returns this struct with kStart or kEnd type, and
// ConversionContext::Convert() will start a new DrawScrollingContentsOp in a
// new ConversionContext, or end the current DrawScrollingContentsOp and return
// to the outer ConversionContext. Then the switch will continue in the new
// context.
struct ScrollTranslationAction {
  STACK_ALLOCATED();

 public:
  enum { kNone, kStart, kEnd } type = kNone;
  const TransformPaintPropertyNode* scroll_translation_to_start = nullptr;

  explicit operator bool() const { return type != kNone; }
};

// State stack of ConversionContext.
// The size of the stack is the number of nested paired items that are
// currently nested. Note that this is a "restore stack", i.e. the top element
// does not represent the current state, but the state prior to applying the
// last paired begin.
struct StateEntry {
  DISALLOW_NEW();

 public:
  // Remembers the type of paired begin that caused a state to be saved.
  // This is for checking integrity of the algorithm.
  enum PairedType { kClip, kClipOmitted, kEffect };
  explicit StateEntry(PairedType type,
                      const TransformPaintPropertyNode* transform,
                      const ClipPaintPropertyNode* clip,
                      const EffectPaintPropertyNode* effect,
                      const TransformPaintPropertyNode* previous_transform)
      : transform(transform),
        clip(clip),
        effect(effect),
        previous_transform(previous_transform),
        type_(type) {}

  void Trace(Visitor* visitor) const {
    visitor->Trace(transform);
    visitor->Trace(clip);
    visitor->Trace(effect);
    visitor->Trace(previous_transform);
  }

  bool IsClip() const { return type_ != kEffect; }
  bool IsEffect() const { return type_ == kEffect; }
  bool NeedsRestore() const { return type_ != kClipOmitted; }

  // These fields are never nullptr. They save ConversionContext::
  // current_transform_, current_clip_ and current_effect_, respectively.
  Member<const TransformPaintPropertyNode> transform;
  Member<const ClipPaintPropertyNode> clip;
  Member<const EffectPaintPropertyNode> effect;
  // This saves ConversionContext::previous_transform_.
  Member<const TransformPaintPropertyNode> previous_transform;
#if DCHECK_IS_ON()
  bool has_effect_hierarchy_issue = false;
#endif

 private:
  PairedType type_;
};

// This structure accumulates bounds of all chunks under an effect. When an
// effect starts, we emit a SaveLayer[Alpha]Op with null bounds, and push a
// new |EffectBoundsInfo| onto |effect_bounds_stack_|. When the effect ends,
// we update the bounds of the op.
struct EffectBoundsInfo {
  DISALLOW_NEW();

 public:
  void Trace(Visitor* visitor) const { visitor->Trace(transform); }

  // The id of the SaveLayer[Alpha]Op for this effect. It's recorded when we
  // push the op for this effect, and used when this effect ends in
  // UpdateSaveLayerBounds().
  size_t save_layer_id;
  // The transform space when the SaveLayer[Alpha]Op was emitted.
  Member<const TransformPaintPropertyNode> transform;
  // Records the bounds of the effect which initiated the entry. Note that
  // the effect is not |effect| (which is the previous effect), but the
  // |current_effect_| when this entry is the top of the stack.
  gfx::RectF bounds;
};

template <typename Result>
class ConversionContext {
  STACK_ALLOCATED();

 public:
  ConversionContext(const PropertyTreeState& layer_state,
                    const gfx::Vector2dF& layer_offset,
                    Result& result,
                    const StateEntry* outer_state_stack_top = nullptr)
      : chunk_to_layer_mapper_(layer_state, layer_offset),
        current_transform_(&layer_state.Transform()),
        current_clip_(&layer_state.Clip()),
        current_effect_(&layer_state.Effect()),
        current_scroll_translation_(
            &current_transform_->NearestScrollTranslationNode()),
        result_(result),
        outer_state_stack_top_(outer_state_stack_top) {}
  ~ConversionContext();

 private:
  void Convert(PaintChunkIterator& chunk_it,
               PaintChunkIterator end_chunk,
               const gfx::Rect* additional_cull_rect = nullptr);

 public:
  // The main function of this class. It converts a list of paint chunks into
  // non-pair display items, and paint properties associated with them are
  // implemented by paired display items.
  // This is done by closing and opening paired items to adjust the current
  // property state to the chunk's state when each chunk is consumed.
  // Note that the clip/effect state is "lazy" in the sense that it stays
  // in whatever state the last chunk left with, and only adjusted when
  // a new chunk is consumed. The class implemented a few helpers to manage
  // state switching so that paired display items are nested properly.
  //
  // State management example (transform tree omitted).
  // Corresponds to unit test PaintChunksToCcLayerTest.InterleavedClipEffect:
  //   Clip tree: C0 <-- C1 <-- C2 <-- C3 <-- C4
  //   Effect tree: E0(clip=C0) <-- E1(clip=C2) <-- E2(clip=C4)
  //   Layer state: C0, E0
  //   Paint chunks: P0(C3, E0), P1(C4, E2), P2(C3, E1), P3(C4, E0)
  // Initialization:
  //   The current state is initalized with the layer state, and starts with
  //   an empty state stack.
  //   current_clip = C0
  //   current_effect = E0
  //   state_stack = []
  // When P0 is consumed, C1, C2 and C3 need to be applied to the state:
  //   Output: Begin_C1 Begin_C2 Begin_C3 Draw_P0
  //   current_clip = C3
  //   state_stack = [C0, C1, C2]
  // When P1 is consumed, C3 needs to be closed before E1 can be entered,
  // then C3 and C4 need to be entered before E2 can be entered:
  //   Output: End_C3 Begin_E1 Begin_C3 Begin_C4 Begin_E2 Draw_P1
  //   current_clip = C4
  //   current_effect = E2
  //   state_stack = [C0, C1, E0, C2, C3, E1]
  // When P2 is consumed, E2 then C4 need to be exited:
  //   Output: End_E2 End_C4 Draw_P2
  //   current_clip = C3
  //   current_effect = E1
  //   state_stack = [C0, C1, E0, C2]
  // When P3 is consumed, C3 must exit before E1 can be exited, then we can
  // enter C3 and C4:
  //   Output: End_C3 End_E1 Enter_C3 Enter_C4 Draw_P3
  //   current_clip = C4
  //   current_effect = E0
  //   state_stack = [C0, C1, C2, C3]
  // At last, close all pushed states to balance pairs (this happens when the
  // context object is destructed):
  //   Output: End_C4 End_C3 End_C2 End_C1
  void Convert(const PaintChunkSubset& chunks,
               const gfx::Rect* additional_cull_rect = nullptr) {
    auto chunk_it = chunks.begin();
    Convert(chunk_it, chunks.end(), additional_cull_rect);
    CHECK(chunk_it == chunks.end());
  }

 private:
  bool HasDrawing(PaintChunkIterator, const PropertyTreeState&) const;

  // Adjust the translation of the whole display list relative to layer offset.
  // It's only called if we actually paint anything.
  void TranslateForLayerOffsetOnce();

  // Switch the current clip to the target state, staying in the same effect.
  // It is no-op if the context is already in the target state.
  // Otherwise zero or more clips will be popped from or pushed onto the
  // current state stack.
  // INPUT:
  // The target clip must be a descendant of the input clip of current effect.
  // OUTPUT:
  // The current transform may be changed.
  // The current clip will change to the target clip.
  // The current effect will not change.
  [[nodiscard]] ScrollTranslationAction SwitchToClip(
      const ClipPaintPropertyNode&);

  // Switch the current effect to the target state.
  // It is no-op if the context is already in the target state.
  // Otherwise zero or more effect effects will be popped from or pushed onto
  // the state stack. As effects getting popped from the stack, clips applied
  // on top of them will be popped as well. Also clips will be pushed at
  // appropriate steps to apply output clip to newly pushed effects.
  // INPUT:
  // The target effect must be a descendant of the layer's effect.
  // OUTPUT:
  // The current transform may be changed.
  // The current clip may be changed, and is guaranteed to be a descendant of
  // the output clip of the target effect.
  // The current effect will change to the target effect.
  [[nodiscard]] ScrollTranslationAction SwitchToEffect(
      const EffectPaintPropertyNode&);

  // Switch the current transform to the target state.
  [[nodiscard]] ScrollTranslationAction SwitchToTransform(
      const TransformPaintPropertyNode&);
  // End the transform state that is established by SwitchToTransform().
  // Called when the next chunk has different property tree state or when we
  // have processed all chunks. See `previous_transform_` for more details.
  void EndTransform();

  // These functions will be specialized for cc::DisplayItemList later.
  ScrollTranslationAction ComputeScrollTranslationAction(
      const TransformPaintPropertyNode&) const {
    return {};
  }
  void EmitDrawScrollingContentsOp(PaintChunkIterator&,
                                   PaintChunkIterator,
                                   const TransformPaintPropertyNode&) {
    NOTREACHED();
  }

  // Applies combined transform from |current_transform_| to |target_transform|
  // This function doesn't change |current_transform_|.
  void ApplyTransform(const TransformPaintPropertyNode& target_transform) {
    if (&target_transform == current_transform_)
      return;
    gfx::Transform projection = TargetToCurrentProjection(target_transform);
    if (projection.IsIdentityOr2dTranslation()) {
      gfx::Vector2dF translation = projection.To2dTranslation();
      if (!translation.IsZero())
        push<cc::TranslateOp>(translation.x(), translation.y());
    } else {
      push<cc::ConcatOp>(gfx::TransformToSkM44(projection));
    }
  }

  gfx::Transform TargetToCurrentProjection(
      const TransformPaintPropertyNode& target_transform) const {
    return GeometryMapper::SourceToDestinationProjection(target_transform,
                                                         *current_transform_);
  }

  void AppendRestore() {
    result_.StartPaint();
    push<cc::RestoreOp>();
    result_.EndPaintOfPairedEnd();
  }

  // Starts an effect state by adjusting clip and transform state, applying
  // the effect as a SaveLayer[Alpha]Op (whose bounds will be updated in
  // EndEffect()), and updating the current state.
  [[nodiscard]] ScrollTranslationAction StartEffect(
      const EffectPaintPropertyNode&);
  // Ends the effect on the top of the state stack if the stack is not empty,
  // and update the bounds of the SaveLayer[Alpha]Op of the effect.
  void EndEffect();
  void UpdateEffectBounds(const gfx::RectF&, const TransformPaintPropertyNode&);

  // Starts a clip state by adjusting the transform state, applying
  // |combined_clip_rect| which is combined from one or more consecutive clips,
  // and updating the current state. |lowest_combined_clip_node| is the lowest
  // node of the combined clips.
  [[nodiscard]] ScrollTranslationAction StartClip(
      const FloatRoundedRect& combined_clip_rect,
      const ClipPaintPropertyNode& lowest_combined_clip_node);
  // Pop one clip state from the top of the stack.
  void EndClip();
  // Pop clip states from the top of the stack until the top is an effect state
  // or the stack is empty.
  [[nodiscard]] ScrollTranslationAction EndClips();

  template <typename T, typename... Args>
  size_t push(Args&&... args) {
    return result_.template push<T>(std::forward<Args>(args)...);
  }

  void PushState(typename StateEntry::PairedType);
  void PopState();

  HeapVector<StateEntry> state_stack_;
  HeapVector<EffectBoundsInfo> effect_bounds_stack_;
  ChunkToLayerMapper chunk_to_layer_mapper_;
  bool translated_for_layer_offset_ = false;

  // These fields are never nullptr.
  const TransformPaintPropertyNode* current_transform_;
  const ClipPaintPropertyNode* current_clip_;
  const EffectPaintPropertyNode* current_effect_;
  const TransformPaintPropertyNode* current_scroll_translation_;

  // The previous transform state before SwitchToTransform() within the current
  // clip/effect state. When the next chunk's transform is different from the
  // current transform we should restore to this transform using EndTransform()
  // which will set this field to nullptr. When a new clip/effect state starts,
  // the value of this field will be saved into the state stack and set to
  // nullptr. When the clip/effect state ends, this field will be restored to
  // the saved value.
  const TransformPaintPropertyNode* previous_transform_ = nullptr;

  Result& result_;

  // Points to the top of stack_stack_ of the outer ConversionContext that
  // initiated the current ConversionContext in EmitDrawScrollingContentsOp().
  const StateEntry* outer_state_stack_top_ = nullptr;
};

template <typename Result>
ConversionContext<Result>::~ConversionContext() {
  // End all states.
  while (state_stack_.size()) {
    if (state_stack_.back().IsEffect()) {
      EndEffect();
    } else {
      EndClip();
    }
  }
  EndTransform();
  if (translated_for_layer_offset_)
    AppendRestore();
}

template <typename Result>
void ConversionContext<Result>::TranslateForLayerOffsetOnce() {
  gfx::Vector2dF layer_offset = chunk_to_layer_mapper_.LayerOffset();
  if (translated_for_layer_offset_ || layer_offset == gfx::Vector2dF()) {
    return;
  }

  result_.StartPaint();
  push<cc::SaveOp>();
  push<cc::TranslateOp>(-layer_offset.x(), -layer_offset.y());
  result_.EndPaintOfPairedBegin();
  translated_for_layer_offset_ = true;
}

// Tries to combine a clip node's clip rect into |combined_clip_rect|.
// Returns whether the clip has been combined.
static bool CombineClip(const ClipPaintPropertyNode& clip,
                        FloatRoundedRect& combined_clip_rect) {
  if (clip.PixelMovingFilter())
    return true;

  // Don't combine into a clip with clip path.
  const auto* parent = clip.UnaliasedParent();
  CHECK(parent);
  if (parent->ClipPath()) {
    return false;
  }

  // Don't combine clips in different transform spaces.
  const auto& transform_space = clip.LocalTransformSpace().Unalias();
  const auto& parent_transform_space = parent->LocalTransformSpace().Unalias();
  if (&transform_space != &parent_transform_space) {
    if (transform_space.Parent() != &parent_transform_space ||
        !transform_space.IsIdentity()) {
      return false;
    }
    // In RasterInducingScroll, don't combine clips across scroll translations.
    if (RuntimeEnabledFeatures::RasterInducingScrollEnabled() &&
        transform_space.ScrollNode()) {
      return false;
    }
  }

  // Don't combine two rounded clip rects.
  bool clip_is_rounded = clip.PaintClipRect().IsRounded();
  bool combined_is_rounded = combined_clip_rect.IsRounded();
  if (clip_is_rounded && combined_is_rounded)
    return false;

  // If one is rounded and the other contains the rounded bounds, use the
  // rounded as the combined.
  if (combined_is_rounded) {
    return clip.PaintClipRect().Rect().Contains(combined_clip_rect.Rect());
  }
  if (clip_is_rounded) {
    if (combined_clip_rect.Rect().Contains(clip.PaintClipRect().Rect())) {
      combined_clip_rect = clip.PaintClipRect();
      return true;
    }
    return false;
  }

  // The combined is the intersection if both are rectangular.
  DCHECK(!combined_is_rounded && !clip_is_rounded);
  combined_clip_rect = FloatRoundedRect(
      IntersectRects(combined_clip_rect.Rect(), clip.PaintClipRect().Rect()));
  return true;
}

template <typename Result>
ScrollTranslationAction ConversionContext<Result>::SwitchToClip(
    const ClipPaintPropertyNode& target_clip) {
  if (&target_clip == current_clip_) {
    return {};
  }

  // Step 1: Exit all clips until the lowest common ancestor is found.
  {
    const auto* lca_clip =
        &target_clip.LowestCommonAncestor(*current_clip_).Unalias();
    const auto* clip = current_clip_;
    while (clip != lca_clip) {
      if (!state_stack_.size() && outer_state_stack_top_) {
        // We are ending a clip that is started from the outer
        // ConversionContext. outer_state_stack_top_ should be always the
        // overflow clip of the current scroll translation.
        CHECK(outer_state_stack_top_->IsClip());
        return {ScrollTranslationAction::kEnd};
      }
      if (!state_stack_.size() || !state_stack_.back().IsClip()) {
        // TODO(crbug.com/40558824): We still have clip hierarchy issues.
        // See crbug.com/40558824#comment57 and crbug.com/352414643 for the
        // test cases.
#if DCHECK_IS_ON()
        DLOG(ERROR) << "Error: Chunk has a clip that escaped its layer's or "
                    << "effect's clip.\ntarget_clip:\n"
                    << target_clip.ToTreeString().Utf8() << "current_clip_:\n"
                    << clip->ToTreeString().Utf8();
#endif
        break;
      }
      DCHECK(clip->Parent());
      clip = &clip->Parent()->Unalias();
      StateEntry& previous_state = state_stack_.back();
      if (clip == lca_clip) {
        // |lca_clip| may be an intermediate clip in a series of combined clips.
        // Jump to the first of the combined clips.
        clip = lca_clip = previous_state.clip;
      }
      if (clip == previous_state.clip) {
        EndClip();
        DCHECK_EQ(current_clip_, clip);
      }
    }
  }

  if (&target_clip == current_clip_) {
    return {};
  }

  // Step 2: Collect all clips between the target clip and the current clip.
  // At this point the current clip must be an ancestor of the target.
  HeapVector<Member<const ClipPaintPropertyNode>, 8> pending_clips;
  for (const auto* clip = &target_clip; clip != current_clip_;
       clip = clip->UnaliasedParent()) {
    // This should never happen unless the DCHECK in step 1 failed.
    if (!clip)
      break;
    pending_clips.push_back(clip);
  }

  // Step 3: Now apply the list of clips in top-down order.
  DCHECK(pending_clips.size());
  auto pending_combined_clip_rect = pending_clips.back()->PaintClipRect();
  const auto* lowest_combined_clip_node = pending_clips.back().Get();
  for (auto i = pending_clips.size() - 1; i--;) {
    const auto* sub_clip = pending_clips[i].Get();
    if (CombineClip(*sub_clip, pending_combined_clip_rect)) {
      // Continue to combine.
      lowest_combined_clip_node = sub_clip;
    } else {
      // |sub_clip| can't be combined to previous clips. Output the current
      // combined clip, and start new combination.
      if (auto action = StartClip(pending_combined_clip_rect,
                                  *lowest_combined_clip_node)) {
        return action;
      }
      pending_combined_clip_rect = sub_clip->PaintClipRect();
      lowest_combined_clip_node = sub_clip;
    }
  }
  if (auto action =
          StartClip(pending_combined_clip_rect, *lowest_combined_clip_node)) {
    return action;
  }

  DCHECK_EQ(current_clip_, &target_clip);
  return {};
}

template <typename Result>
ScrollTranslationAction ConversionContext<Result>::StartClip(
    const FloatRoundedRect& combined_clip_rect,
    const ClipPaintPropertyNode& lowest_combined_clip_node) {
  if (combined_clip_rect.Rect() == gfx::RectF(InfiniteIntRect())) {
    PushState(StateEntry::kClipOmitted);
  } else {
    const auto& local_transform =
        lowest_combined_clip_node.LocalTransformSpace().Unalias();
    if (&local_transform != current_transform_) {
      EndTransform();
      if (auto action = ComputeScrollTranslationAction(local_transform)) {
        return action;
      }
    }
    result_.StartPaint();
    push<cc::SaveOp>();
    ApplyTransform(local_transform);
    const bool antialias = true;
    if (combined_clip_rect.IsRounded()) {
      push<cc::ClipRRectOp>(SkRRect(combined_clip_rect), SkClipOp::kIntersect,
                            antialias);
    } else {
      push<cc::ClipRectOp>(gfx::RectFToSkRect(combined_clip_rect.Rect()),
                           SkClipOp::kIntersect, antialias);
    }
    if (const auto& clip_path = lowest_combined_clip_node.ClipPath()) {
      push<cc::ClipPathOp>(clip_path->GetSkPath(), SkClipOp::kIntersect,
                           antialias);
    }
    result_.EndPaintOfPairedBegin();

    PushState(StateEntry::kClip);
    current_transform_ = &local_transform;
  }
  current_clip_ = &lowest_combined_clip_node;
  return {};
}

bool HasRealEffects(const EffectPaintPropertyNode& current,
                    const EffectPaintPropertyNode& ancestor) {
  for (const auto* node = &current; node != &ancestor;
       node = node->UnaliasedParent()) {
    if (node->HasRealEffects())
      return true;
  }
  return false;
}

template <typename Result>
ScrollTranslationAction ConversionContext<Result>::SwitchToEffect(
    const EffectPaintPropertyNode& target_effect) {
  if (&target_effect == current_effect_) {
    return {};
  }

  // Step 1: Exit all effects until the lowest common ancestor is found.
  const auto& lca_effect =
      target_effect.LowestCommonAncestor(*current_effect_).Unalias();

#if DCHECK_IS_ON()
  bool has_effect_hierarchy_issue = false;
#endif

  while (current_effect_ != &lca_effect) {
    // This EndClips() and the later EndEffect() pop to the parent effect.
    if (auto action = EndClips()) {
      return action;
    }
    if (!state_stack_.size()) {
      // TODO(crbug.com/40558824): We still have clip hierarchy issues.
      // See crbug.com/40558824#comment57 for the test case.
#if DCHECK_IS_ON()
      DLOG(ERROR) << "Error: Chunk has an effect that escapes layer's effect.\n"
                  << "target_effect:\n"
                  << target_effect.ToTreeString().Utf8() << "current_effect_:\n"
                  << current_effect_->ToTreeString().Utf8();
      has_effect_hierarchy_issue = true;
#endif
      // We can continue if the extra effects causing the clip hierarchy issue
      // are no-op.
      if (!HasRealEffects(*current_effect_, lca_effect)) {
        break;
      }
      return {};
    }
    EndEffect();
  }

  // Step 2: Collect all effects between the target effect and the current
  // effect. At this point the current effect must be an ancestor of the target.
  HeapVector<Member<const EffectPaintPropertyNode>, 8> pending_effects;
  for (const auto* effect = &target_effect; effect != &lca_effect;
       effect = effect->UnaliasedParent()) {
    // This should never happen unless the DCHECK in step 1 failed.
    if (!effect)
      break;
    pending_effects.push_back(effect);
  }

  // Step 3: Now apply the list of effects in top-down order.
  for (const auto& sub_effect : base::Reversed(pending_effects)) {
#if DCHECK_IS_ON()
    if (!has_effect_hierarchy_issue)
      DCHECK_EQ(current_effect_, sub_effect->UnaliasedParent());
#endif
    if (auto action = StartEffect(*sub_effect)) {
      return action;
    }
#if DCHECK_IS_ON()
    state_stack_.back().has_effect_hierarchy_issue = has_effect_hierarchy_issue;
    // This applies only to the first new effect.
    has_effect_hierarchy_issue = false;
#endif
  }
  return {};
}

template <typename Result>
ScrollTranslationAction ConversionContext<Result>::StartEffect(
    const EffectPaintPropertyNode& effect) {
  // Before each effect can be applied, we must enter its output clip first,
  // or exit all clips if it doesn't have one.
  if (effect.OutputClip()) {
    if (auto action = SwitchToClip(effect.OutputClip()->Unalias())) {
      return action;
    }
    // Adjust transform first. Though a non-filter effect itself doesn't depend
    // on the transform, switching to the target transform before
    // SaveLayer[Alpha]Op will help the rasterizer optimize a non-filter
    // SaveLayer[Alpha]Op/DrawRecord/Restore sequence into a single DrawRecord
    // which is much faster. This also avoids multiple Save/Concat/.../Restore
    // pairs for multiple consecutive effects in the same transform space, by
    // issuing only one pair around all of the effects.
    if (auto action =
            SwitchToTransform(effect.LocalTransformSpace().Unalias())) {
      return action;
    }
  } else if (auto action = EndClips()) {
    return action;
  }

  bool has_filter = !effect.Filter().IsEmpty();
  bool has_opacity = effect.Opacity() != 1.f;
  // TODO(crbug.com/1334293): Normally backdrop filters should be composited and
  // effect.BackdropFilter() should be null, but compositing can be disabled in
  // rare cases such as PaintPreview. For now non-composited backdrop filters
  // are not supported and are ignored.
  bool has_other_effects = effect.BlendMode() != SkBlendMode::kSrcOver;
  // We always create separate effect nodes for normal effects and filter
  // effects, so we can handle them separately.
  DCHECK(!has_filter || !(has_opacity || has_other_effects));

  // Apply effects.
  size_t save_layer_id = kNotFound;
  result_.StartPaint();
  if (!has_filter) {
    if (has_other_effects) {
      cc::PaintFlags flags;
      flags.setBlendMode(effect.BlendMode());
      flags.setAlphaf(effect.Opacity());
      save_layer_id = push<cc::SaveLayerOp>(flags);
    } else {
      save_layer_id = push<cc::SaveLayerAlphaOp>(effect.Opacity());
    }
  } else {
    // Handle filter effect.
    // The `layer_bounds` parameter is only used to compute the ZOOM lens
    // bounds, which we never generate.
    cc::PaintFlags filter_flags;
    filter_flags.setImageFilter(cc::RenderSurfaceFilters::BuildImageFilter(
        effect.Filter().AsCcFilterOperations()));
    save_layer_id = push<cc::SaveLayerOp>(filter_flags);
  }
  result_.EndPaintOfPairedBegin();

  DCHECK_NE(save_layer_id, kNotFound);

  // Adjust state and push previous state onto effect stack.
  // TODO(trchen): Change input clip to expansion hint once implemented.
  const ClipPaintPropertyNode* input_clip = current_clip_;
  PushState(StateEntry::kEffect);
  effect_bounds_stack_.emplace_back(
      EffectBoundsInfo{save_layer_id, current_transform_});
  current_clip_ = input_clip;
  current_effect_ = &effect;

  if (effect.Filter().HasReferenceFilter()) {
    // Map a random point in the reference box through the filter to determine
    // the bounds of the effect on an empty source. For empty chunks, or chunks
    // with empty bounds, with a filter applied that produces output even when
    // there's no input this will expand the bounds to match.
    gfx::RectF filtered_bounds = current_effect_->MapRect(
        gfx::RectF(effect.Filter().ReferenceBox().CenterPoint(), gfx::SizeF()));
    effect_bounds_stack_.back().bounds = filtered_bounds;
    // Emit an empty paint operation to add the filtered bounds (mapped to layer
    // space) to the visual rect of the filter's SaveLayerOp.
    result_.StartPaint();
    result_.EndPaintOfUnpaired(chunk_to_layer_mapper_.MapVisualRect(
        gfx::ToEnclosingRect(filtered_bounds)));
  }
  return {};
}

template <typename Result>
void ConversionContext<Result>::UpdateEffectBounds(
    const gfx::RectF& bounds,
    const TransformPaintPropertyNode& transform) {
  if (effect_bounds_stack_.empty() || bounds.IsEmpty())
    return;

  auto& effect_bounds_info = effect_bounds_stack_.back();
  gfx::RectF mapped_bounds = bounds;
  GeometryMapper::SourceToDestinationRect(
      transform, *effect_bounds_info.transform, mapped_bounds);
  effect_bounds_info.bounds.Union(mapped_bounds);
}

template <typename Result>
void ConversionContext<Result>::EndEffect() {
#if DCHECK_IS_ON()
  const auto& previous_state = state_stack_.back();
  DCHECK(previous_state.IsEffect());
  if (!previous_state.has_effect_hierarchy_issue) {
    DCHECK_EQ(current_effect_->UnaliasedParent(), previous_state.effect);
  }
  DCHECK_EQ(current_clip_, previous_state.clip);
#endif

  DCHECK(effect_bounds_stack_.size());
  const auto& bounds_info = effect_bounds_stack_.back();
  gfx::RectF bounds = bounds_info.bounds;
  if (current_effect_->Filter().IsEmpty()) {
    if (!bounds.IsEmpty()) {
      result_.UpdateSaveLayerBounds(bounds_info.save_layer_id,
                                    gfx::RectFToSkRect(bounds));
    }
  } else {
    // We need an empty bounds for empty filter to avoid performance issue of
    // PDF renderer. See crbug.com/740824.
    result_.UpdateSaveLayerBounds(bounds_info.save_layer_id,
                                  gfx::RectFToSkRect(bounds));
    // We need to propagate the filtered bounds to the parent.
    bounds = current_effect_->MapRect(bounds);
  }

  effect_bounds_stack_.pop_back();
  EndTransform();
  // Propagate the bounds to the parent effect.
  UpdateEffectBounds(bounds, *current_transform_);
  PopState();
}

template <typename Result>
ScrollTranslationAction ConversionContext<Result>::EndClips() {
  while (state_stack_.size() && state_stack_.back().IsClip()) {
    EndClip();
  }
  if (!state_stack_.size() && outer_state_stack_top_) {
    // outer_state_stack_top_ should be always the overflow clip of the current
    // scroll translation. The outer ConversionState should continue to end the
    // clips.
    CHECK(outer_state_stack_top_->IsClip());
    return {ScrollTranslationAction::kEnd};
  }
  return {};
}

template <typename Result>
void ConversionContext<Result>::EndClip() {
  DCHECK(state_stack_.back().IsClip());
  DCHECK_EQ(state_stack_.back().effect, current_effect_);
  EndTransform();
  PopState();
}

template <typename Result>
void ConversionContext<Result>::PushState(
    typename StateEntry::PairedType type) {
  state_stack_.emplace_back(type, current_transform_, current_clip_,
                            current_effect_, previous_transform_);
  previous_transform_ = nullptr;
}

template <typename Result>
void ConversionContext<Result>::PopState() {
  DCHECK_EQ(nullptr, previous_transform_);

  const auto& previous_state = state_stack_.back();
  if (previous_state.NeedsRestore())
    AppendRestore();
  current_transform_ = previous_state.transform;
  previous_transform_ = previous_state.previous_transform;
  current_clip_ = previous_state.clip;
  current_effect_ = previous_state.effect;
  state_stack_.pop_back();
}

template <typename Result>
ScrollTranslationAction ConversionContext<Result>::SwitchToTransform(
    const TransformPaintPropertyNode& target_transform) {
  if (&target_transform == current_transform_) {
    return {};
  }

  EndTransform();
  if (&target_transform == current_transform_) {
    return {};
  }

  if (auto action = ComputeScrollTranslationAction(target_transform)) {
    return action;
  }

  gfx::Transform projection = TargetToCurrentProjection(target_transform);
  if (projection.IsIdentity()) {
    return {};
  }

  result_.StartPaint();
  push<cc::SaveOp>();
  if (projection.IsIdentityOr2dTranslation()) {
    gfx::Vector2dF translation = projection.To2dTranslation();
    push<cc::TranslateOp>(translation.x(), translation.y());
  } else {
    push<cc::ConcatOp>(gfx::TransformToSkM44(projection));
  }
  result_.EndPaintOfPairedBegin();
  previous_transform_ = current_transform_;
  current_transform_ = &target_transform;
  return {};
}

template <typename Result>
void ConversionContext<Result>::EndTransform() {
  if (!previous_transform_)
    return;

  result_.StartPaint();
  push<cc::RestoreOp>();
  result_.EndPaintOfPairedEnd();
  current_transform_ = previous_transform_;
  previous_transform_ = nullptr;
}

template <>
void ConversionContext<cc::DisplayItemList>::EmitDrawScrollingContentsOp(
    PaintChunkIterator& chunk_it,
    PaintChunkIterator end_chunk,
    const TransformPaintPropertyNode& scroll_translation) {
  CHECK(RuntimeEnabledFeatures::RasterInducingScrollEnabled());
  CHECK(scroll_translation.ScrollNode());
  DCHECK_EQ(previous_transform_, nullptr);

  // Switch to the parent of the scroll translation in the current context.
  auto action = SwitchToTransform(*scroll_translation.UnaliasedParent());
  // This should not need to switch to any other scroll translation.
  CHECK(!action);

  // The scrolling contents will be recorded into this DisplayItemList as if
  // the scrolling contents creates a layer.
  auto scrolling_contents_list = base::MakeRefCounted<cc::DisplayItemList>();
  ConversionContext<cc::DisplayItemList>(
      PropertyTreeState(scroll_translation, *current_clip_, *current_effect_),
      gfx::Vector2dF(), *scrolling_contents_list, &state_stack_.back())
      .Convert(chunk_it, end_chunk);

  EndTransform();
  scrolling_contents_list->Finalize();

  gfx::Rect visual_rect = chunk_to_layer_mapper_.MapVisualRectFromState(
      InfiniteIntRect(),
      PropertyTreeState(scroll_translation,
                        *scroll_translation.ScrollNode()->OverflowClipNode(),
                        // The effect state doesn't matter.
                        chunk_to_layer_mapper_.LayerState().Effect()));
  result_.PushDrawScrollingContentsOp(
      scroll_translation.ScrollNode()->GetCompositorElementId(),
      std::move(scrolling_contents_list), visual_rect);
}

template <>
ScrollTranslationAction
ConversionContext<cc::DisplayItemList>::ComputeScrollTranslationAction(
    const TransformPaintPropertyNode& target_transform) const {
  if (!RuntimeEnabledFeatures::RasterInducingScrollEnabled()) {
    return {};
  }

  const auto& target_scroll_translation =
      target_transform.NearestScrollTranslationNode();
  if (&target_scroll_translation == current_scroll_translation_) {
    return {};
  }

  const auto& chunk_scroll_translation = chunk_to_layer_mapper_.ChunkState()
                                             .Transform()
                                             .NearestScrollTranslationNode();
  // In most real-world cases, target_scroll_translation equals
  // chunk_scroll_translation. In less common cases, chunk_scroll_translation
  // is deeper than target_scroll_translation (e.g. when a chunk enters
  // multiple levels of scrolling states). In very rare case for a
  // non-composited fixed-attachment background, target_scroll_translation of
  // the background clip is deeper than chunk_scroll_translation, and we
  // should emit a transform operation for the clip to avoid infinite loop of
  // starting (by the transform of the clip) and ending (by the chunk
  // transform) empty DrawScrollingContentsOps.
  if (!target_scroll_translation.IsAncestorOf(chunk_scroll_translation)) {
    return {};
  }

  if (current_scroll_translation_ ==
      target_scroll_translation.ParentScrollTranslationNode()) {
    // We need to enter a new level of scroll translation. If a PaintChunk
    // enters multiple levels of scroll translations at once, this function
    // will be called for each level of overflow clip before it's called for
    // the scrolling contents, so we only need to check one level of scroll
    // translation here.
    return {ScrollTranslationAction::kStart, &target_scroll_translation};
  }

  DCHECK(target_scroll_translation.IsAncestorOf(*current_scroll_translation_));
  return {ScrollTranslationAction::kEnd};
}

template <typename Result>
bool ConversionContext<Result>::HasDrawing(
    PaintChunkIterator chunk_it,
    const PropertyTreeState& chunk_state) const {
  // If we have an empty paint chunk, then we would prefer ignoring it.
  // However, a reference filter can generate visible effect from invisible
  // source, and we need to emit paint operations for it.
  if (&chunk_state.Effect() != current_effect_) {
    return true;
  }
  DisplayItemRange items = chunk_it.DisplayItems();
  if (items.size() == 0) {
    return false;
  }
  if (items.size() > 1) {
    // Assume the chunk has drawing if it has more than one display items.
    return true;
  }
  if (auto* drawing = DynamicTo<DrawingDisplayItem>(*items.begin())) {
    if (drawing->GetPaintRecord().empty() &&
        // See can_ignore_record in Convert()'s inner loop.
        &chunk_state.Effect() == &EffectPaintPropertyNode::Root()) {
      return false;
    }
  }
  return true;
}

template <typename Result>
void ConversionContext<Result>::Convert(PaintChunkIterator& chunk_it,
                                        PaintChunkIterator end_chunk,
                                        const gfx::Rect* additional_cull_rect) {
  for (; chunk_it != end_chunk; ++chunk_it) {
    const auto& chunk = *chunk_it;
    if (chunk.effectively_invisible) {
      continue;
    }

    PropertyTreeState chunk_state = chunk.properties.Unalias();
    if (!HasDrawing(chunk_it, chunk_state)) {
      continue;
    }

    TranslateForLayerOffsetOnce();
    chunk_to_layer_mapper_.SwitchToChunkWithState(chunk, chunk_state);

    if (additional_cull_rect) {
      gfx::Rect chunk_visual_rect =
          chunk_to_layer_mapper_.MapVisualRect(chunk.drawable_bounds);
      if (additional_cull_rect &&
          !additional_cull_rect->Intersects(chunk_visual_rect)) {
        continue;
      }
    }

    ScrollTranslationAction action = SwitchToEffect(chunk_state.Effect());
    if (!action) {
      action = SwitchToClip(chunk_state.Clip());
    }
    if (!action) {
      action = SwitchToTransform(chunk_state.Transform());
    }
    if (action.type == ScrollTranslationAction::kStart) {
      CHECK(action.scroll_translation_to_start);
      EmitDrawScrollingContentsOp(chunk_it, end_chunk,
                                  *action.scroll_translation_to_start);
      // Now chunk_it points to the last chunk in the scrolling contents.
      // We need to continue with the chunk in the next loop in case switching
      // to the chunk state hasn't finished in EmitDrawScrollingContentsOp.
      // The following line neutralize the ++chunk_it in the `for` statement.
      --chunk_it;
      continue;
    }
    if (action.type == ScrollTranslationAction::kEnd) {
      if (outer_state_stack_top_) {
        // Return to the calling EmitDrawScrollingContentsOp().
        return;
      } else {
        // TODO(crbug.com/40558824): This can happen when we encounter a
        // clip hierarchy issue. We have to continue.
      }
    }

    for (const auto& item : chunk_it.DisplayItems()) {
      PaintRecord record;
      if (auto* scrollbar = DynamicTo<ScrollbarDisplayItem>(item)) {
        record = scrollbar->Paint();
      } else if (auto* drawing = DynamicTo<DrawingDisplayItem>(item)) {
        record = drawing->GetPaintRecord();
      } else {
        continue;
      }

      // If we have an empty paint record, then we would prefer ignoring it.
      // However, if we also have a non-root effect, the empty paint record
      // might be for a mask with empty content which should make the masked
      // content fully invisible. We need to "draw" this record to ensure that
      // the effect has correct visual rect.
      bool can_ignore_record =
          &chunk_state.Effect() == &EffectPaintPropertyNode::Root();
      if (record.empty() && can_ignore_record) {
        continue;
      }

      gfx::Rect visual_rect =
          chunk_to_layer_mapper_.MapVisualRect(item.VisualRect());
      if (additional_cull_rect && can_ignore_record &&
          !additional_cull_rect->Intersects(visual_rect)) {
        continue;
      }

      result_.StartPaint();
      if (!record.empty()) {
        push<cc::DrawRecordOp>(std::move(record));
      }
      result_.EndPaintOfUnpaired(visual_rect);
    }

    // Most effects apply to drawable contents only. Reference filters are
    // exceptions, for which we have already added the chunk bounds mapped
    // through the filter to the bounds of the effect in StartEffect().
    UpdateEffectBounds(gfx::RectF(chunk.drawable_bounds),
                       chunk_state.Transform());
  }
}

}  // unnamed namespace

void PaintChunksToCcLayer::ConvertInto(
    const PaintChunkSubset& chunks,
    const PropertyTreeState& layer_state,
    const gfx::Vector2dF& layer_offset,
    RasterUnderInvalidationCheckingParams* under_invalidation_checking_params,
    cc::DisplayItemList& cc_list) {
  ConversionContext(layer_state, layer_offset, cc_list).Convert(chunks);
  if (under_invalidation_checking_params) {
    auto& params = *under_invalidation_checking_params;
    PaintRecorder recorder;
    recorder.beginRecording();
    // Create a complete cloned list for under-invalidation checking. We can't
    // use cc_list because it is not finalized yet.
    PaintOpBufferExt buffer;
    ConversionContext(layer_state, layer_offset, buffer).Convert(chunks);
    recorder.getRecordingCanvas()->drawPicture(buffer.ReleaseAsRecord());
    params.tracking.CheckUnderInvalidations(params.debug_name,
                                            recorder.finishRecordingAsPicture(),
                                            params.interest_rect);
    auto under_invalidation_record = params.tracking.UnderInvalidationRecord();
    if (!under_invalidation_record.empty()) {
      cc_list.StartPaint();
      cc_list.push<cc::DrawRecordOp>(std::move(under_invalidation_record));
      cc_list.EndPaintOfUnpaired(params.interest_rect);
    }
  }
}

PaintRecord PaintChunksToCcLayer::Convert(const PaintChunkSubset& chunks,
                                          const PropertyTreeState& layer_state,
                                          const gfx::Rect* cull_rect) {
  PaintOpBufferExt buffer;
  ConversionContext(layer_state, gfx::Vector2dF(), buffer)
      .Convert(chunks, cull_rect);
  return buffer.ReleaseAsRecord();
}

namespace {

struct NonCompositedScroll {
  DISALLOW_NEW();

 public:
  Member<const TransformPaintPropertyNode> scroll_translation;
  // The hit-testable rect of the scroller in the layer space.
  gfx::Rect layer_hit_test_rect;
  // Accumulated hit test opaqueness of a) the scroller itself and b)
  // contents after the scroller intersecting layer_hit_test_rect.
  // If it's kMixed, scroll in some areas in the layer can't reliably scroll
  // `scroll_translation`.
  cc::HitTestOpaqueness hit_test_opaqueness;

  void Trace(Visitor* visitor) const { visitor->Trace(scroll_translation); }
};

class LayerPropertiesUpdater {
  STACK_ALLOCATED();

 public:
  LayerPropertiesUpdater(cc::Layer& layer,
                         const PropertyTreeState& layer_state,
                         const PaintChunkSubset& chunks,
                         cc::LayerSelection& layer_selection,
                         bool selection_only)
      : chunk_to_layer_mapper_(layer_state, layer.offset_to_transform_parent()),
        layer_(layer),
        chunks_(chunks),
        layer_selection_(layer_selection),
        selection_only_(selection_only),
        layer_scroll_translation_(
            layer_state.Transform().NearestScrollTranslationNode()) {}

  void Update();

 private:
  TouchAction ShouldDisableCursorControl();
  void UpdateTouchActionRegion(const HitTestData&);
  void UpdateWheelEventRegion(const HitTestData&);

  void UpdateScrollHitTestData(const PaintChunk&);
  void AddNonCompositedScroll(const PaintChunk&);
  const TransformPaintPropertyNode& TopNonCompositedScroll(
      const TransformPaintPropertyNode&) const;
  void UpdatePreviousNonCompositedScrolls(const PaintChunk&);

  void UpdateForNonCompositedScrollbar(const ScrollbarDisplayItem&);
  void UpdateRegionCaptureData(const RegionCaptureData&);
  gfx::Point MapSelectionBoundPoint(const gfx::Point&) const;
  cc::LayerSelectionBound PaintedSelectionBoundToLayerSelectionBound(
      const PaintedSelectionBound&) const;
  void UpdateLayerSelection(const LayerSelectionData&);

  ChunkToLayerMapper chunk_to_layer_mapper_;
  cc::Layer& layer_;
  const PaintChunkSubset& chunks_;
  cc::LayerSelection& layer_selection_;
  bool selection_only_;
  const TransformPaintPropertyNode& layer_scroll_translation_;

  cc::TouchActionRegion touch_action_region_;
  TouchAction last_disable_cursor_control_ = TouchAction::kNone;
  const ScrollPaintPropertyNode* last_disable_cursor_control_scroll_ = nullptr;

  cc::Region wheel_event_region_;
  cc::Region main_thread_scroll_hit_test_region_;
  viz::RegionCaptureBounds capture_bounds_;

  // Top-level (i.e., non-nested) non-composited scrolls. Nested non-composited
  // scrollers will force the containing top non-composited scroller to hit test
  // on the main thread, to avoid the complexity and cost of mapping the scroll
  // hit test rect of nested scroller to the layer space, especially when the
  // parent scroller scrolls. TODO(crbug.com/359279553): Investigate if we can
  // optimize this.
  HeapVector<NonCompositedScroll, 4> top_non_composited_scrolls_;
};

TouchAction LayerPropertiesUpdater::ShouldDisableCursorControl() {
  const auto* scroll_node = chunk_to_layer_mapper_.ChunkState()
                                .Transform()
                                .NearestScrollTranslationNode()
                                .ScrollNode();
  if (scroll_node == last_disable_cursor_control_scroll_) {
    return last_disable_cursor_control_;
  }

  last_disable_cursor_control_scroll_ = scroll_node;
  // If the element has an horizontal scrollable ancestor (including itself), we
  // need to disable cursor control by setting the bit kInternalPanXScrolls.
  last_disable_cursor_control_ = TouchAction::kNone;
  // TODO(input-dev): Consider to share the code with
  // ThreadedInputHandler::FindNodeToLatch.
  for (; scroll_node; scroll_node = scroll_node->Parent()) {
    if (scroll_node->UserScrollableHorizontal() &&
        scroll_node->ContainerRect().width() <
            scroll_node->ContentsRect().width()) {
      last_disable_cursor_control_ = TouchAction::kInternalPanXScrolls;
      break;
    }
    // If it is not kAuto, scroll can't propagate, so break here.
    if (scroll_node->OverscrollBehaviorX() !=
        cc::OverscrollBehavior::Type::kAuto) {
      break;
    }
  }
  return last_disable_cursor_control_;
}

void LayerPropertiesUpdater::UpdateTouchActionRegion(
    const HitTestData& hit_test_data) {
  if (hit_test_data.touch_action_rects.empty()) {
    return;
  }

  for (const auto& touch_action_rect : hit_test_data.touch_action_rects) {
    gfx::Rect rect =
        chunk_to_layer_mapper_.MapVisualRect(touch_action_rect.rect);
    if (rect.IsEmpty()) {
      continue;
    }
    TouchAction touch_action = touch_action_rect.allowed_touch_action;
    if ((touch_action & TouchAction::kPanX) != TouchAction::kNone) {
      touch_action |= ShouldDisableCursorControl();
    }
    touch_action_region_.Union(touch_action, rect);
  }
}

void LayerPropertiesUpdater::UpdateWheelEventRegion(
    const HitTestData& hit_test_data) {
  for (const auto& wheel_event_rect : hit_test_data.wheel_event_rects) {
    wheel_event_region_.Union(
        chunk_to_layer_mapper_.MapVisualRect(wheel_event_rect));
  }
}

void LayerPropertiesUpdater::UpdateScrollHitTestData(const PaintChunk& chunk) {
  const HitTestData& hit_test_data = *chunk.hit_test_data;
  if (hit_test_data.scroll_hit_test_rect.IsEmpty()) {
    return;
  }

  // A scroll hit test rect contributes to the non-fast scrollable region if
  // - the scroll_translation pointer is null, or
  // - the scroll node is not composited.
  if (const auto scroll_translation = hit_test_data.scroll_translation) {
    const auto* scroll_node = scroll_translation->ScrollNode();
    DCHECK(scroll_node);
    // TODO(crbug.com/1230615): Remove this when we fix the root cause.
    if (!scroll_node) {
      return;
    }
    auto scroll_element_id = scroll_node->GetCompositorElementId();
    if (layer_.element_id() == scroll_element_id) {
      // layer_ is the composited layer of the scroll hit test chunk.
      return;
    }
  }

  if (RuntimeEnabledFeatures::FastNonCompositedScrollHitTestEnabled() &&
      hit_test_data.scroll_translation) {
    CHECK_EQ(chunk.id.type, DisplayItem::Type::kScrollHitTest);
    AddNonCompositedScroll(chunk);
    return;
  }

  gfx::Rect rect =
      chunk_to_layer_mapper_.MapVisualRect(hit_test_data.scroll_hit_test_rect);
  if (rect.IsEmpty()) {
    return;
  }
  main_thread_scroll_hit_test_region_.Union(rect);

  // The scroll hit test rect of scrollbar or resizer also contributes to the
  // touch action region.
  if (chunk.id.type == DisplayItem::Type::kScrollbarHitTest ||
      chunk.id.type == DisplayItem::Type::kResizerScrollHitTest) {
    touch_action_region_.Union(TouchAction::kNone, rect);
  }
}

const TransformPaintPropertyNode&
LayerPropertiesUpdater::TopNonCompositedScroll(
    const TransformPaintPropertyNode& scroll_translation) const {
  const auto* node = &scroll_translation;
  do {
    const auto* parent = node->ParentScrollTranslationNode();
    if (parent == &layer_scroll_translation_) {
      return *node;
    }
    node = parent;
  } while (node);
  // TODO(crbug.com/40558824): Abnormal hierarchy.
  return scroll_translation;
}

void LayerPropertiesUpdater::AddNonCompositedScroll(const PaintChunk& chunk) {
  DCHECK(RuntimeEnabledFeatures::FastNonCompositedScrollHitTestEnabled());
  const auto& scroll_translation = *chunk.hit_test_data->scroll_translation;
  const auto& top_scroll = TopNonCompositedScroll(scroll_translation);
  if (&top_scroll == &scroll_translation) {
    auto hit_test_opaqueness = chunk.hit_test_opaqueness;
    if (hit_test_opaqueness == cc::HitTestOpaqueness::kOpaque &&
        !chunk_to_layer_mapper_.ClipRect().IsTight()) {
      hit_test_opaqueness = cc::HitTestOpaqueness::kMixed;
    }
    top_non_composited_scrolls_.emplace_back(
        &scroll_translation,
        chunk_to_layer_mapper_.MapVisualRect(
            chunk.hit_test_data->scroll_hit_test_rect),
        hit_test_opaqueness);
  } else {
    // A top non-composited scroller with nested non-composited scrollers is
    // forced to be non-fast.
    for (auto& scroll : top_non_composited_scrolls_) {
      if (scroll.scroll_translation == &top_scroll) {
        scroll.hit_test_opaqueness = cc::HitTestOpaqueness::kMixed;
        break;
      }
    }
  }
}

// Updates hit_test_opaqueness on previous non-composited scrollers to be
// HitTestOpaqueness::kMixed if the chunk is hit testable and overlaps.
// Hit tests in these cases cannot be handled on the compositor thread.
void LayerPropertiesUpdater::UpdatePreviousNonCompositedScrolls(
    const PaintChunk& chunk) {
  if (top_non_composited_scrolls_.empty()) {
    return;
  }
  DCHECK(RuntimeEnabledFeatures::FastNonCompositedScrollHitTestEnabled());

  if (chunk.hit_test_data && chunk.hit_test_data->scroll_translation) {
    // ScrollHitTest has been handled in AddNonCompositedScroll().
    return;
  }

  if (chunk.hit_test_opaqueness == cc::HitTestOpaqueness::kTransparent) {
    return;
  }

  const auto* scroll_translation =
      &chunk.properties.Transform().Unalias().NearestScrollTranslationNode();
  if (scroll_translation == &layer_scroll_translation_) {
    // The new chunk is not scrollable in the layer. Any previous scroller
    // intersecting with the new chunk will need main thread hit test.
    gfx::Rect chunk_hit_test_rect =
        chunk_to_layer_mapper_.MapVisualRect(chunk.bounds);
    for (auto& previous_scroll : base::Reversed(top_non_composited_scrolls_)) {
      if (previous_scroll.layer_hit_test_rect.Intersects(chunk_hit_test_rect)) {
        previous_scroll.hit_test_opaqueness = cc::HitTestOpaqueness::kMixed;
      }
      if (previous_scroll.layer_hit_test_rect.Contains(chunk_hit_test_rect)) {
        break;
      }
    }
    return;
  }

  const auto& top_scroll = TopNonCompositedScroll(*scroll_translation);
  if (&top_scroll != scroll_translation) {
    // The chunk is under a nested non-composited scroller. We should have
    // forced or will force the top scroll to be non-fast, so we don't need
    // to do anything here.
    return;
  }
  // The chunk is in the scrolling contents of a top non-composited scroller.
  // Find the scroller. Normally the loop runs only one iteration, unless the
  // scrolling contents of the scroller interlace with other scrollers.
  NonCompositedScroll* non_composited_scroll = nullptr;
  for (auto& previous_scroll : base::Reversed(top_non_composited_scrolls_)) {
    if (previous_scroll.scroll_translation == scroll_translation) {
      non_composited_scroll = &previous_scroll;
      break;
    }
  }
  if (!non_composited_scroll) {
    // The chunk appears before the ScrollHitTest chunk of top_scroll.
    // The chunk's hit-test status doesn't matter because it will be covered
    // by the future ScrollHitTest.
    return;
  }
  if (non_composited_scroll->hit_test_opaqueness ==
      cc::HitTestOpaqueness::kTransparent) {
    // non_composited_scroll has pointer-events:none but the chunk is
    // hit-testable.
    non_composited_scroll->hit_test_opaqueness = cc::HitTestOpaqueness::kMixed;
  }
  if (non_composited_scroll->hit_test_opaqueness ==
      cc::HitTestOpaqueness::kMixed) {
    // non_composited_scroll will generate a rect in
    // main_thread_scroll_hit_test_region_ which will disable all fast scroll
    // in the area, so no need to check overlap with other scrollers.
    return;
  }

  // Assume the chunk can appear anywhere in non_composited_scroll, so use
  // non_composited_scroll->layer_hit_test_rect to check overlap.
  const gfx::Rect& hit_test_rect = non_composited_scroll->layer_hit_test_rect;
  // This is the same as the loop under '== &layer_scroll_translation_` but
  // stops at scroll_translation. Normally this loop is no-op, unless the
  // scrolling contents of the scroller interlace with other scrollers
  // (which will be tested overlap with the hit_test_rect).
  for (auto& previous_scroll : base::Reversed(top_non_composited_scrolls_)) {
    if (previous_scroll.scroll_translation == scroll_translation) {
      break;
    }
    if (previous_scroll.layer_hit_test_rect.Intersects(hit_test_rect)) {
      previous_scroll.hit_test_opaqueness = cc::HitTestOpaqueness::kMixed;
    }
    if (previous_scroll.layer_hit_test_rect.Contains(hit_test_rect)) {
      break;
    }
  }
}

const ScrollbarDisplayItem* NonCompositedScrollbarDisplayItem(
    PaintChunkIterator chunk_it,
    const cc::Layer& layer) {
  if (chunk_it->size() != 1) {
    return nullptr;
  }
  const auto* scrollbar =
      DynamicTo<ScrollbarDisplayItem>(*chunk_it.DisplayItems().begin());
  if (!scrollbar) {
    return nullptr;
  }
  if (scrollbar->ElementId() == layer.element_id()) {
    // layer_ is the composited layer of the scrollbar.
    return nullptr;
  }
  return scrollbar;
}

void LayerPropertiesUpdater::UpdateForNonCompositedScrollbar(
    const ScrollbarDisplayItem& scrollbar) {
  // A non-composited scrollbar contributes to the non-fast scrolling region
  // and the touch action region.
  gfx::Rect rect = chunk_to_layer_mapper_.MapVisualRect(scrollbar.VisualRect());
  if (rect.IsEmpty()) {
    return;
  }
  main_thread_scroll_hit_test_region_.Union(rect);
  touch_action_region_.Union(TouchAction::kNone, rect);
}

void LayerPropertiesUpdater::UpdateRegionCaptureData(
    const RegionCaptureData& region_capture_data) {
  for (const std::pair<RegionCaptureCropId, gfx::Rect>& pair :
       region_capture_data.map) {
    capture_bounds_.Set(pair.first.value(),
                        chunk_to_layer_mapper_.MapVisualRect(pair.second));
  }
}

gfx::Point LayerPropertiesUpdater::MapSelectionBoundPoint(
    const gfx::Point& point) const {
  return gfx::ToRoundedPoint(
      chunk_to_layer_mapper_.Transform().MapPoint(gfx::PointF(point)));
}

cc::LayerSelectionBound
LayerPropertiesUpdater::PaintedSelectionBoundToLayerSelectionBound(
    const PaintedSelectionBound& bound) const {
  cc::LayerSelectionBound layer_bound;
  layer_bound.type = bound.type;
  layer_bound.hidden = bound.hidden;
  layer_bound.edge_start = MapSelectionBoundPoint(bound.edge_start);
  layer_bound.edge_end = MapSelectionBoundPoint(bound.edge_end);
  return layer_bound;
}

void LayerPropertiesUpdater::UpdateLayerSelection(
    const LayerSelectionData& layer_selection_data) {
  if (layer_selection_data.start) {
    layer_selection_.start =
        PaintedSelectionBoundToLayerSelectionBound(*layer_selection_data.start);
    layer_selection_.start.layer_id = layer_.id();
  }

  if (layer_selection_data.end) {
    layer_selection_.end =
        PaintedSelectionBoundToLayerSelectionBound(*layer_selection_data.end);
    layer_selection_.end.layer_id = layer_.id();
  }
}

void LayerPropertiesUpdater::Update() {
  bool any_selection_was_painted = false;
  for (auto it = chunks_.begin(); it != chunks_.end(); ++it) {
    const PaintChunk& chunk = *it;
    const auto* non_composited_scrollbar =
        NonCompositedScrollbarDisplayItem(it, layer_);
    if ((!selection_only_ &&
         (chunk.hit_test_data || non_composited_scrollbar ||
          chunk.region_capture_data || !top_non_composited_scrolls_.empty())) ||
        chunk.layer_selection_data) {
      chunk_to_layer_mapper_.SwitchToChunk(chunk);
    }
    if (!selection_only_) {
      if (chunk.hit_test_data) {
        UpdateTouchActionRegion(*chunk.hit_test_data);
        UpdateWheelEventRegion(*chunk.hit_test_data);
        UpdateScrollHitTestData(chunk);
      }
      UpdatePreviousNonCompositedScrolls(chunk);
      if (non_composited_scrollbar) {
        UpdateForNonCompositedScrollbar(*non_composited_scrollbar);
      }
      if (chunk.region_capture_data) {
        UpdateRegionCaptureData(*chunk.region_capture_data);
      }
    }
    if (chunk.layer_selection_data) {
      any_selection_was_painted |=
          chunk.layer_selection_data->any_selection_was_painted;
      UpdateLayerSelection(*chunk.layer_selection_data);
    }
  }

  if (!selection_only_) {
    layer_.SetTouchActionRegion(std::move(touch_action_region_));
    layer_.SetWheelEventRegion(std::move(wheel_event_region_));
    layer_.SetCaptureBounds(std::move(capture_bounds_));

    std::vector<cc::ScrollHitTestRect> non_composited_scroll_hit_test_rects;
    for (const auto& scroll : top_non_composited_scrolls_) {
      if (scroll.hit_test_opaqueness == cc::HitTestOpaqueness::kMixed) {
        main_thread_scroll_hit_test_region_.Union(scroll.layer_hit_test_rect);
      } else if (scroll.hit_test_opaqueness == cc::HitTestOpaqueness::kOpaque) {
        non_composited_scroll_hit_test_rects.emplace_back(
            scroll.scroll_translation->ScrollNode()->GetCompositorElementId(),
            scroll.layer_hit_test_rect);
      }
    }
    layer_.SetMainThreadScrollHitTestRegion(
        std::move(main_thread_scroll_hit_test_region_));
    layer_.SetNonCompositedScrollHitTestRects(
        std::move(non_composited_scroll_hit_test_rects));
  }

  if (any_selection_was_painted) {
    // If any selection was painted, but we didn't see the start or end bound
    // recorded, it could have been outside of the painting cull rect thus
    // invisible. Mark the bound as such if this is the case.
    if (layer_selection_.start.type == gfx::SelectionBound::EMPTY) {
      layer_selection_.start.type = gfx::SelectionBound::LEFT;
      layer_selection_.start.hidden = true;
    }

    if (layer_selection_.end.type == gfx::SelectionBound::EMPTY) {
      layer_selection_.end.type = gfx::SelectionBound::RIGHT;
      layer_selection_.end.hidden = true;
    }
  }
}

}  // namespace

void PaintChunksToCcLayer::UpdateLayerProperties(
    cc::Layer& layer,
    const PropertyTreeState& layer_state,
    const PaintChunkSubset& chunks,
    cc::LayerSelection& layer_selection,
    bool selection_only) {
  LayerPropertiesUpdater(layer, layer_state, chunks, layer_selection,
                         selection_only)
      .Update();
}

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::StateEntry)
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::EffectBoundsInfo)
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::NonCompositedScroll)
