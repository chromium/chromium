// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PRE_PAINT_TREE_WALK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PRE_PAINT_TREE_WALK_H_

#include "base/dcheck_is_on.h"
#include "base/gtest_prod_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_builder.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutObject;
class LocalFrameView;
struct NGLink;
class NGPhysicalBoxFragment;
class NGPhysicalFragment;

// This class walks the whole layout tree, beginning from the root
// LocalFrameView, across frame boundaries. Helper classes are called for each
// tree node to perform actual actions.  It expects to be invoked in InPrePaint
// phase.
class CORE_EXPORT PrePaintTreeWalk final {
  STACK_ALLOCATED();

 public:
  PrePaintTreeWalk() = default;
  void WalkTree(LocalFrameView& root_frame);

  static bool ObjectRequiresPrePaint(const LayoutObject&);
  static bool ObjectRequiresTreeBuilderContext(const LayoutObject&);

  // Keeps information about the parent fragment that we need to search inside
  // to find out-of-flow positioned descendants, and also which fragmentainer
  // we're inside (which will serve as a fragment ID in FragmentData).
  struct ContainingFragment {
    STACK_ALLOCATED();

   public:
    bool IsInFragmentationContext() const;

    const NGPhysicalBoxFragment* fragment = nullptr;
    wtf_size_t fragmentainer_idx = WTF::kNotFound;
    int fragmentation_nesting_level = 0;
  };

  // This provides a default base copy constructor for PrePaintTreeWalkContext.
  // It contains all fields except for tree_builder_context which needs special
  // treatment in the copy constructor.
  struct PrePaintTreeWalkContextBase {
    STACK_ALLOCATED();

   protected:
    PrePaintTreeWalkContextBase() = default;
    PrePaintTreeWalkContextBase(const PrePaintTreeWalkContextBase&) = default;

   public:
    // Reset fragmentation when entering something that shouldn't be affected by
    // the current fragmentation context(s).
    void ResetFragmentation() {
      current_container = {};
      absolute_positioned_container = {};
      fixed_positioned_container = {};
    }

    PaintInvalidatorContext paint_invalidator_context;

    // The ancestor in the PaintLayer tree which is a scroll container. Note
    // that it is tree ancestor, not containing block or stacking ancestor.
    PaintLayer* ancestor_scroll_container_paint_layer = nullptr;

    // Whether there is a blocking touch event handler on any ancestor.
    bool inside_blocking_touch_event_handler = false;

    // When the effective allowed touch action changes on an ancestor, the
    // entire subtree may need to update.
    bool effective_allowed_touch_action_changed = false;

    // Whether there is a blocking wheel event handler on any ancestor.
    bool inside_blocking_wheel_event_handler = false;

    // When the blocking wheel event handlers change on an ancestor, the entire
    // subtree may need to update.
    bool blocking_wheel_event_handler_changed = false;

    // True if we're visiting the parent for the first time, i.e. when we're in
    // the first fragmentainer where the parent occurs (or if we're not
    // fragmented at all).
    bool is_parent_first_for_node = true;

    ContainingFragment current_container;
    ContainingFragment absolute_positioned_container;
    ContainingFragment fixed_positioned_container;
  };

  struct PrePaintTreeWalkContext : public PrePaintTreeWalkContextBase {
    PrePaintTreeWalkContext() { tree_builder_context.emplace(); }
    PrePaintTreeWalkContext(const PrePaintTreeWalkContext& parent_context,
                            bool needs_tree_builder_context)
        : PrePaintTreeWalkContextBase(parent_context) {
      if (needs_tree_builder_context
#if DCHECK_IS_ON()
          || RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()
#endif
      ) {
        DCHECK(parent_context.tree_builder_context);
        tree_builder_context.emplace(*parent_context.tree_builder_context);
#if DCHECK_IS_ON()
        DCHECK(!needs_tree_builder_context ||
               parent_context.tree_builder_context->is_actually_needed);
        tree_builder_context->is_actually_needed = needs_tree_builder_context;
#endif
      }
    }

    PrePaintTreeWalkContext(const PrePaintTreeWalkContext&) = delete;
    PrePaintTreeWalkContext& operator=(const PrePaintTreeWalkContext&) = delete;

    bool NeedsTreeBuilderContext() const {
      return tree_builder_context.has_value()
#if DCHECK_IS_ON()
             && tree_builder_context->is_actually_needed
#endif
          ;
    }

    absl::optional<PaintPropertyTreeBuilderContext> tree_builder_context;
  };

  static bool ContextRequiresChildPrePaint(const PrePaintTreeWalkContext&);
  static bool ContextRequiresChildTreeBuilderContext(
      const PrePaintTreeWalkContext&);

#if DCHECK_IS_ON()
  void CheckTreeBuilderContextState(const LayoutObject&,
                                    const PrePaintTreeWalkContext&);
#endif

