<!---
  The live version of this document can be viewed at:
  https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/core/paint/README.md
-->

# renderer/core/paint

The code in this directory converts the LayoutObject tree into an efficient
rendering format for the compositor (a list of cc::Layers containing display
item lists, and associated cc::PropertyTrees). For a high level overview, see
the [Overview](#Overview) section.

For information about how the display list and paint property trees are
implemented, see
[the platform paint README file](../../platform/graphics/paint/README.md).

This code is owned by the
[rendering team](https://www.chromium.org/teams/rendering).

[TOC]

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

    *   Replaced normal-flow stacking elements:
        [replaced elements](https://html.spec.whatwg.org/C/#replaced-elements)
        that do not have non-auto z-index but are stacking contexts for elements
        below them. Right now the only example is SVG `<foreignObject>`. The
        difference between these elements and regular stacking contexts is that
        they paint in the foreground phase of the painting algorithm (as opposed
        to the positioned descendants phase).

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
    container chain which is composited. CompositeAfterPaint doesn't have this
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

The primary responsibility of this directory is to convert the outputs from
layout (the `LayoutObject` tree) to the inputs of the compositor
(the `cc::Layer` list, which contains display items, and the associated
`cc::PropertyNode`s).

This process is done in the following document lifecycle phases:

*   Compositing update (`kInCompositingUpdate`, `kCompositingInputsClean`)
    *    Decides layerization (GraphicsLayers).
    *    This is only needed for the
         [current compositing algorithm](#Current-compositing-algorithm-CompositeBeforePaint_)
         and will go away with
         [CompositeAfterPaint](#New-compositing-algorithm-CompositeAfterPaint_).
*   [PrePaint](#PrePaint) (`kInPrePaint`)
    *    [Paint invalidation](#Paint-invalidation) which invalidates display
         items which need to be painted.
    *    [Builds paint property trees](#Building-paint-property-trees).
*   [Paint](#Paint) (`kInPaint`)
    *    Walks the LayoutObject tree and creates a display item list.
    *    Groups the display list into paint chunks which share the same
         property tree state.
    *    Commits the results to the compositor.
        *    [CompositeAfterPaint](##New-compositing-algorithm-CompositeAfterPaint_)
             will decide layerization at this point.
        *    Passes the paint chunks to the compositor in a cc::Layer list.
        *    Converts the blink property tree nodes into cc property tree nodes.


Compositing decisions are currently made before paint (see
[Current compositing algorithm](#Current-compositing-algorithm-CompositeBeforePaint_))
but there is an in-progress refactoring to make compositing decisions after
paint (see
[CompositeAfterPaint](##New-compositing-algorithm-CompositeAfterPaint_)). The
most recent step towards CompositeAfterPaint was a project called
[BlinkGenPropertyTrees](https://docs.google.com/document/d/17GKr2uIH2O5GthdTyvJpv1qZjoHYoLgrzvCkbCHoID4/view)
which uses the compositing decisions from the current compositor
(PaintLayerCompositor, which produces GraphicsLayers) with the new
CompositeAfterPaint compositor (PaintArtifactCompositor). This is done by a step
at the end of paint which collects all painted GraphicsLayers (and their
associated cc::Layers) as a list of
[ForeignLayerDisplayItem](../../platform/graphics/paint/foreign_layer_display_item.h)s.
Foreign layers are typically used for cc::Layers managed outside blink (e.g.,
video layers, plugin layers) and are treated as opaque composited content by
the PaintArtifactCompositor. This approach of using foreign layers starts using
much of the new PaintArtifactCompositor logic (e.g., converting blink property
trees to cc property trees) without changing how compositing decisions are made.

[Debugging blink objects](https://docs.google.com/document/d/1vgQY11pxRQUDAufxSsc2xKyQCKGPftZ5wZnjY2El4w8/view)
has information about dumping the paint and compositing datastructures for
debugging.


### Current compositing algorithm (CompositeBeforePaint)

The current compositing system chooses which `LayoutObject`s paint into their
own composited backing texture. This is called "having a compositing trigger".
These textures correspond to GraphicsLayers. There are also additional
`GraphicsLayer`s which represent property tree-related effects.

All elements which do not have a compositing trigger paint into the texture
of the nearest `LayoutObject`with a compositing trigger on its
*compositing container chain* (except for squashed layers; see below). For
historical, practical and implementation detail reasons, only `LayoutObject`s
with `PaintLayer`s can have a compositing trigger. See
[crbug.com/370604](https://crbug.com/370604) for a bug tracking this limitation,
which is often referred to as the **fundamental compositing bug**.

The various compositing triggers are listed in
[compositing_reasons.h](../../platform/graphics/compositing_reasons.h) and fall
in to several categories:

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


The flow of data from the LayoutObject tree to the cc::Layer list and cc
property trees is described below:

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
[Debugging blink objects](https://docs.google.com/document/d/1vgQY11pxRQUDAufxSsc2xKyQCKGPftZ5wZnjY2El4w8/view)
has information about dumping these paint and compositing datastructures for
debugging.

### New compositing algorithm (CompositeAfterPaint)

This is a new mode under development. In this mode, layerization decisions are
made after paint.

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
[Debugging blink objects](https://docs.google.com/document/d/1vgQY11pxRQUDAufxSsc2xKyQCKGPftZ5wZnjY2El4w8/view)
has information about dumping these paint and compositing datastructures for
debugging.

### Comparison of the current and new compositing algorithms

The
[current compositing design](#Current-compositing-algorithm-CompositeBeforePaint_)
is an incremental step towards the new
[CompositeAfterPaint](##New-compositing-algorithm-CompositeAfterPaint_) design
and was launched as [BlinkGenPropertyTrees](https://docs.google.com/document/d/17GKr2uIH2O5GthdTyvJpv1qZjoHYoLgrzvCkbCHoID4/view). The design before
BlinkGenPropertyTrees is not described in this document.


|                                 | Current (CompositeBeforePaint)               | New (CompositeAfterPaint) |
|---------------------------------|:---------------------------------------------|:--------------------------|
| REF::CompositeAfterPaintEnabled | False                                        | True                      |
| Layerization                    | PaintLayerCompositor, CompositedLayerMapping | PaintArtifactCompositor   |
| PaintController                 | One per GraphicsLayer                        | One per LocalFrameView    |

## PrePaint
[`PrePaintTreeWalk`](pre_paint_tree_walk.h)

During the `InPrePaint` document lifecycle state, this class is called to walk
the whole layout tree, beginning from the root FrameView, and across frame
boundaries. This is an in-order tree traversal which is important for
efficiently computing DOM-order hierarchy such as the parent containing block.

The PrePaint walk has two primary goals:
[paint invalidation](#Paint-invalidation) and
[building paint property trees](#Building-paint-property-trees).

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

*   Paint invalidation container (Slimming Paint v1 only): As described by
    the definitions in [Other glossaries](#Other-glossaries), the paint
    invalidation container for stacked objects can differ from normal objects,
    we have to track both separately. Here is an example:

        <div style="overflow: scroll">
            <div id=A style="position: absolute"></div>
            <div id=B></div>
        </div>

    If the scroller is composited (for high-DPI screens for example), it is the
    paint invalidation container for div B, but not A.

*   Painting layer: the layer which will initiate painting of the current
    object. It's the same value as `LayoutObject::PaintingLayer()`.

[`PaintInvalidator`](PaintInvalidator.h) initializes `PaintInvalidatorContext`
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
with `kPseudoIdFirstLineInherited`.

The normal paint invalidation of texts doesn't work for first line because:

*   `ComputedStyle::VisualInvalidationDiff()` can't detect first line style
    changes;
*   The normal paint invalidation is based on whole LayoutObject's, not aware of
    the first line.

We have a special path for first line style change: the style system informs the
layout system when the computed first-line style changes through
`LayoutObject::FirstLineStyleDidChange()`. When this happens, we invalidate all
`InlineBox`es in the first line.

### Building paint property trees
[`PaintPropertyTreeBuilder`](paint_property_tree_builder.h)

This class is responsible for building property trees
(see
[platform/paint/README.md](../../platform/graphics/paint/README.md#Paint-properties)
for information about what property trees are).

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
```
                        +----------------------+
                        | 1. Root LayoutObject |
                        +----------------------+
                          /                  \
           +-----------------+            +-----------------+
           | 2. LayoutObject |            | 3. LayoutObject |
           +-----------------+            +-----------------+
             /                              /             \
  +-----------------+          +-----------------+    +-----------------+
  | 4. LayoutObject |          | 5. LayoutObject |    | 6. LayoutObject |
  +-----------------+          +-----------------+    +-----------------+
                                 /             \
                   +-----------------+     +-----------------+
                   | 7. LayoutObject |     | 8. LayoutObject |
                   +-----------------+     +-----------------+
```
Suppose that element 3's style changes to include a transform (e.g.
`transform: translateX(10px)`).

Typically, here is the order of the walk (depth first) and updates:

*    Root element 1 is visited since some descendant needs updates
*    Element 2 is visited since it is one of the descendants, but it doesn't
     need updates.
*    Element 4 is skipped since the above step didn't need to recurse.
*    Element 3 is visited since it's a descendant of the root element, and its
     property trees are updated to include a new transform. This causes a flag
     to be flipped that all subtree nodes need an update.
*    Elements are then visited in depth order: 5, 7, 8, 6. Elements 5 and 6
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
`LayoutObject`s and `FragmentData`. If there is multicolumn/pagination,
there may be more `FragmentData`s. If a `LayoutObject` has a property node,
each of its fragments will have one. The parent of a fragment's property node is
the property node that belongs to the ancestor `LayoutObject` which is part of
the same column. For example, if there are 3 columns and both a parent and child
`LayoutObject` have a transform, there will be 3 `FragmentData` objects for
the parent, 3 for the child, each `FragmentData` will have its own
`TransformPaintPropertyNode`, and the child's ith fragment's transform will
point to the ith parent's transform.

Each `FragmentData` receives its own `ClipPaintPropertyNode`. They
also store a unique `PaintOffset, `PaginationOffset and
`LocalBordreBoxProperties` object.

See
[`LayoutMultiColumnFlowThread.h`](../layout/layout_multi_column_flow_thread.h)
for a much more detail about multicolumn/pagination.

## Paint

Paint walks the LayoutObject tree in paint-order and produces a list of
display items. This is implemented using static painter classes
(e.g., [`BlockPainter`](block_painter.cc)) and appends display items to a
[`PaintController`](../../platform/graphics/paint/paint_controller.h). During
this treewalk, the current property tree state is maintained (see:
`PaintController::UpdateCurrentPaintChunkProperties`). The `PaintController`
segments the display item list into
[`PaintChunk`](../../platform/graphics/paint/paint_chunk.h)s which are
sequential display items that share a common property tree state.

With the
[current compositing algorithm](#Current-compositing-algorithm-CompositeBeforePaint_),
the paint-order `LayoutObject` treewalk is initiated by `GraphicsLayer`s, and
each `GraphicsLayer` contains a `PaintController`. In the new compositing
approach,
[CompositeAfterPaint](##New-compositing-algorithm-CompositeAfterPaint_), there
is only one `PaintController` for the entire `LocalFrameView`.

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

There are many conditions affecting whether we need to generate subsequence for
a PaintLayer and whether we can use cached subsequence for a PaintLayer. See
`ShouldCreateSubsequence()` and `shouldRepaintSubsequence()` in
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

When layer structure changes, and we are not invalidate paint of the changed
subtree, we need to manually update the `NeedsPaintPhaseXXX` flags. For example,
if an object changes style and creates a self-painting-layer, we copy the flags
from its containing self-painting layer to this layer, assuming that this layer
needs all paint phases that its container self-painting layer needs.

We could update the `NeedsPaintPhaseXXX` flags in a separate tree walk, but that
would regress performance of the first paint. For CompositeAfterPaint, we can
update the flags during the pre-painting tree walk to simplify the logic.

### Hit test painting

Hit testing is done in paint-order, and to preserve this information the paint
system is re-used to paint hit test display items in the background phase of
painting. This information is then used in the compositor to implement cc-side
hit testing. Hit test display items are produced even if there is no painted
content.

There are two types of hit test painting:

1. [HitTestDisplayItem](../../platform/graphics/paint/hit_test_display_item.h)

    Used for [touch action rects](http://docs.google.com/document/u/1/d/1ksiqEPkDeDuI_l5HvWlq1MfzFyDxSnsNB8YXIaXa3sE/view)
    which are areas of the page that allow certain gesture effects, as well as
    areas of the page that disallow touch events due to blocking touch event
    handlers.

2. [ScrollHitTestDisplayItem](../../platform/graphics/paint/scroll_hit_test_display_item.h)

    Used to create
    [non-fast scrollable regions](https://docs.google.com/document/d/1IyYJ6bVF7KZq96b_s5NrAzGtVoBXn_LQnya9y4yT3iw/view)
    to prevent compositor scrolling of non-composited scrollers, plugins with
    blocking scroll event handlers, and resize handles.

    This is also used for CompositeAfterPaint to force a special cc::Layer that
    is marked as being scrollable.

### Scrollbar painting

For now in pre-CompositeAfterPaint, we have distinct paths for composited
scrollbars and non-composited scrollbars. For a composited scrollbar,
PaintArtifactCompositor creates a GraphicsLayer, then ScrollingCoordinator
creates the cc scrollbar layer which is set as the content layer of the
GraphicsLayer. For a non-composited scrollbar, ScrollableAreaPainter paints
the scrollbar into various drawing display items.

In CompositeAfterPaint, during painting, for a non-custom scrollbar we create a
[ScrollbarDisplayItem](../../platform/graphics/paint/scrollbar_display_item.h)
which contains a [cc::Scrollbar](../../../../cc/input/scrollbar.h) and other
information that are needed to actually paint the scrollbar into a paint record
or to create a cc scrollbar layer. During PaintArtifactCompositor update,
we decide whether to composite the scrollbar and, if not composited, actually
paint the scrollbar as a paint record, otherwise create a cc scrollbar layer
of type cc::SolidColorScrollbarLayer, cc::PaintedScrollbarLayer or
cc::PaintedOverlayScrollbarLayer depending on the type of the scrollbar.

In CompositeAfterPaint, custom scrollbars are still painted into drawing
display items directly.

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

