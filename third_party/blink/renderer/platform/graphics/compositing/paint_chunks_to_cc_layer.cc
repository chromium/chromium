// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"

#include "base/logging.h"
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

template <typename Result>
class ConversionContext {
  STACK_ALLOCATED();

 public:
  ConversionContext(const PropertyTreeState& layer_state,
                    const gfx::Vector2dF& layer_offset,
                    Result& result)
      : layer_state_(layer_state),
        layer_offset_(layer_offset),
        current_transform_(&layer_state.Transform()),
        current_clip_(&layer_state.Clip()),
        current_effect_(&layer_state.Effect()),
        chunk_to_layer_mapper_(layer_state_, layer_offset_),
        result_(result) {}
  ~ConversionContext();

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
  void Convert(const PaintChunkSubset&,
               const gfx::Rect* additional_cull_rect = nullptr);

 private:
  // Adjust the translation of the whole display list relative to layer offset.
  // It's only called if we actually paint anything.
  void TranslateForLayerOffsetOnce();

  // Switch the current property tree state to the chunk's state. It's only
  // called if we actually paint anything, and should execute for a chunk
  // only once.
  void SwitchToChunkState(const PaintChunk&);

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
  void SwitchToClip(const ClipPaintPropertyNode&);

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
  void SwitchToEffect(const EffectPaintPropertyNode&);

  // Switch the current transform to the target state.
  void SwitchToTransform(const TransformPaintPropertyNode&);
  // End the transform state that is estalished by SwitchToTransform().
  // Called when the next chunk has different property tree state and when we
  // have processed all chunks.
  void EndTransform();

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
  void StartEffect(const EffectPaintPropertyNode&);
  // Ends the effect on the top of the state stack if the stack is not empty,
  // and update the bounds of the SaveLayer[Alpha]Op of the effect.
  void EndEffect();
  void UpdateEffectBounds(const gfx::RectF&, const TransformPaintPropertyNode&);

  // Starts a clip state by adjusting the transform state, applying
  // |combined_clip_rect| which is combined from one or more consecutive clips,
  // and updating the current state. |lowest_combined_clip_node| is the lowest
  // node of the combined clips.
  void StartClip(const FloatRoundedRect& combined_clip_rect,
                 const ClipPaintPropertyNode& lowest_combined_clip_node);
  // Pop one clip state from the top of the stack.
  void EndClip();
  // Pop clip states from the top of the stack until the top is an effect state
  // or the stack is empty.
  void EndClips();

  template <typename T, typename... Args>
  size_t push(Args&&... args) {
    return result_.template push<T>(std::forward<Args>(args)...);
  }

  // State stack.
  // The size of the stack is the number of nested paired items that are
  // currently nested. Note that this is a "restore stack", i.e. the top
  // element does not represent the current state, but the state prior to
  // applying the last paired begin.
  struct StateEntry {
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

    bool IsClip() const { return type_ != kEffect; }
    bool IsEffect() const { return type_ == kEffect; }
    bool NeedsRestore() const { return type_ != kClipOmitted; }

    // These fields are neve nullptr.
    const TransformPaintPropertyNode* transform;
    const ClipPaintPropertyNode* clip;
    const EffectPaintPropertyNode* effect;
    // See ConversionContext<Result>::previous_transform_.
    const TransformPaintPropertyNode* previous_transform;
#if DCHECK_IS_ON()
    bool has_pre_cap_effect_hierarchy_issue = false;
#endif

   private:
    PairedType type_;
  };
  void PushState(typename StateEntry::PairedType);
  void PopState();
  Vector<StateEntry> state_stack_;

  const PropertyTreeState& layer_state_;
  const gfx::Vector2dF layer_offset_;
  bool translated_for_layer_offset_ = false;

  // These fields are never nullptr.
  const TransformPaintPropertyNode* current_transform_;
  const ClipPaintPropertyNode* current_clip_;
  const EffectPaintPropertyNode* current_effect_;

  // The previous transform state before SwitchToTransform() within the current
  // clip/effect state. When the next chunk's transform is different from the
  // current transform we should restore to this transform using EndTransform()
  // which will set this field to nullptr. When a new clip/effect state starts,
  // the value of this field will be saved into the state stack and set to
  // nullptr. When the clip/effect state ends, this field will be restored to
  // the saved value.
  const TransformPaintPropertyNode* previous_transform_ = nullptr;

