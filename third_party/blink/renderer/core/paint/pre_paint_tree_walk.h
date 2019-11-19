// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PRE_PAINT_TREE_WALK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PRE_PAINT_TREE_WALK_H_

#include "third_party/blink/renderer/core/paint/clip_rect.h"
#include "third_party/blink/renderer/core/paint/paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_builder.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutObject;
class LocalFrameView;

// This class walks the whole layout tree, beginning from the root
// LocalFrameView, across frame boundaries. Helper classes are called for each
// tree node to perform actual actions.  It expects to be invoked in InPrePaint
// phase.
class CORE_EXPORT PrePaintTreeWalk {
  DISALLOW_NEW();

 public:
  PrePaintTreeWalk() = default;
  void WalkTree(LocalFrameView& root_frame);

  static bool ObjectRequiresPrePaint(const LayoutObject&);
  static bool ObjectRequiresTreeBuilderContext(const LayoutObject&);

 private:
  friend PaintInvalidatorContext::ParentContextAccessor;

  // PrePaintTreewalkContext is large and can lead to stack overflows
  // when recursion is deep so these context objects are allocated on the heap.
  // See: https://crbug.com/698653.
  struct PrePaintTreeWalkContext {
    PrePaintTreeWalkContext() { tree_builder_context.emplace(); }
    PrePaintTreeWalkContext(
        const PrePaintTreeWalkContext& parent_context,
        const PaintInvalidatorContext::ParentContextAccessor&
            parent_context_accessor,
        bool needs_tree_builder_context)
        : paint_invalidator_context(parent_context_accessor),
          ancestor_overflow_paint_layer(
              parent_context.ancestor_overflow_paint_layer),
          inside_blocking_touch_event_handler(
              parent_context.inside_blocking_touch_event_handler),
          effective_allowed_touch_action_changed(
              parent_context.effective_allowed_touch_action_changed),
          clip_changed(parent_context.clip_changed) {
      if (needs_tree_builder_context || DCHECK_IS_ON()) {
        DCHECK(parent_context.tree_builder_context);
        tree_builder_context.emplace(*parent_context.tree_builder_context);
      }
#if DCHECK_IS_ON()
      if (needs_tree_builder_context)
        DCHECK(parent_context.tree_builder_context->is_actually_needed);
      tree_builder_context->is_actually_needed = needs_tree_builder_context;
#endif
    }

    base::Optional<PaintPropertyTreeBuilderContext> tree_builder_context;
    PaintInvalidatorContext paint_invalidator_context;

    // The ancestor in the PaintLayer tree which has overflow clip, or
    // is the root layer. Note that it is tree ancestor, not containing
    // block or stacking ancestor.
    PaintLayer* ancestor_overflow_paint_layer = nullptr;

    // Whether there is a blocking touch event handler on any ancestor.
    bool inside_blocking_touch_event_handler = false;

    // When the effective allowed touch action changes on an ancestor, the
    // entire subtree may need to update.
    bool effective_allowed_touch_action_changed = false;

    // This is set to true once we see tree_builder_context->clip_changed is
    // true. It will be propagated to descendant contexts even if we don't
    // create tree_builder_context.
    bool clip_changed = false;
  };

  static bool ContextRequiresPrePaint(const PrePaintTreeWalkContext&);
  static bool ContextRequiresTreeBuilderContext(const PrePaintTreeWalkContext&,
                                                const LayoutObject&);

  void CheckTreeBuilderContextState(const LayoutObject&,
                                    const PrePaintTreeWalkContext&);

  const PrePaintTreeWalkContext& ContextAt(wtf_size_t index) {
    DCHECK_LT(index, context_storage_.size());
    return context_storage_[index];
  }

  void Walk(LocalFrameView&);

  // This is to minimize stack frame usage during recursion. Modern compilers
  // (MSVC in particular) can inline across compilation units, resulting in
  // very big stack frames. Splitting the heavy lifting to a separate function
  // makes sure the stack frame is freed prior to making a recursive call.
  // See https://crbug.com/781301 .
  NOINLINE void WalkInternal(const LayoutObject&, PrePaintTreeWalkContext&);
  void Walk(const LayoutObject&);

  bool NeedsTreeBuilderContextUpdate(const LocalFrameView&,
                                     const PrePaintTreeWalkContext&);
  void UpdateAuxiliaryObjectProperties(const LayoutObject&,
                                       PrePaintTreeWalkContext&);

  bool NeedsEffectiveAllowedTouchActionUpdate(const LayoutObject&,
                                              PrePaintTreeWalkContext&) const;
  // Updates |LayoutObject::InsideBlockingTouchEventHandler|. Also ensures
  // |PrePaintTreeWalkContext.effective_allowed_touch_action_changed| is set
  // which will ensure the subtree is updated too.
  void UpdateEffectiveAllowedTouchAction(const LayoutObject&,
                                         PrePaintTreeWalkContext&);
  void InvalidatePaintForHitTesting(const LayoutObject&,
                                    PrePaintTreeWalkContext&);

  void ResizeContextStorageIfNeeded();

  PaintInvalidator paint_invalidator_;
  Vector<PrePaintTreeWalkContext> context_storage_;

  bool needs_invalidate_chrome_client_ = false;

  FRIEND_TEST_ALL_PREFIXES(PrePaintTreeWalkTest, ClipRects);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PRE_PAINT_TREE_WALK_H_
