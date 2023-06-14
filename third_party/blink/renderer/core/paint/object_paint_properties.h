// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINT_PROPERTIES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINT_PROPERTIES_H_

#include <memory>

#include "base/dcheck_is_on.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// This interface is for storing the paint property nodes created by a
// LayoutObject. The object owns each of the property nodes directly and RefPtrs
// are only used to harden against use-after-free bugs. These paint properties
// are built/updated by PaintPropertyTreeBuilder during the PrePaint lifecycle
// step.
//
// [update & clear implementation note] This class has Update[property](...) and
// Clear[property]() helper functions for efficiently creating and updating
// properties. The update functions returns a 3-state result to indicate whether
// the value or the existence of the node has changed. They use a create-or-
// update pattern of re-using existing properties for efficiency:
// 1. It avoids extra allocations.
// 2. It preserves existing child->parent pointers.
// The clear functions return true if an existing node is removed. Property
// nodes store parent pointers but not child pointers and these return values
// are important for catching property tree structure changes which require
// updating descendant's parent pointers.
class CORE_EXPORT ObjectPaintProperties {
  USING_FAST_MALLOC(ObjectPaintProperties);

 public:
  virtual ~ObjectPaintProperties();
  static std::unique_ptr<ObjectPaintProperties> Create();

// Preprocessor macro declarations.
//
// The following defines 3 functions and one variable:
// - Foo(): a getter for the property.
// - UpdateFoo(): an update function.
// - ClearFoo(): a clear function
// - foo_: the variable itself.
//
// Note that clear* functions return true if the property tree structure
// changes (an existing node was deleted), and false otherwise. See the
// class-level comment ("update & clear implementation note") for details
// about why this is needed for efficient updates.
#define ADD_NODE_DECL(type, function)                                  \
  virtual const type##PaintPropertyNode* function() const = 0;         \
  virtual PaintPropertyChangeType Update##function(                    \
      const type##PaintPropertyNodeOrAlias& parent,                    \
      type##PaintPropertyNode::State&& state,                          \
      const type##PaintPropertyNode::AnimationState& animation_state = \
          type##PaintPropertyNode::AnimationState()) = 0;              \
  virtual bool Clear##function() = 0  // (End of ADD_NODE_DECL definition)

#define ADD_ALIAS_NODE_DECL(type, function)                           \
  virtual const type##PaintPropertyNodeOrAlias* function() const = 0; \
  virtual PaintPropertyChangeType Update##function(                   \
      const type##PaintPropertyNodeOrAlias& parent) = 0;              \
  virtual bool Clear##function() = 0  // (End of ADD_ALIAS_NODE_DECL definition)

#define ADD_TRANSFORM_DECL(function) ADD_NODE_DECL(Transform, function)
#define ADD_EFFECT_DECL(function) ADD_NODE_DECL(Effect, function)
#define ADD_CLIP_DECL(function) ADD_NODE_DECL(Clip, function)