  // This structure accumulates bounds of all chunks under an effect. When an
  // effect starts, we emit a SaveLayer[Alpha]Op with null bounds, and push a
  // new |EffectBoundsInfo| onto |effect_bounds_stack_|. When the effect ends,
  // we update the bounds of the op.
  struct EffectBoundsInfo {
    // The id of the SaveLayer[Alpha]Op for this effect. It's recorded when we
    // push the op for this effect, and used when this effect ends in
    // UpdateSaveLayerBounds().
    size_t save_layer_id;
    // The transform space when the SaveLayer[Alpha]Op was emitted.
    const TransformPaintPropertyNode* transform;
    // Records the bounds of the effect which initiated the entry. Note that
    // the effect is not |effect| (which is the previous effect), but the
    // |current_effect_| when this entry is the top of the stack.
    gfx::RectF bounds;
  };
  Vector<EffectBoundsInfo> effect_bounds_stack_;

  ChunkToLayerMapper chunk_to_layer_mapper_;

  Result& result_;
};

template <typename Result>
ConversionContext<Result>::~ConversionContext() {
  // End all states.
  while (state_stack_.size()) {
    if (state_stack_.back().IsEffect())
      EndEffect();
    else
      EndClip();
  }
  EndTransform();
  if (translated_for_layer_offset_)
    AppendRestore();
}

template <typename Result>
void ConversionContext<Result>::TranslateForLayerOffsetOnce() {
  if (translated_for_layer_offset_ || layer_offset_ == gfx::Vector2dF())
    return;

  result_.StartPaint();
  push<cc::SaveOp>();
  push<cc::TranslateOp>(-layer_offset_.x(), -layer_offset_.y());
  result_.EndPaintOfPairedBegin();
  translated_for_layer_offset_ = true;
}

template <typename Result>
void ConversionContext<Result>::SwitchToChunkState(const PaintChunk& chunk) {
  TranslateForLayerOffsetOnce();
  chunk_to_layer_mapper_.SwitchToChunk(chunk);

  const auto& chunk_state = chunk.properties;
  SwitchToEffect(chunk_state.Effect().Unalias());
  SwitchToClip(chunk_state.Clip().Unalias());
  SwitchToTransform(chunk_state.Transform().Unalias());
}

