<!---
  The live version of this document can be viewed at:
  https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/paint/README.md
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

*   Visual rect: the bounding box of all pixels that will be painted by a
    for a [display item](../../platform/graphics/paint/README.md#display-items)
    It's in the space of the containing transform property node (see [Building
    paint property trees](#building-paint-property-trees)). It's calculated
    during paint for each display item.

*   Isolation nodes/boundary: In certain situations, it is possible to put in
    place a barrier that isolates a subtree from being affected by its
    ancestors. This barrier is called an isolation boundary and is implemented
    in the property trees as isolation nodes that serve as roots for any
    descendant property nodes. Currently, the `contain: paint` css property
    establishes an isolation boundary.

*   Local property tree state: the `PropertyTreeState` associated with each
    fragment. All fragments have a well-defined local property tree state.
    This is often cached in the `LocalBorderBoxProperties`
    struct that belongs to `FragmentData`. Some `FragmentData` objects don't
    have a `LocalBorderBoxProperties`, but that is merely a memory optimization.

## Overview

The primary responsibility of this directory is to convert the outputs from
layout (the `LayoutObject` tree) to the inputs of the compositor
(the `cc::Layer` list, which contains display items, and the associated
`cc::PropertyNode`s).

This process is done in the following document lifecycle phases:

*   [PrePaint](#PrePaint) (`kInPrePaint`)
    *    [Paint invalidation](#Paint-invalidation) which invalidates display
         items which need to be painted.
    *    [Builds paint property trees](#Building-paint-property-trees).
*   [Paint](#Paint) (`kInPaint`)
    *    Walks the LayoutObject tree and creates a display item list.
    *    Groups the display list into paint chunks which share the same
         property tree state.
    *    Commits the results to the compositor.
        *    Decides which cc::Layers to create based on paint chunks.
        *    Passes the paint chunks to the compositor in a cc::Layer list.
        *    Converts the blink property tree nodes into cc property tree nodes.

[Debugging blink objects](https://docs.google.com/document/d/1vgQY11pxRQUDAufxSsc2xKyQCKGPftZ5wZnjY2El4w8/view)
has information about dumping the paint and compositing datastructures for
debugging.


### Compositing algorithm

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
`PaintInvalidatorContext` passed from the parent object. It tracks the painting
layer which will initiate painting of the current object.

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
`LocalBorderBoxProperties` object.

## Paint

Within a PaintLayer, paint walks the PhysicalFragment tree in paint-order and
produces a list of display items. This is implemented using static painter
classes (such as [`BoxFragmentPainter`](box_fragment_painter.cc)) and
appends display items to a
[`PaintController`](../../platform/graphics/paint/paint_controller.h). There is
only one `PaintController` for the entire `LocalFrameView`. During this
treewalk, the current property tree state is maintained (see:
`PaintController::UpdateCurrentPaintChunkProperties`). The `PaintController`
segments the display item list into
[`PaintChunk`](../../platform/graphics/paint/paint_chunk.h)s which are
sequential display items that share a common property tree state.

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
`PaintPhaseDescendantOutlinesOnly` and `PaintPhaseFloat` for empty paint phases.

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

### Property tree update optimization

In some specific cases of style updates, we can directly update the property
tree without needing to run the property tree builder (Which requires a layout
tree walk). During `PaintLayer::StyleDidChange` we check if this update meets
the requirements for a quick update, and if so we add it to a list of pending
updates (Those updates can't be executed on the fly because then paint offset
changes can't be detected correctly).

The updates are executed later in `PrePaintTreeWalk::WalkTree`.
If at some point during pre-paint we reach a node that has a pending update,
we mark that node as needs full update, and remove the pending update from the
list

When setting the display-locked property of an object (or ending a forced
scope, effectively locking it), we remove all the pending opacity updates of
that document. We actually need to remove only the updates for objects that are
in that display, but the check is too expensive, so we remove all of the
pending updates.

Current updates that are checked for an optimized update are transform updates
and opacity updates.

### Hit test information recording

Hit testing is done in paint-order, and to preserve this information the paint
system is re-used to record hit test information when painting the background.
This information is then used in the compositor to implement cc-side hit
testing. Hit test information is recorded even if there is no painted content.

We record different types of hit test information in the following data
structures:

1. Paint chunk bounds

   The bounds of the current paint chunk are expanded to ensure the bounds
   contain the hit testable area.

2. [`HitTestData::touch_action_rects`](../../platform/graphics/paint/hit_test_data.h)

   Used for [touch action rects](http://docs.google.com/document/u/1/d/1ksiqEPkDeDuI_l5HvWlq1MfzFyDxSnsNB8YXIaXa3sE/view)
   which are areas of the page that allow certain gesture effects, as well as
   areas of the page that disallow touch events due to blocking touch event
   handlers.

3. [`HitTestData::wheel_event_rects`](../../platform/graphics/paint/hit_test_data.h)

   Used for [wheel event handler regions](https://docs.google.com/document/d/1ar4WhVnLA-fmw6atgP-23iq-ys_NfFoGb3LA5AgaylA/view)
      which are areas of the page that disallow default wheel event processing
      due to blocking wheel event handlers.

4. [`HitTestData::scroll_translation`](../../platform/graphics/paint/hit_test_data.h)
   and
   [`HitTestData::scroll_hit_test_rect`](../../platform/graphics/paint/hit_test_data.h)

   Used to create
   [non-fast scrollable regions](https://docs.google.com/document/d/1IyYJ6bVF7KZq96b_s5NrAzGtVoBXn_LQnya9y4yT3iw/view)
   to prevent compositor scrolling of non-composited scrollers, plugins with
   blocking scroll event handlers, and resize handles.

   If `scroll_translation` is not null, this is also used to force a special
   cc::Layer that is marked as being scrollable when composited scrolling is
   needed for the scroller.

### Scrollbar painting

During painting, for a non-custom scrollbar we create a
[ScrollbarDisplayItem](../../platform/graphics/paint/scrollbar_display_item.h)
which contains a [cc::Scrollbar](../../../../cc/input/scrollbar.h) and other
information that are needed to actually paint the scrollbar into a paint record
or to create a cc scrollbar layer. During PaintArtifactCompositor update,
we decide whether to composite the scrollbar and, if not composited, actually
paint the scrollbar as a paint record, otherwise create a cc scrollbar layer
of type cc::SolidColorScrollbarLayer, cc::PaintedScrollbarLayer or
cc::PaintedOverlayScrollbarLayer depending on the type of the scrollbar.

Custom scrollbars are still painted into drawing display items directly.