  // Transform node method declarations.
  //
  // The hierarchy of the transform subtree created by a LayoutObject is as
  // follows:
  // [ PaintOffsetTranslation ]
  // |   Normally paint offset is accumulated without creating a node until
  // |   we see, for example, transform or position:fixed.
  // +-[ StickyTranslation ]
  //  /    This applies the sticky offset induced by position:sticky.
  // |
  // +-[ AnchorScrollTranslation ]
  //  /    This applies the scrolling offset induced by CSS anchor-scroll.
  // |
  // +-[ Translate ]
  //   |   The transform from CSS 'translate' (including the effects of
  //  /    'transform-origin').
  // |
  // +-[ Rotate ]
  //   |   The transform from CSS 'rotate' (including the effects of
  //  /    'transform-origin').
  // |
  // +-[ Scale ]
  //   |   The transform from CSS 'scale' (including the effects of
  //  /    'transform-origin').
  // |
  // +-[ Offset ]
  //   |   The transform from the longhand properties that comprise the CSS
  //  /    'offset' shorthand (including the effects of 'transform-origin').
  // |
  // +-[ Transform ]
  //   |   The transform from CSS 'transform' (including the effects of
  //   |   'transform-origin').
  //   |
  //   |   For SVG, this also includes 'translate', 'rotate', 'scale',
  //   |   'offset-*' (instead of the nodes above) and the effects of
  //   |   some characteristics of the SVG viewport and the "SVG
  //   |   additional translation" (for the x and y attributes on
  //   |   svg:use).
  //   |
  //   |   This is the local border box space (see
  //   |   FragmentData::LocalBorderBoxProperties); the nodes below influence
  //   |   the transform for the children but not the LayoutObject itself.
  //   |
  //   +-[ Perspective ]
  //     |   The space created by CSS perspective.
  //     +-[ ReplacedContentTransform ]
  //         Additional transform for replaced elements to implement object-fit.
  //         (Replaced elements don't scroll.)
  //     OR
  //     +-[ ScrollTranslation ]
  //         The space created by overflow clip. The translation equals the
  //         offset between the scrolling contents and the scrollable area of
  //         the container, both originated from the top-left corner, so it is
  //         the scroll position (instead of scroll offset) of the
  //         ScrollableArea.
  //
  // ... +-[ TransformIsolationNode ]
  //         This serves as a parent to subtree transforms on an element with
  //         paint containment. It induces a PaintOffsetTranslation node and
  //         is the deepest child of any transform tree on the contain: paint
  //         element.
  //
  // This hierarchy is related to the order of transform operations in
  // https://drafts.csswg.org/css-transforms-2/#accumulated-3d-transformation-matrix-computation
  virtual bool HasTransformNode() const = 0;
  virtual bool HasCSSTransformPropertyNode() const = 0;
  virtual std::array<const TransformPaintPropertyNode*, 5>
  AllCSSTransformPropertiesOutsideToInside() const = 0;
  ADD_TRANSFORM_DECL(PaintOffsetTranslation);
  ADD_TRANSFORM_DECL(StickyTranslation);
  ADD_TRANSFORM_DECL(AnchorScrollTranslation);
  ADD_TRANSFORM_DECL(Translate);
  ADD_TRANSFORM_DECL(Rotate);
  ADD_TRANSFORM_DECL(Scale);
  ADD_TRANSFORM_DECL(Offset);
  ADD_TRANSFORM_DECL(Transform);
  ADD_TRANSFORM_DECL(Perspective);
  ADD_TRANSFORM_DECL(ReplacedContentTransform);
  ADD_TRANSFORM_DECL(ScrollTranslation);
  using ScrollPaintPropertyNodeOrAlias = ScrollPaintPropertyNode;
  ADD_NODE_DECL(Scroll, Scroll);
  ADD_ALIAS_NODE_DECL(Transform, TransformIsolationNode);

  // Effect node method declarations.
  //
  // The hierarchy of the effect subtree created by a LayoutObject is as
  // follows:
  // [ Effect ]
  // |     Isolated group to apply various CSS effects, including opacity,
  // |     mix-blend-mode, backdrop-filter, and for isolation if a mask needs
  // |     to be applied or backdrop-dependent children are present.
  // +-[ Filter ]
  // |     Isolated group for CSS filter.
  // +-[ Mask ]
  // | |   Isolated group for painting the CSS mask or the mask-based CSS
  // | |   clip-path. This node will have SkBlendMode::kDstIn and shall paint
  // | |   last, i.e. after masked contents.
  // | +-[ ClipPathMask ]
  // |     Isolated group for painting the mask-based CSS clip-path. This node
  // |     will have SkBlendMode::kDstIn and shall paint last, i.e. after
  // |     clipped contents. If there is no Mask node, then this node is a
  // |     direct child of the Effect node.
  // +-[ VerticalScrollbarEffect / HorizontalScrollbarEffect / ScrollCorner ]
  //       Overlay Scrollbars on Aura and Android need effect node for fade
  //       animation. Also used in ViewTransitions to separate out scrollbars
  //       from the root snapshot.
  //
  // ... +-[ EffectIsolationNode ]
  //       This serves as a parent to subtree effects on an element with paint
  //       containment, It is the deepest child of any effect tree on the
  //       contain: paint element.
  virtual bool HasEffectNode() const = 0;
  ADD_EFFECT_DECL(Effect);
  ADD_EFFECT_DECL(Filter);
  ADD_EFFECT_DECL(VerticalScrollbarEffect);
  ADD_EFFECT_DECL(HorizontalScrollbarEffect);
  ADD_EFFECT_DECL(ScrollCornerEffect);
  ADD_EFFECT_DECL(Mask);
  ADD_EFFECT_DECL(ClipPathMask);
  ADD_ALIAS_NODE_DECL(Effect, EffectIsolationNode);