// Tries to combine a clip node's clip rect into |combined_clip_rect|.
// Returns whether the clip has been combined.
static bool CombineClip(const ClipPaintPropertyNode& clip,
                        FloatRoundedRect& combined_clip_rect) {
  if (clip.PixelMovingFilter())
    return true;

  // Don't combine into a clip with clip path.
  DCHECK(clip.Parent());
  if (clip.UnaliasedParent()->ClipPath())
    return false;

  // Don't combine clips in different transform spaces.
  const auto& transform_space = clip.LocalTransformSpace().Unalias();
  const auto& parent_transform_space =
      clip.UnaliasedParent()->LocalTransformSpace().Unalias();
  if (&transform_space != &parent_transform_space &&
      (transform_space.Parent() != &parent_transform_space ||
       !transform_space.IsIdentity()))
    return false;

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
void ConversionContext<Result>::SwitchToClip(
    const ClipPaintPropertyNode& target_clip) {
  if (&target_clip == current_clip_)
    return;

  // Step 1: Exit all clips until the lowest common ancestor is found.
  const auto* lca_clip =
      &target_clip.LowestCommonAncestor(*current_clip_).Unalias();
  while (current_clip_ != lca_clip) {
    if (!state_stack_.size() || !state_stack_.back().IsClip()) {
      // TODO(crbug.com/803649): We still have clip hierarchy issues with
      // fragment clips. See crbug.com/1240080 for the test case. Will change
      // the above condition to DCHECK after LayoutNGBlockFragmentation is fully
      // launched.
#if DCHECK_IS_ON()
      DLOG(ERROR) << "Error: Chunk has a clip that escaped its layer's or "
                  << "effect's clip.\ntarget_clip:\n"
                  << target_clip.ToTreeString().Utf8() << "current_clip_:\n"
                  << current_clip_->ToTreeString().Utf8();
#endif
      break;
    }
    DCHECK(current_clip_->Parent());
    current_clip_ = &current_clip_->Parent()->Unalias();
    StateEntry& previous_state = state_stack_.back();
    if (current_clip_ == lca_clip) {
      // |lca_clip| is an intermediate clip in a series of combined clips.
      // Jump to the first of the combined clips.
      current_clip_ = lca_clip = previous_state.clip;
    }
    if (current_clip_ == previous_state.clip)
      EndClip();
  }

  if (&target_clip == current_clip_)
    return;

  // Step 2: Collect all clips between the target clip and the current clip.
  // At this point the current clip must be an ancestor of the target.
  Vector<const ClipPaintPropertyNode*, 1u> pending_clips;
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
  const auto* lowest_combined_clip_node = pending_clips.back();
  for (auto i = pending_clips.size() - 1; i--;) {
    const auto* sub_clip = pending_clips[i];
    if (CombineClip(*sub_clip, pending_combined_clip_rect)) {
      // Continue to combine.
      lowest_combined_clip_node = sub_clip;
    } else {
      // |sub_clip| can't be combined to previous clips. Output the current
      // combined clip, and start new combination.
      StartClip(pending_combined_clip_rect, *lowest_combined_clip_node);
      pending_combined_clip_rect = sub_clip->PaintClipRect();
      lowest_combined_clip_node = sub_clip;
    }
  }
  StartClip(pending_combined_clip_rect, *lowest_combined_clip_node);

  DCHECK_EQ(current_clip_, &target_clip);
}

template <typename Result>
void ConversionContext<Result>::StartClip(
    const FloatRoundedRect& combined_clip_rect,
    const ClipPaintPropertyNode& lowest_combined_clip_node) {
  if (combined_clip_rect.Rect() == gfx::RectF(LayoutRect::InfiniteIntRect())) {
    PushState(StateEntry::kClipOmitted);
  } else {
    const auto& local_transform =
        lowest_combined_clip_node.LocalTransformSpace().Unalias();
    if (&local_transform != current_transform_)
      EndTransform();
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
void ConversionContext<Result>::SwitchToEffect(
    const EffectPaintPropertyNode& target_effect) {
  if (&target_effect == current_effect_)
    return;

  // Step 1: Exit all effects until the lowest common ancestor is found.
  const auto& lca_effect =
      target_effect.LowestCommonAncestor(*current_effect_).Unalias();

#if DCHECK_IS_ON()
  bool has_effect_hierarchy_issue = false;
#endif

  while (current_effect_ != &lca_effect) {
    // This EndClips() and the later EndEffect() pop to the parent effect.
    EndClips();
    if (!state_stack_.size()) {
      // TODO(crbug.com/803649): We still have clip hierarchy issues with
      // fragment clips. See crbug.com/1240080 for the test case. Will change
      // the above condition to DCHECK after LayoutNGBlockFragmentation is fully
      // launched.
#if DCHECK_IS_ON()
      DLOG(ERROR) << "Error: Chunk has an effect that escapes layer's effect.\n"
                  << "target_effect:\n"
                  << target_effect.ToTreeString().Utf8() << "current_effect_:\n"
                  << current_effect_->ToTreeString().Utf8();
      has_effect_hierarchy_issue = true;
#endif
      // In pre-CompositeAfterPaint, we may squash one layer into another, but
      // the squashing layer may create more effect nodes not for real effects,
      // causing squashed layer's effect to escape the squashing layer's effect.
      // We can continue because the extra effects are noop.
      if (!HasRealEffects(*current_effect_, lca_effect))
        break;
      return;
    }
    EndEffect();
  }

  // Step 2: Collect all effects between the target effect and the current
  // effect. At this point the current effect must be an ancestor of the target.
  Vector<const EffectPaintPropertyNode*, 1u> pending_effects;
  for (const auto* effect = &target_effect; effect != &lca_effect;
       effect = effect->UnaliasedParent()) {
    // This should never happen unless the DCHECK in step 1 failed.
    if (!effect)
      break;
    pending_effects.push_back(effect);
  }

  // Step 3: Now apply the list of effects in top-down order.
  for (auto i = pending_effects.size(); i--;) {
    const EffectPaintPropertyNode* sub_effect = pending_effects[i];
#if DCHECK_IS_ON()
    if (!has_effect_hierarchy_issue)
      DCHECK_EQ(current_effect_, sub_effect->UnaliasedParent());
#endif
    StartEffect(*sub_effect);
#if DCHECK_IS_ON()
    state_stack_.back().has_pre_cap_effect_hierarchy_issue =
        has_effect_hierarchy_issue;
    // This applies only to the first new effect.
    has_effect_hierarchy_issue = false;
#endif
  }
}

template <typename Result>
void ConversionContext<Result>::StartEffect(
    const EffectPaintPropertyNode& effect) {
  // Before each effect can be applied, we must enter its output clip first,
  // or exit all clips if it doesn't have one.
  if (effect.OutputClip())
    SwitchToClip(effect.OutputClip()->Unalias());
  else
    EndClips();

  size_t save_layer_id = kNotFound;

  // Adjust transform first. Though a non-filter effect itself doesn't depend on
  // the transform, switching to the target transform before SaveLayer[Alpha]Op
  // will help the rasterizer optimize a non-filter SaveLayer[Alpha]Op/
  // DrawRecord/Restore sequence into a single DrawRecord which is much faster.
  // This also avoids multiple Save/Concat/.../Restore pairs for multiple
  // consecutive effects in the same transform space, by issuing only one pair
  // around all of the effects.
  SwitchToTransform(effect.LocalTransformSpace().Unalias());

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
    // The size parameter is only used to computed the origin of zoom
    // operation, which we never generate.
    gfx::SizeF empty;
    cc::PaintFlags filter_flags;
    filter_flags.setImageFilter(cc::RenderSurfaceFilters::BuildImageFilter(
        effect.Filter().AsCcFilterOperations(), empty));
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
  if (!previous_state.has_pre_cap_effect_hierarchy_issue)
    DCHECK_EQ(current_effect_->UnaliasedParent(), previous_state.effect);
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
void ConversionContext<Result>::EndClips() {
  while (state_stack_.size() && state_stack_.back().IsClip())
    EndClip();
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
void ConversionContext<Result>::SwitchToTransform(
    const TransformPaintPropertyNode& target_transform) {
  if (&target_transform == current_transform_)
    return;

  EndTransform();
  if (&target_transform == current_transform_)
    return;

  gfx::Transform projection = TargetToCurrentProjection(target_transform);
  if (projection.IsIdentity())
    return;

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

template <typename Result>
void ConversionContext<Result>::Convert(const PaintChunkSubset& chunks,
                                        const gfx::Rect* additional_cull_rect) {
  for (auto it = chunks.begin(); it != chunks.end(); ++it) {
    const auto& chunk = *it;
    if (chunk.effectively_invisible)
      continue;
    const auto& chunk_state = chunk.properties;
    bool switched_to_chunk_state = false;

    for (const auto& item : it.DisplayItems()) {
      PaintRecord record;
      if (auto* scrollbar = DynamicTo<ScrollbarDisplayItem>(item))
        record = scrollbar->Paint();
      else if (auto* drawing = DynamicTo<DrawingDisplayItem>(item))
        record = drawing->GetPaintRecord();
      else
        continue;

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

      // `SwitchToChunkState` will also update `chunk_to_layer_mapper_`'s chunk
      // but we need to explicitly switch the state ahead of time to ensure the
      // call to `chunk_to_layer_mapper_.MapVisualRect` uses the correct state.
      if (!switched_to_chunk_state) {
        chunk_to_layer_mapper_.SwitchToChunk(chunk);
      }

      gfx::Rect visual_rect =
          chunk_to_layer_mapper_.MapVisualRect(item.VisualRect());
      if (additional_cull_rect && can_ignore_record &&
          !additional_cull_rect->Intersects(visual_rect)) {
        continue;
      }

      if (!switched_to_chunk_state) {
        SwitchToChunkState(chunk);
        switched_to_chunk_state = true;
      }

      result_.StartPaint();
      if (!record.empty()) {
        push<cc::DrawRecordOp>(std::move(record));
      }
      result_.EndPaintOfUnpaired(visual_rect);
    }

    // If we have an empty paint chunk, then we would prefer ignoring it.
    // However, a reference filter can generate visible effect from invisible
    // source, and we need to emit paint operations for it.
    if (!switched_to_chunk_state && &chunk_state.Effect() != current_effect_)
      SwitchToChunkState(chunk);

    // Most effects apply to drawable contents only. Reference filters are
    // exceptions, for which we have already added the chunk bounds mapped
    // through the filter to the bounds of the effect in StartEffect().
    UpdateEffectBounds(gfx::RectF(chunk.drawable_bounds),
                       chunk_state.Transform().Unalias());
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

static void UpdateTouchActionRegion(
    const HitTestData& hit_test_data,
    const PropertyTreeState& layer_state,
    const PropertyTreeState& chunk_state,
    const gfx::Vector2dF& layer_offset,
    cc::TouchActionRegion& touch_action_region) {
  // If the element has an horizontal scrollable ancestor (including itself), we
  // need to disable cursor control by setting the bit kInternalPanXScrolls.
  TouchAction disable_cursor_control = TouchAction::kNone;
  // TODO(input-dev): Consider to share the code with
  // ThreadedInputHandler::FindNodeToLatch.
  for (const auto* scroll_node = chunk_state.Transform().ScrollNode();
       scroll_node; scroll_node = scroll_node->Parent()) {
    if (scroll_node->UserScrollableHorizontal() &&
        scroll_node->ContainerRect().width() <
            scroll_node->ContentsRect().width()) {
      disable_cursor_control = TouchAction::kInternalPanXScrolls;
      break;
    }
    // If it is not kAuto, scroll can't propagate, so break here.
    if (scroll_node->OverscrollBehaviorX() !=
        cc::OverscrollBehavior::Type::kAuto)
      break;
  }

  for (const auto& touch_action_rect : hit_test_data.touch_action_rects) {
    FloatClipRect rect(gfx::RectF(touch_action_rect.rect));
    if (!GeometryMapper::LocalToAncestorVisualRect(chunk_state, layer_state,
                                                   rect)) {
      continue;
    }
    rect.Move(-layer_offset);
    TouchAction touch_action = touch_action_rect.allowed_touch_action;
    if ((touch_action & TouchAction::kPanX) != TouchAction::kNone)
      touch_action |= disable_cursor_control;
    touch_action_region.Union(touch_action, gfx::ToEnclosingRect(rect.Rect()));
  }
}

static void UpdateWheelEventRegion(const HitTestData& hit_test_data,
                                   const PropertyTreeState& layer_state,
                                   const PropertyTreeState& chunk_state,
                                   const gfx::Vector2dF& layer_offset,
                                   cc::Region& wheel_event_region) {
  for (const auto& wheel_event_rect : hit_test_data.wheel_event_rects) {
    FloatClipRect rect((gfx::RectF(wheel_event_rect)));
    if (!GeometryMapper::LocalToAncestorVisualRect(chunk_state, layer_state,
                                                   rect)) {
      continue;
    }
    rect.Move(-layer_offset);
    wheel_event_region.Union(gfx::ToEnclosingRect(rect.Rect()));
  }
}

static void UpdateNonFastScrollableRegion(
    cc::Layer& layer,
    const HitTestData& hit_test_data,
    const PropertyTreeState& layer_state,
    const PropertyTreeState& chunk_state,
    const gfx::Vector2dF& layer_offset,
    cc::Region& non_fast_scrollable_region) {
  if (hit_test_data.scroll_hit_test_rect.IsEmpty())
    return;

  // Skip the scroll hit test rect if it is for scrolling this cc::Layer.
  if (const auto scroll_translation = hit_test_data.scroll_translation) {
    const auto* scroll_node = scroll_translation->ScrollNode();
    DCHECK(scroll_node);
    // TODO(crbug.com/1222613): Remove this when we fix the root cause.
    if (!scroll_node)
      return;
    auto scroll_element_id = scroll_node->GetCompositorElementId();
    if (layer.element_id() == scroll_element_id)
      return;
  }

  FloatClipRect rect(gfx::RectF(hit_test_data.scroll_hit_test_rect));
  if (!GeometryMapper::LocalToAncestorVisualRect(chunk_state, layer_state,
                                                 rect))
    return;

  rect.Move(-layer_offset);
  non_fast_scrollable_region.Union(gfx::ToEnclosingRect(rect.Rect()));
}

static void UpdateTouchActionWheelEventHandlerAndNonFastScrollableRegions(
    cc::Layer& layer,
    const PropertyTreeState& layer_state,
    const PaintChunkSubset& chunks) {
  gfx::Vector2dF layer_offset = layer.offset_to_transform_parent();
  cc::TouchActionRegion touch_action_region;
  cc::Region wheel_event_region;
  cc::Region non_fast_scrollable_region;
  for (const auto& chunk : chunks) {
    if (!chunk.hit_test_data)
      continue;
    auto chunk_state = chunk.properties.GetPropertyTreeState().Unalias();
    UpdateTouchActionRegion(*chunk.hit_test_data, layer_state, chunk_state,
                            layer_offset, touch_action_region);
    UpdateWheelEventRegion(*chunk.hit_test_data, layer_state, chunk_state,
                           layer_offset, wheel_event_region);
    UpdateNonFastScrollableRegion(layer, *chunk.hit_test_data, layer_state,
                                  chunk_state, layer_offset,
                                  non_fast_scrollable_region);
  }
  layer.SetTouchActionRegion(std::move(touch_action_region));
  layer.SetWheelEventRegion(std::move(wheel_event_region));
  layer.SetNonFastScrollableRegion(std::move(non_fast_scrollable_region));
}

static void UpdateRegionCaptureData(cc::Layer& layer,
                                    const PropertyTreeState& layer_state,
                                    const PaintChunkSubset& chunks) {
  const gfx::Vector2dF layer_offset = layer.offset_to_transform_parent();
  viz::RegionCaptureBounds capture_bounds;
  for (const PaintChunk& chunk : chunks) {
    if (!chunk.region_capture_data)
      continue;

    const PropertyTreeState chunk_state =
        chunk.properties.GetPropertyTreeState().Unalias();
    for (const std::pair<RegionCaptureCropId, gfx::Rect>& pair :
         *chunk.region_capture_data) {
      FloatClipRect rect(gfx::RectF(pair.second));
      if (!GeometryMapper::LocalToAncestorVisualRect(chunk_state, layer_state,
                                                     rect))
        continue;

      rect.Move(-layer_offset);
      capture_bounds.Set(pair.first.value(), gfx::ToEnclosingRect(rect.Rect()));
    }
  }

  // At this point, the bounds are in the coordinate space of
  // the layer we are adding them to.
  layer.SetCaptureBounds(std::move(capture_bounds));
}

static gfx::Point MapSelectionBoundPoint(const gfx::Point& point,
                                         const PropertyTreeState& layer_state,
                                         const PropertyTreeState& chunk_state,
                                         const gfx::Vector2dF& layer_offset) {
  gfx::PointF mapped_point =
      GeometryMapper::SourceToDestinationProjection(chunk_state.Transform(),
                                                    layer_state.Transform())
          .MapPoint(gfx::PointF(point));

  mapped_point -= layer_offset;
  return gfx::ToRoundedPoint(mapped_point);
}

static cc::LayerSelectionBound
ConvertPaintedSelectionBoundToLayerSelectionBound(
    const PaintedSelectionBound& bound,
    const PropertyTreeState& layer_state,
    const PropertyTreeState& chunk_state,
    const gfx::Vector2dF& layer_offset) {
  cc::LayerSelectionBound layer_bound;
  layer_bound.type = bound.type;
  layer_bound.hidden = bound.hidden;
  layer_bound.edge_start = MapSelectionBoundPoint(bound.edge_start, layer_state,
                                                  chunk_state, layer_offset);
  layer_bound.edge_end = MapSelectionBoundPoint(bound.edge_end, layer_state,
                                                chunk_state, layer_offset);
  return layer_bound;
}

bool PaintChunksToCcLayer::UpdateLayerSelection(
    cc::Layer& layer,
    const PropertyTreeState& layer_state,
    const PaintChunkSubset& chunks,
    cc::LayerSelection& layer_selection) {
  gfx::Vector2dF layer_offset = layer.offset_to_transform_parent();
  bool any_selection_was_painted = false;
  for (const auto& chunk : chunks) {
    if (!chunk.layer_selection_data)
      continue;

    any_selection_was_painted |=
        chunk.layer_selection_data->any_selection_was_painted;

    auto chunk_state = chunk.properties.GetPropertyTreeState().Unalias();
    if (chunk.layer_selection_data->start) {
      const PaintedSelectionBound& bound =
          chunk.layer_selection_data->start.value();
      layer_selection.start = ConvertPaintedSelectionBoundToLayerSelectionBound(
          bound, layer_state, chunk_state, layer_offset);
      layer_selection.start.layer_id = layer.id();
    }

    if (chunk.layer_selection_data->end) {
      const PaintedSelectionBound& bound =
          chunk.layer_selection_data->end.value();
      layer_selection.end = ConvertPaintedSelectionBoundToLayerSelectionBound(
          bound, layer_state, chunk_state, layer_offset);
      layer_selection.end.layer_id = layer.id();
    }
  }

  return any_selection_was_painted;
}

void PaintChunksToCcLayer::UpdateLayerProperties(
    cc::Layer& layer,
    const PropertyTreeState& layer_state,
    const PaintChunkSubset& chunks) {
  UpdateTouchActionWheelEventHandlerAndNonFastScrollableRegions(
      layer, layer_state, chunks);
  UpdateRegionCaptureData(layer, layer_state, chunks);
}

}  // namespace blink