  // Upon entering a child LayoutObject, create an NGPrePaintInfo, and populate
  // everything except its FragmentData. We need to get a bit further inside the
  // child (WalkInternal()) before we can set up FragmentData (if we get there
  // at all).
  NGPrePaintInfo CreatePrePaintInfo(const NGLink& child,
                                    const PrePaintTreeWalkContext& context);

  // Locate and/or set up a FragmentData object for the current object /
  // physical fragment.
  FragmentData* GetOrCreateFragmentData(const LayoutObject&,
                                        const PrePaintTreeWalkContext&,
                                        const NGPrePaintInfo&);

  void UpdateContextForOOFContainer(const LayoutObject&,
                                    PrePaintTreeWalkContext&,
                                    const NGPhysicalBoxFragment*);

  void Walk(LocalFrameView&, const PrePaintTreeWalkContext& parent_context);

  // This is to minimize stack frame usage during recursion. Modern compilers
  // (MSVC in particular) can inline across compilation units, resulting in
  // very big stack frames. Splitting the heavy lifting to a separate function
  // makes sure the stack frame is freed prior to making a recursive call.
  // See https://crbug.com/781301 .
  NOINLINE void WalkInternal(const LayoutObject&,
                             PrePaintTreeWalkContext&,
                             NGPrePaintInfo*);

  // Add any "missable" children to a list. Missable children are children that
  // we might not find during LayoutObject traversal. This happens when an
  // ancestor LayoutObject (of the missable child) has no fragment inside a
  // given fragmentainer, e.g. when there's an OOF fragment, but its containing
  // block has no fragment inside that fragmentainer. Later, during the child
  // walk, when a missable child is actually walked, it's removed from the
  // list.
  //
  // Returns true if there are any missable children inside the fragment, false
  // otherwise.
  bool CollectMissableChildren(PrePaintTreeWalkContext&,
                               const NGPhysicalBoxFragment&);

  // Based on the context established by |ancestor|, modify it to become correct
  // for |object|, at least as far as OOF containing block info is concerned.
  void RebuildContextForMissedDescendant(const LayoutObject& ancestor,
                                         const LayoutObject& object,
                                         PrePaintTreeWalkContext&);

  // Walk any missed children (i.e. those collected by CollectMissableChildren()
  // and not walked by Walk()) after child object traversal.
  void WalkMissedChildren(const LayoutObject& ancestor,
                          const NGPhysicalBoxFragment&,
                          const PrePaintTreeWalkContext&);

  void WalkFragmentationContextRootChildren(const LayoutObject&,
                                            const NGPhysicalBoxFragment&,
                                            const PrePaintTreeWalkContext&);
  void WalkLayoutObjectChildren(const LayoutObject&,
                                const NGPhysicalBoxFragment*,
                                const PrePaintTreeWalkContext&);
  void WalkChildren(const LayoutObject&,
                    const NGPhysicalBoxFragment*,
                    PrePaintTreeWalkContext&,
                    bool is_inside_fragment_child = false);
  void Walk(const LayoutObject&,
            const PrePaintTreeWalkContext& parent_context,
            NGPrePaintInfo*);

  bool NeedsTreeBuilderContextUpdate(const LocalFrameView&,
                                     const PrePaintTreeWalkContext&);
  bool NeedsTreeBuilderContextUpdate(const LayoutObject&,
                                     const PrePaintTreeWalkContext&);
  // Updates |LayoutObject::InsideBlockingTouchEventHandler|. Also ensures
  // |PrePaintTreeWalkContext.effective_allowed_touch_action_changed| is set
  // which will ensure the subtree is updated too.
  void UpdateEffectiveAllowedTouchAction(const LayoutObject&,
                                         PrePaintTreeWalkContext&);
  // Updates |LayoutObject::InsideBlockingWheelEventHandler|. Also ensures
  // |PrePaintTreeWalkContext.blocking_wheel_event_handler_changed| is set
  // which will ensure the subtree is updated too.
  void UpdateBlockingWheelEventHandler(const LayoutObject&,
                                       PrePaintTreeWalkContext&);
  void InvalidatePaintForHitTesting(const LayoutObject&,
                                    PrePaintTreeWalkContext&);

  PaintInvalidator paint_invalidator_;

  // List of fragments that may be missed during LayoutObject walking. See
  // CollectMissableChildren() and WalkMissedChildren().
  HeapHashSet<Member<const NGPhysicalFragment>> pending_missables_;

  bool needs_invalidate_chrome_client_ = false;

  FRIEND_TEST_ALL_PREFIXES(PrePaintTreeWalkTest, ClipRects);
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::PrePaintTreeWalk::PrePaintTreeWalkContext)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PRE_PAINT_TREE_WALK_H_