  // Clip node declarations.
  //
  // The hierarchy of the clip subtree created by a LayoutObject is as follows:
  // [ ViewTransitionClip ]
  // |   Clip created only when there is an active ViewTransition. This is used
  // |   to clip the element's painting to a subset close to the viewport.
  // |   See https://drafts.csswg.org/css-view-transitions-1/
  // |       #compute-the-interest-rectangle-algorithm for details.
  // +-[ ClipPathClip ]
  //   |  Clip created by path-based CSS clip-path. Only exists if the
  //  /   clip-path is "simple" that can be applied geometrically. This and
  // /    the ClipPathMask effect node are mutually exclusive.
  // +-[ MaskClip ]
  //   |   Clip created by CSS mask or mask-based CSS clip-path.
  //   |   It serves two purposes:
  //   |   1. Cull painting of the masked subtree. Because anything outside of
  //   |      the mask is never visible, it is pointless to paint them.
  //   |   2. Raster clip of the masked subtree. Because the mask implemented
  //   |      as SkBlendMode::kDstIn, pixels outside of mask's bound will be
  //   |      intact when they shall be masked out. This clip ensures no pixels
  //   |      leak out.
  //   +-[ CssClip ]
  //     |   Clip created by CSS clip. CSS clip applies to all descendants, this
  //     |   node only applies to containing block descendants. For descendants
  //     |   not contained by this object, use [ css clip fixed position ].
  //     +-[ OverflowControlsClip ]
  //     |   Clip created by overflow clip to clip overflow controls
  //     |   (scrollbars, resizer, scroll corner) that would overflow the box.
  //     +-[ BackgroundClip ]
  //     |   Clip created for CompositeBackgroundAttachmentFixed background
  //     |   according to CSS background-clip.
  //     +-[ PixelMovingFilterClipExpander ]
  //       | Clip created by pixel-moving filter. Instead of intersecting with
  //       | the current clip, this clip expands the current clip to include all
  //      /  pixels in the filtered content that may affect the pixels in the
  //     /   current clip.
  //     +-[ InnerBorderRadiusClip ]
  //       |   Clip created by a rounded border with overflow clip. This clip is
  //       |   not inset by scrollbars.
  //       +-[ OverflowClip ]
  //             Clip created by overflow clip and is inset by the scrollbar.
  //   [ CssClipFixedPosition ]
  //       Clip created by CSS clip. Only exists if the current clip includes
  //       some clip that doesn't apply to our fixed position descendants.
  //
  //  ... +-[ ClipIsolationNode ]
  //       This serves as a parent to subtree clips on an element with paint
  //       containment. It is the deepest child of any clip tree on the contain:
  //       paint element.
  virtual bool HasClipNode() const = 0;
  ADD_CLIP_DECL(PixelMovingFilterClipExpander);
  ADD_CLIP_DECL(ClipPathClip);
  ADD_CLIP_DECL(MaskClip);
  ADD_CLIP_DECL(CssClip);
  ADD_CLIP_DECL(CssClipFixedPosition);
  ADD_CLIP_DECL(OverflowControlsClip);
  ADD_CLIP_DECL(BackgroundClip);
  ADD_CLIP_DECL(InnerBorderRadiusClip);
  ADD_CLIP_DECL(OverflowClip);
  ADD_ALIAS_NODE_DECL(Clip, ClipIsolationNode);

#undef ADD_CLIP_DECL
#undef ADD_EFFECT_DECL
#undef ADD_TRANSFORM_DECL
#undef ADD_NODE_DECL
#undef ADD_ALIAS_NODE_DECL

// Debug-only state change validation method declarations.
//
// Used by find_properties_needing_update.h for verifying state doesn't
// change.
#if DCHECK_IS_ON()
  virtual void SetImmutable() const = 0;
  virtual bool IsImmutable() const = 0;
  virtual void SetMutable() const = 0;
  virtual void Validate() = 0;
#endif

  // Direct update method declarations.
  virtual PaintPropertyChangeType DirectlyUpdateTransformAndOrigin(
      TransformPaintPropertyNode::TransformAndOrigin&& transform_and_origin,
      const TransformPaintPropertyNode::AnimationState& animation_state) = 0;

  virtual PaintPropertyChangeType DirectlyUpdateOpacity(
      float opacity,
      const EffectPaintPropertyNode::AnimationState& animation_state) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINT_PROPERTIES_H_
