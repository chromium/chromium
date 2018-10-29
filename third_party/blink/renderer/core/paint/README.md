# `Source/core/paint`

This directory contains implementation of painters of layout objects. It covers
the following document lifecycle phases:

*   Layerization (`kInCompositingUpdate`, `kCompositingInputsClean` and `kCompositingClean`)
*   PaintInvalidation (`InPaintInvalidation` and `PaintInvalidationClean`)
*   PrePaint (`InPrePaint` and `PrePaintClean`)
*   Paint (`InPaint` and `PaintClean`)

## Glossaries

### Stacked elements and stacking contexts

This chapter is basically a clarification of [CSS 2.1 appendix E. Elaborate
description of Stacking Contexts](http://www.w3.org/TR/CSS21/zindex.html).

Note: we use 'element' instead of 'object' in this chapter to keep consistency
with the spec. We use 'object' in other places in this document.

According to the documentation, we can have the following types of elements that
are treated in different ways during painting:

*   Stacked objects: objects that are z-ordered in stacking contexts, including:

    *   Stacking contexts: elements with non-auto z-indices or other properties
        that affect stacking e.g. transform, opacity, blend-mode.

    *   Replaced normal-flow stacking elements: [replaced elements](https://html.spec.whatwg.org/multipage/rendering.html#replaced-elements)
        that do not have non-auto z-index but are stacking contexts for
        elements below them. Right now the only example is SVG <foreignObject>.
        The difference between these elements and regular stacking contexts is
        that they paint in the foreground phase of the painting algorithm
        (as opposed to the positioned descendants phase).

    *   Elements that are not real stacking contexts but are treated as stacking
        contexts but don't manage other stacked elements. Their z-ordering are
        managed by real stacking contexts. They are positioned elements with
        `z-index: auto` (E.2.8 in the documentation).

        They must be managed by the enclosing stacking context as stacked
        elements because `z-index:auto` and `z-index:0` are considered equal for
        stacking context sorting and they may interleave by DOM order.

        The difference of a stacked element of this type from a real stacking
        context is that it doesn't manage z-ordering of stacked descendants.
        These descendants are managed by the parent stacking context of this
        stacked element.

    "Stacked element" is not defined as a formal term in the documentation, but
    we found it convenient to use this term to refer to any elements
    participating z-index ordering in stacking contexts.

    A stacked element is represented by a `PaintLayerStackingNode` associated
    with a `PaintLayer`. It's painted as self-painting `PaintLayer`s by
    `PaintLayerPainter`
    by executing all of the steps of the painting algorithm explained in the
    documentation for the element. When painting a stacked element of the second
    type, we don't paint its stacked descendants which are managed by the parent
    stacking context.

*   Non-stacked pseudo stacking contexts: elements that are not stacked, but
    paint their descendants (excluding any stacked contents) as if they created
    stacking contexts. This includes

    *   inline blocks, inline tables, inline-level replaced elements
        (E.2.7.2.1.4 in the documentation)
    *   non-positioned floating elements (E.2.5 in the documentation)
    *   [flex items](http://www.w3.org/TR/css-flexbox-1/#painting)
    *   [grid items](http://www.w3.org/TR/css-grid-1/#z-order)
    *   custom scrollbar parts

    They are painted by `ObjectPainter::paintAllPhasesAtomically()` which
    executes all of the steps of the painting algorithm explained in the
    documentation, except ignores any descendants which are positioned or have
    non-auto z-index (which is achieved by skipping descendants with
    self-painting layers).

*   Other normal elements.

### Other glossaries

*   Paint container: the parent of an object for painting, as defined by
    [CSS2.1 spec for painting]((http://www.w3.org/TR/CSS21/zindex.html)). For
    regular objects, this is the parent in the DOM. For stacked objects, it's
    the containing stacking context-inducing object.

*   Paint container chain: the chain of paint ancestors between an element and
    the root of the page.

*   Compositing container: an implementation detail of Blink, which uses
    `PaintLayer`s to represent some layout objects. It is the ancestor along the
    paint ancestor chain which has a PaintLayer. Implemented in
    `PaintLayer::compositingContainer()`. Think of it as skipping intermediate
    normal objects and going directly to the containing stacked object.

*   Compositing container chain: same as paint chain, but for compositing
    container.

*   Paint invalidation container: the nearest object on the compositing
    container chain which is composited. Slimming paint V2 doesn't have this
    concept.

*   Visual rect: the bounding box of all pixels that will be painted by a
    [display item client](../../platform/graphics/paint/README.md#display-items).
    It's in the space of the containing transform property node (see [Building
    paint property trees](#building-paint-property-trees)).

*   Isolation nodes/boundary: In certain situations, it is possible to put in
    place a barrier that isolates a subtree from being affected by its
    ancestors. This barrier is called an isolation boundary and is implemented
    in the property trees as isolation nodes that serve as roots for any
    descendant property nodes. Currently, the `contain: paint` css property
    establishes an isolation boundary.

## Overview

The primary responsibility of this module is to convert the outputs from layout
(the `LayoutObject` tree) to the inputs of the compositor (the `cc::Layer` tree
and associated display items).

At the time of writing, there are three operation modes that are switched by
`RuntimeEnabledFeatures`.


### SlimmingPaintV175 (a.k.a. SPv1.75)

This mode is for incrementally shipping completed features from SPv2. SPv1.75
reuses layerization from SPv1, but will cherrypick property-tree-based paint
from SPv2. Meta display items are abandoned in favor of property tree. Each
drawable GraphicsLayer's layer state will be computed by the property tree
builder. During paint, each display item will be associated with a property
tree state. At the end of paint, meta display items will be generated from
the state differences between the chunk and the layer.

```
from layout
  |
  v
+------------------------------+
| LayoutObject/PaintLayer tree |-----------+
+------------------------------+           |
  |                                        |
  | PaintLayerCompositor::UpdateIfNeeded() |
  |   CompositingInputsUpdater::Update()   |
  |   CompositingLayerAssigner::Assign()   |
  |   GraphicsLayerUpdater::Update()       | PrePaintTreeWalk::Walk()
  |   GraphicsLayerTreeBuilder::Rebuild()  |   PaintPropertyTreeBuider::UpdatePropertiesForSelf()
  v                                        |
+--------------------+                   +------------------+
| GraphicsLayer tree |<------------------|  Property trees  |
+--------------------+                   +------------------+
  |   |                                    |              |
  |   |<-----------------------------------+              |
  |   | LocalFrameView::PaintTree()                       |
  |   |   LocalFrameView::PaintGraphicsLayerRecursively() |
  |   |     GraphicsLayer::Paint()                        |
  |   |       CompositedLayerMapping::PaintContents()     |
  |   |         PaintLayerPainter::PaintLayerContents()   |
  |   |           ObjectPainter::Paint()                  |
  |   v                                                   |
  | +---------------------------------+                   |
  | | DisplayItemList/PaintChunk list |                   |
  | +---------------------------------+                   |
  |   |                                                   |
  |   |<--------------------------------------------------+
  |   | PaintChunksToCcLayer::Convert()
  |   |
  |   | WebContentLayer shim
  v   v
+----------------+
| cc::Layer tree |
+----------------+
  |
  | to compositor
  v
```

#### SPv1 compositing algorithm

The SPv1 compositing system chooses which `LayoutObject`s paint into their
own composited backing texture. This is called "having a compositing trigger".
These textures correspond to GraphicsLayers. There are also additional
`GraphicsLayer`s which represent property tree-related effects.

All elements which do not have a compositing trigger paint into the texture
of the nearest `LayoutObject`with a compositing trigger on its
*compositing container chain* (except for squashed layers; see below). For
historical, practical and implementation detail reasons, only `LayoutObject`s
with `PaintLayer`s can have a compositing trigger. See crbug.com/370604 for a
bug tracking this limitation, which is often referred to as the "fundamental
compositing bug".

The various compositing triggers are listed
[here](../../platform/graphics/compositing_reasons.h).
They fall in to several categories:
1. Direct reasons due to CSS style (see `CompositingReason::kComboAllDirectStyleDeterminedReasons`)
2. Direct reasons due to other conditions (see `CompositingReason::kComboAllDirectNonStyleDeterminedReasons`)
3. Composited scrolling-dependent reasons (see `CompositingReason::kComboAllCompositedScrollingDeterminedReasons`)
4. Composited descendant-dependent reasons (see `CompositingReason::kComboCompositedDescendants`)
5. Overlap-dependent reasons (See `CompositingReasons::kComboSquashableReasons`)

The triggers have no effect unless `PaintLayerCompositor::CanBeComposited`
returns true.

Category (1) always triggers compositing of a `LayoutObject` based on its own
style. Category (2) triggers based on the `LayoutObject`'s style, its DOM
ancestors, and whether it is a certain kind of frame root. Category (3)
triggers based on whether composited scrolling applies to the `LayoutObject`,
or the `LayoutObject` moves relative to a composited scroller (position: fixed
or position: sticky). Category (4) triggers if there are any stacking
descendants of the `LayoutObject` that end up composited. Category 5 triggers
if the `LayoutObject` paints after and overlaps (or may overlap) another
composited layer.

Note that composited scrolling is special. Several ways it is special:
 * Composited descendants do _not_ necessarily cause composited scrolling of an
ancestor.
 * The presence of LCD text prevents composited scrolling in the
absence of other overriding triggers.
 * Local frame roots always use
composited scrolling if they have overflow.
 * Non-local frame roots use
composited scrolling if they have overflow and any composited descendants.
 * Composited scrolling is indicated by a bit on PaintLayerScrollableArea, not
 a direct compositing reason. This bit is then transformed into a compositing
 reason from category (3) during the CompositingRequirementsUpdater

Note that overlap triggers have two special behaviors:
 * Any `LayoutObject`
which may overlap a `LayoutObject` that uses composited scrolling or a
transform animation, paints after it, and scrolls with respect to it, receives
an overlap trigger. In some cases this trigger is too aggressive.
 * Inline CSS
transform is treated as if it was a transform animation. (This is a heuristic
to speed up the compositing step but leads to more composited layers.)

The sequence of work during the `DocumentLifecycle` to compute these triggers
is as follows:

 * `kInStyleRecalc`: compute (1) and most of (4) by calling
`CompositingReasonFinder::PotentialCompositingReasonsFromStyle` and caching
the result on `PaintLayer`, accessible via
`PaintLayer::PotentialCompositingReasonsFromStyle`. Dirty bits in
`StyleDifference` determine whether this has to be re-computed on a particular
lifecycle update.
 * `kInCompositingUpdate`: compute (2) `CompositingInputsUpdater`. Also
 set the composited scrolling bit on `PaintLayerScrollableArea` if applicable.
 * `kCompositingInputsClean`: compute (3), the rest of (4), and (5), in
`CompositingRequirementsUpdater`

### BlinkGenPropertyTrees

This mode is for incrementally shipping completed features from SPv2. It is
based on SPv1.75 and starts sending a layer list and property trees directly to
the compositor. BlinkGenPropertyTrees still uses the GraphicsLayers from SPv1.75
and plugs them in as foreign layers to the SPv2 compositor
(PaintArtifactCompositor).

```
from layout
  |
  v
+------------------------------+
| LayoutObject/PaintLayer tree |-----------+
+------------------------------+           |
  |                                        |
  | PaintLayerCompositor::UpdateIfNeeded() |
  |   CompositingInputsUpdater::Update()   |
  |   CompositingLayerAssigner::Assign()   |
  |   GraphicsLayerUpdater::Update()       | PrePaintTreeWalk::Walk()
  |   GraphicsLayerTreeBuilder::Rebuild()  |   PaintPropertyTreeBuider::UpdatePropertiesForSelf()
  v                                        |
+--------------------+                   +------------------+
| GraphicsLayer tree |<------------------|  Property trees  |
+--------------------+                   +------------------+
      |                                    |              |
      |<-----------------------------------+              |
      | LocalFrameView::PaintTree()                       |
      |   LocalFrameView::PaintGraphicsLayerRecursively() |
      |     GraphicsLayer::Paint()                        |
      |       CompositedLayerMapping::PaintContents()     |
      |         PaintLayerPainter::PaintLayerContents()   |
      |           ObjectPainter::Paint()                  |
      v                                                   |
    +---------------------------------+                   |
    | DisplayItemList/PaintChunk list |                   |
    +---------------------------------+                   |
      |                                                   |
      |<--------------------------------------------------+
      | PaintChunksToCcLayer::Convert()                   |
      v                                                   |
+----------------+                                        |
| Foreign layers |                                        |
+----------------+                                        |
  |                                                       |
  |    LocalFrameView::PushPaintArtifactToCompositor()    |
  |         PaintArtifactCompositor::Update()             |
  +--------------------+       +--------------------------+
                       |       |
                       v       v
        +----------------+  +-----------------------+
        | cc::Layer list |  |   cc property trees   |
        +----------------+  +-----------------------+
                |              |
  +-------------+--------------+
  | to compositor
  v
```

### SlimmingPaintV2 (a.k.a. SPv2)

This is a new mode under development. In this mode, layerization runs after
pre-paint and paint, and meta display items are abandoned in favor of property
trees.

The process starts with pre-paint to generate property trees. During paint,
each generated display item will be associated with a property tree state.
Adjacent display items having the same property tree state will be grouped as
`PaintChunk`. The list of paint chunks then will be processed by
`PaintArtifactCompositor` for layerization. Property nodes that will be
composited are converted into cc property nodes, while non-composited property
nodes are converted into meta display items by `PaintChunksToCcLayer`.

```
from layout
  |
  v
+------------------------------+
| LayoutObject/PaintLayer tree |
+------------------------------+
  |     |
  |     | PrePaintTreeWalk::Walk()
  |     |   PaintPropertyTreeBuider::UpdatePropertiesForSelf()
  |     v
  |   +--------------------------------+
  |<--|         Property trees         |
  |   +--------------------------------+
  |                                  |
  | LocalFrameView::PaintTree()      |
  |   FramePainter::Paint()          |
  |     PaintLayerPainter::Paint()   |
  |       ObjectPainter::Paint()     |
  v                                  |
+---------------------------------+  |
| DisplayItemList/PaintChunk list |  |
+---------------------------------+  |
  |                                  |
  |<---------------------------------+
  | LocalFrameView::PushPaintArtifactToCompositor()
  |   PaintArtifactCompositor::Update()
  |
  +---+---------------------------------+
  |   v                                 |
  | +----------------------+            |
  | | Chunk list for layer |            |
  | +----------------------+            |
  |   |                                 |
  |   | PaintChunksToCcLayer::Convert() |
  v   v                                 v
+----------------+ +-----------------------+
| cc::Layer list | |   cc property trees   |
+----------------+ +-----------------------+
  |                  |
  +------------------+
  | to compositor
  v
```

### Comparison of the three modes

```
                                 | SPv175             | BlinkGenPropertyTrees | SPv2
---------------------------------+--------------------+-----------------------+-------
REF::BlinkGenPropertyTreesEnabled| false              | true                  | false
REF::SPv2Enabled                 | false              | false                 | true
Layerization                     | PLC/CLM            | PLC/CLM               | PAC
cc property tree builder         | on                 | off                   | off
```

## PrePaint
[`PrePaintTreeWalk`](pre_paint_tree_walk.h)

During `InPrePaint` document lifecycle state, this class is called to walk the
whole layout tree, beginning from the root FrameView, across frame boundaries.
We do the following during the tree walk:

### Building paint property trees
[`PaintPropertyTreeBuilder`](paint_property_tree_builder.h)

This class is responsible for building property trees
(see [the platform paint README file](../../platform/graphics/paint/README.md)).

Each `PaintLayer`'s `LayoutObject` has one or more `FragmentData` objects (see
below for more on fragments). Every `FragmentData` has an
`ObjectPaintProperties` object if any property nodes are induced by it. For
example, if the object has a transform, its `ObjectPaintProperties::Transform()`
field points at the `TransformPaintPropertyNode` representing that transform.

The `NeedsPaintPropertyUpdate`, `SubtreeNeedsPaintPropertyUpdate` and
`DescendantNeedsPaintPropertyUpdate` dirty bits on `LayoutObject` control how
much of the layout tree is traversed during each `PrePaintTreeWalk`.

Additionally, some dirty bits are cleared at an isolation boundary. For example
if the paint property tree topology has changed by adding or removing nodes
for an element, we typically force a subtree walk for all descendants since
the descendant nodes may now refer to new parent nodes. However, at an
isolation boundary, we can reason that none of the descendants of an isolation
element would be affected, since the highest node that the paint property nodes
of an isolation element's subtree can reference are the isolation
nodes established at this element itself.

Implementation note: the isolation boundary is achieved using alias nodes, which
are nodes that are put in place on an isolated element for clip, transform, and
effect trees. These nodes do not themselves contribute to any painted output,
but serve as parents to the subtree nodes. The alias nodes and isolation nodes
are synonymous and are used interchangeably. Also note that these nodes are
placed as children of the regular nodes of the element. This means that the
element itself is not isolated against ancestor mutations; it only isolates the
element's subtree.

Example tree:
+----------------------+
| 1. Root LayoutObject |
+----------------------+
      |       |
      |       +-----------------+
      |                         |
      v                         v
+-----------------+       +-----------------+
| 2. LayoutObject |       | 3. LayoutObject |
+-----------------+       +-----------------+
      |                         |
      v                         |
+-----------------+             |
| 4. LayoutObject |             |
+-----------------+             |
                                |
      +-------------------------+
      |                         |
+-----------------+       +-----------------+
| 5. LayoutObject |       | 6. LayoutObject |
+-----------------+       +-----------------+
      |   |
      |   +---------------------+
      |                         |
      v                         v
+-----------------+       +-----------------+
| 7. LayoutObject |       | 8. LayoutObject |
+-----------------+       +-----------------+

Suppose that element 3's style changes to include a transform (e.g.
"transform: translateX(10px);").

Typically, here is the order of the walk (depth first) and updates:
*    Root element 1 is visited since some descendant needs updates
*    Element 2 is visited since it is one of the descendants, but it doesn't
     need updates.
*    Element 4 is skipped since the above step didn't need to recurse.
*    Element 3 is visited since it's a descendant of the root element, and its
     property trees are updated to include a new transform. This causes a flag
     to be flipped that all subtree nodes need an update.
*    Elements are then visited in the depth order: 5, 7, 8, 6. Elements 5 and 6
     reparent their transform nodes to point to the transform node of element 3.
     Elements 7 and 8 are visited and updated but no changes occur.

Now suppose that element 5 has "contain: paint" style, which establishes an
isolation boundary. The walk changes in the following way:

*    Root element 1 is visited since some descendant needs updates
*    Element 2 is visited since it is one of the descendants, but it doesn't
     need updates.
*    Element 4 is skipped since the above step didn't need to recurse.
*    Element 3 is visited since it's a descendant of the root element, and its
     property trees are updated to include a new transform. This causes a flag
     to be flipped that all subtree nodes need an update.
*    Element 5 is visited and updated by reparenting the transform nodes.
     However, now the element is an isolation boundary so elements 7 and 8 are
     not visited (i.e. the forced subtree update flag is ignored).
*    Element 6 is visited as before and is updated to reparent the transform
     node.

Note that there are subtleties when deciding whether we can skip the subtree
walk. Specifically, not all subtree walks can be stopped at an isolation
boundary. For more information, see
[`PaintPropertyTreeBuilder`](paint_property_tree_builder.h) and its use of
IsolationPiercing vs IsolationBlocked subtree update reasons.


#### Fragments

In the absence of multicolumn/pagination, there is a 1:1 correspondence between
self-painting `PaintLayer`s and `FragmentData`. If there is
multicolumn/pagination, there may be more `FragmentData`s.. If a `PaintLayer`
has a property node, each of its fragments will have one. The parent of a
fragment's property node is the property node that belongs to the ancestor
`PaintLayer` which is part of the same column. For example, if there are 3
columns and both a parent and child `PaintLayer` have a transform, there will be
3 `FragmentData` objects for the parent, 3 for the child, each `FragmentData`
will have its own `TransformPaintPropertyNode`, and the child's ith fragment's
transform will point to the ith parent's transform.

Each `FragmentData` receives its own `ClipPaintPropertyNode`. They
also store a unique `PaintOffset, `PaginationOffset and
`LocalBordreBoxProperties` object.

See [`LayoutMultiColumnFlowThread.h`](../layout/layout_multi_column_flow_thread.h)
for a much more detail about multicolumn/pagination.

### Paint invalidation
[`PaintInvalidator`](paint_invalidator.h)

Paint invalidator marks anything that need to be painted differently from the
original cached painting.

During the document lifecycle stages prior to PrePaint, objects are marked for
needing paint invalidation checking if needed by style change, layout change,
compositing change, etc. In PrePaint stage, we traverse the layout tree in
pre-order, crossing frame boundaries, for marked subtrees and objects and
invalidate display item clients that will generate different display items.

At the beginning of the PrePaint tree walk, a root `PaintInvalidatorContext`
is created for the root `LayoutView`. During the tree walk, one
`PaintInvalidatorContext` is created for each visited object based on the
`PaintInvalidatorContext` passed from the parent object. It tracks the following
information to provide O(1) complexity access to them if possible:

*   Paint invalidation container (Slimming Paint v1 only): Since as indicated by
    the definitions in [Glossaries](#other-glossaries), the paint invalidation
    container for stacked objects can differ from normal objects, we have to
    track both separately. Here is an example:

        <div style="overflow: scroll">
            <div id=A style="position: absolute"></div>
            <div id=B></div>
        </div>

    If the scroller is composited (for high-DPI screens for example), it is the
    paint invalidation container for div B, but not A.

*   Painting layer: the layer which will initiate painting of the current
    object. It's the same value as `LayoutObject::PaintingLayer()`.

`PaintInvalidator`[PaintInvalidator.h] initializes `PaintInvalidatorContext`
for the current object, then calls `LayoutObject::InvalidatePaint()` which
calls the object's paint invalidator (e.g. `BoxPaintInvalidator`) to complete
paint invalidation of the object.

#### Paint invalidation of text

Text is painted by `InlineTextBoxPainter` using `InlineTextBox` as display
item client. Text backgrounds and masks are painted by `InlineTextFlowPainter`
using `InlineFlowBox` as display item client. We should invalidate these display
item clients when their painting will change.

`LayoutInline`s and `LayoutText`s are marked for full paint invalidation if
needed when new style is set on them. During paint invalidation, we invalidate
the `InlineFlowBox`s directly contained by the `LayoutInline` in
`LayoutInline::InvalidateDisplayItemClients()` and `InlineTextBox`s contained by
the `LayoutText` in `LayoutText::InvalidateDisplayItemClients()`. We don't need
to traverse into the subtree of `InlineFlowBox`s in
`LayoutInline::InvalidateDisplayItemClients()` because the descendant
`InlineFlowBox`s and `InlineTextBox`s will be handled by their owning
`LayoutInline`s and `LayoutText`s, respectively, when changed style is
propagated.

#### Specialty of `::first-line`

`::first-line` pseudo style dynamically applies to all `InlineBox`'s in the
first line in the block having `::first-line` style. The actual applied style is
computed from the `::first-line` style and other applicable styles.

If the first line contains any `LayoutInline`, we compute the style from the
`::first-line` style and the style of the `LayoutInline` and apply the computed
style to the first line part of the `LayoutInline`. In Blink's style
implementation, the combined first line style of `LayoutInline` is identified
with `FIRST_LINE_INHERITED` pseudo ID.

The normal paint invalidation of texts doesn't work for first line because
*   `ComputedStyle::VisualInvalidationDiff()` can't detect first line style
    changes;
*   The normal paint invalidation is based on whole LayoutObject's, not aware of
    the first line.

We have a special path for first line style change: the style system informs the
layout system when the computed first-line style changes through
`LayoutObject::FirstLineStyleDidChange()`. When this happens, we invalidate all
`InlineBox`es in the first line.

## Paint

### Paint result caching

`PaintController` holds the previous painting result as a cache of display
items. If some painter would generate results same as those of the previous
painting, we'll skip the painting and reuse the display items from cache.

#### Display item caching

When a painter would create a `DrawingDisplayItem` exactly the same as the
display item created in the previous painting, we'll reuse the previous one
instead of repainting it.

#### Subsequence caching

When possible, we create a scoped `SubsequenceRecorder` in
`PaintLayerPainter::PaintContents()` to record all display items generated in
the scope as a "subsequence". Before painting a layer, if we are sure that the
layer will generate exactly the same display items as the previous paint, we'll
get the whole subsequence from the cache instead of repainting them.

There are many conditions affecting
*   whether we need to generate subsequence for a PaintLayer;
*   whether we can use cached subsequence for a PaintLayer.
See `ShouldCreateSubsequence()` and `shouldRepaintSubsequence()` in
`PaintLayerPainter.cpp` for the conditions.

### Empty paint phase optimization

During painting, we walk the layout tree multiple times for multiple paint
phases. Sometimes a layer contain nothing needing a certain paint phase and we
can skip tree walk for such empty phases. Now we have optimized
`PaintPhaseDescendantBlockBackgroundsOnly`, `PaintPhaseDescendantOutlinesOnly`
and `PaintPhaseFloat` for empty paint phases.

During paint invalidation, we set the containing self-painting layer's
`NeedsPaintPhaseXXX` flag if the object has something needing to be painted in
the paint phase.

During painting, we check the flag before painting a paint phase and skip the
tree walk if the flag is not set.

It's hard to clear a `NeedsPaintPhaseXXX` flag when a layer no longer needs the
paint phase, so we never clear the flags. Instead, we use another set of flags
(`PreviousPaintPhaseXXXWasEmpty`) to record if a painting of a phase actually
produced nothing. We'll skip the next painting of the phase if the flag is set,
regardless of the corresponding `NeedsPaintPhaseXXX` flag. We will clear the
`PreviousPaintPhaseXXXWasEmpty` flags when we paint with different clipping,
scroll offset or interest rect from the previous paint.

We don't clear the `PreviousPaintPhaseXXXWasEmpty` flags when the layer is
marked `NeedsRepaint`. Instead we clear the flag when the corresponding
`NeedsPaintPhaseXXX` is set. This ensures that we won't clear
`PreviousPaintPhaseXXXWasEmpty` flags when unrelated things changed which won't

When layer structure changes, and we are not invalidate paint of the changed
subtree, we need to manually update the `NeedsPaintPhaseXXX` flags. For example,
if an object changes style and creates a self-painting-layer, we copy the flags
from its containing self-painting layer to this layer, assuming that this layer
needs all paint phases that its container self-painting layer needs.

We could update the `NeedsPaintPhaseXXX` flags in a separate tree walk, but that
would regress performance of the first paint. For slimming paint v2, we can
update the flags during the pre-painting tree walk to simplify the logics.

### Hit test painting

Hit testing is done in paint-order. The |PaintTouchActionRects| flag enables a
mode where hit test display items are emitted in the background phase of
painting. Hit test display items are produced even if there is no painted
content.

### PaintNG

[LayoutNG](../layout/ng/README.md]) is a project that will change how Layout
generates geometry/style information for painting. Instead of modifying
LayoutObjects, LayoutNG will generate an NGFragment tree.

NGPaintFragments are:
* immutable
* all coordinates are physical. See
[layout_box_model_object.h](../layout/layout_box_model_object.h).
* instead of Location(), NGFragment has Offset(), a physical offset from parent
fragment.

The goal is for PaintNG to eventually paint from NGFragment tree,
and not see LayoutObjects at all. Until this goal is reached,
LegacyPaint, and NGPaint will coexist.

When a particular LayoutObject subclass fully migrates to NG, its LayoutObject
geometry information might no longer be updated\(\*\), and its
painter needs to be rewritten to paint NGFragments.
For example, see how BlockPainter is being rewritten as NGBoxFragmentPainter.
