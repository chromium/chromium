<!---
  The live version of this document can be viewed at:
  https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/platform/graphics/paint/README.md
-->

# Platform paint code

This directory contains the implementation of display lists and display
list-based painting, except for code which requires knowledge of `core/`
concepts, such as DOM elements and layout objects.

For information about how the display list and paint property trees are
generated, see [the core paint README file](../../../core/paint/README.md).

This code is owned by the [rendering team](https://www.chromium.org/teams/rendering).

[TOC]

## Paint artifact

[Paint artifact](paint_artifact.h) consists of a list of display items in paint
order (ideally mostly or all drawings), partitioned into *paint chunks* which
define certain *paint properties* which affect how the content should be drawn
or composited.

## Paint properties

Paint properties define characteristics of how a paint chunk should be drawn,
such as the transform it should be drawn with. To enable efficient updates,
a chunk's paint properties are described hierarchically. For instance, each
chunk is associated with a transform node, whose matrix should be multiplied by
its ancestor transform nodes in order to compute the final transformation matrix
to the screen.

See [`ObjectPaintProperties`](../../../core/paint/object_paint_properties.h) for
description of all paint properties that we create for a `LayoutObject`.

Paint properties are represented by four paint property trees (transform, clip,
effect and scroll) each of which contains corresponding type of
[paint property nodes](paint_property_node.h). Each paint property node has a
pointer to the parent node. The parent node pointers link the paint property
nodes in a tree.

### Transforms

Each paint chunk is associated with a [transform node](transform_paint_property_node.h),
which defines the coordinate space in which the content should be painted.

Each transform node has:

* a 4x4 [`TransformationMatrix`](../../transforms/transformation_matrix.h)
* a 3-dimensional transform origin, which defines the origin relative to which
  the transformation matrix should be applied (e.g. a rotation applied with some
  transform origin will rotate the plane about that point)
* a boolean indicating whether the transform should be projected into the plane
  of its parent (i.e., whether the total transform inherited from its parent
  should be flattened before this node's transform is applied and propagated to
  children)
* an integer rendering context ID; content whose transform nodes share a
  rendering context ID should sort together
* other fields, see [the header file](transform_paint_property_node.h)

*** note
The painting system may create transform nodes which don't affect the position
of points in the xy-plane, but which have an apparent effect only when
multiplied with other transformation matrices. In particular, a transform node
may be created to establish a perspective matrix for descendant transforms in
order to create the illusion of depth.
***

Note that, even though CSS does not permit it in the DOM, the transform tree can
have nodes whose children do not flatten their inherited transform and
participate in no 3D rendering context. For example, not flattening is necessary
to preserve the 3D character of the perspective transform, but this does not
imply any 3D sorting.

### Clips

Each paint chunk is associated with a [clip node](clip_paint_property_node.h),
which defines the raster region that will be applied on the canvas when
the chunk is rastered.

Each clip node has:

* A float rect with (optionally) rounded corner radius.
* An optional clip path if the clip is a clip path.
* An associated transform node, which the clip rect is based on.

The raster region defined by a node is the rounded rect and/or clip path
transformed to the root space, intersects with the raster region defined by its
parent clip node (if not root).

### Effects

Each paint chunk is associated with an [effect node](effect_paint_property_node.h),
which defines the effect (opacity, transfer mode, filter, mask, etc.) that
should be applied to the content before or as it is composited into the content
below.

Each effect node has:

* effects, including opacity, transfer mode, filter, mask, etc.
* an optional associated clip node which clips the output of the effect when
  composited into the current backdrop.
* an associated transform node which defines the geometry space of some
  geometry-related effects (e.g. some filters).

The hierarchy in the *effect tree* defines the dependencies between
rasterization of different contents.

One can imagine each effect node as corresponding roughly to a bitmap that is
drawn before being composited into another bitmap, though for implementation
reasons this may not be how it is actually implemented.

### Scrolling

Each paint chunk is associated with a [scroll node](scroll_paint_property_node.h)
which defines information about how a subtree scrolls so threads other than the
main thread can perform scrolling. Scroll information includes:

* Which directions, if any, are scrollable by the user.
* A reference to a [transform node](transform_paint_property_node.h) which contains
  a 2d scroll offset.
* The extent that can be scrolled. For example, an overflow clip of size 7x9
  with scrolling contents of size 7x13 can scroll 4px vertically and none
  horizontally.

To ensure geometry operations are simple and only deal with transforms, the
scroll offset is stored as a 2d transform in the transform tree.

## Display items

A display item is the smallest unit of a display list in Blink. Each display
item is identified by an ID consisting of:

* an opaque pointer to the *display item client* that produced it
* a type (from the `DisplayItem::Type` enum)

In practice, display item clients are generally subclasses of `LayoutObject`,
but can be other Blink objects which get painted, such as inline boxes and drag
images.

*** note
It is illegal for there to be two display items with the same ID in a display
item list, except for display items that are marked uncacheable
(see [DisplayItemCacheSkipper](display_item_cache_skipper.h)).
***

Generally, clients of this code should use stack-allocated recorder classes to
emit display items to a `PaintController` (using `GraphicsContext`).

### Standalone display items

#### [DrawingDisplayItem](drawing_display_item.h)

Holds a `PaintRecord` which contains the paint operations required to draw some
atom of content.

#### [ForeignLayerDisplayItem](foreign_layer_display_item.h)

Draws an atom of content, but using a `cc::Layer` produced by some agent outside
of the normal Blink paint system (for example, a plugin). Since they always map
to a `cc::Layer`, they are always the only display item in their paint chunk,
and are ineligible for squashing with other layers.

#### [ScrollbarDisplayItem](scrollbar_display_item.h)

Contains a [cc::Scrollbar](../../../../../../cc/input/scrollbar.h) and other
information that are needed to paint a scrollbar into a paint record or to
create a cc scrollbar layer. During the PaintArtifactCompositor update, we
decide whether to composite the scrollbar and, if not composited, actually paint
the scrollbar as a paint record, otherwise create a cc scrollbar layer of type
cc::SolidColorScrollbarLayer, cc::PaintedScrollbarLayer or
cc::PaintedOverlayScrollbarLayer depending on the type of the scrollbar.

## Paint controller

Callers use `GraphicsContext` (via its drawing methods, and its
`paintController()` accessor) and scoped recorder classes, which emit items into
a `PaintController`.

`PaintController` is responsible for producing the paint artifact. It contains
the *current* paint artifact, and *new* display items and paint chunks, which
are added as content is painted.

Painters should call `PaintController::UseCachedItemIfPossible()` or
`PaintController::UseCachedSubsequenceIfPossible()` and if the function returns
`true`, existing display items that are still valid in the *current* paint artifact
will be reused and the painter should skip real painting of the item or subsequence.

When the new display items have been populated, clients call
`commitNewDisplayItems`, which replaces the previous artifact with the new data,
producing a new paint artifact.

At this point, the paint artifact is ready to be drawn or composited.

### Paint result caching and invalidation

See [Display item caching](../../../core/paint/README.md#paint-result-caching)
and [Paint invalidation](../../../core/paint/README.md#paint-invalidation) for
more details about how caching and invalidation are handled in blink core
module using `PaintController` API.

## Paint artifact compositor

[`PaintArtifactCompositor`](../compositing/paint_artifact_compositor.h) is
responsible for consuming the `PaintArtifact` produced by the `PaintController`,
and converting it into a form suitable for the compositor to consume.

`PaintArtifactCompositor` creates a list of cc::Layers from the paint chunks.
The entry point to layerization is `PaintArtifactCompositor::LayerizeGroup`.
This algorithm has to make tradeoffs between GPU memory and reducing the costs
when things change. The algorithm starts by creating `PendingLayer`s for each
paint chunk, and then tries to combine `PendingLayer`s whenever possible.
Reasons that prevent combining `PendingLayer`s include: having a direct
compositing reason on property nodes, having interleaving composited content
(known as "overlap testing"), and avoiding wasting large areas (known as
"sparsity", see `kMergeSparsityAreaTolerance`). Once the list of `PendingLayer`s
is known, `cc::Layer`s area created for each (see: PaintChunksToCcLayer).

### Direct paint property updates

PaintArtifactCompositor::Update is expensive and can be avoided for simple
paint property updates where layerization is known to not change. For example,
the `kWillChangeTransform` direct compositing reason will force the layerization
algorithm to create cc::Layers so the will-change content can move without
needing to change cc::Layers. `DirectlyUpdateTransform` can then be used to
update the transform property node without doing a full
PaintArtifactCompositor::Update.

### Repaint-only updates

PaintArtifactCompositor::Update is expensive and can be avoided for simple
paint changes where layerization is known to not change. For example, if the
color of a display item changes in a way that does not affect layerization, we
can just update the display items of the existing cc::Layers. This is
implemented in `UpdateRepaintedLayers`.

## Raster invalidation

This is to mark which parts of the composited layers need to be re-rasterized to
reflect changes of painting, by comparing the current paint artifact against the
previous paint artifact. It's the last step of painting.

It's done in two levels:

* Paint chunk level [`RasterInvalidator`](raster_invalidator.h): matches each
  paint chunk in the current paint artifact against the corresponding paint
  chunk in the previous paint artifact, by matching their ids. There are
  following cases:

  * A new paint chunk doesn't match any old paint chunk (appearing): The bounds
    of the new paint chunk in the composited layer will be fully raster
    invalidated.

  * An old paint chunk doesn't match any new paint chunk (disappearing): The
    bounds of the old paint chunk in the composited layer will be fully raster
    invalidated.

  * A new paint chunk matches an old paint chunk:

    * The new paint chunk is moved backward (reordering): this may expose other
      chunks that was previously covered by it: Both of the old bounds and the
      new bounds will be fully raster invalidated.

    * Paint properties of the paint chunk changed:

      * If only clip changed, the difference between the old bounds and
        the new bounds will be raster invalidated (i.e. do incremental
        invalidation).

      * Otherwise, both of the old bounds and the new bounds will be fully
        raster invalidated.

    * Otherwise, check for changed display items within the paint chunk.

* Display item level [`DisplayItemRasterInvalidator`](display_item_raster_invalidator.h):
  This is executed when a new chunk matches an old chunk in-order and paint
  properties didn't change. The algorithm checks changed display items within a
  paint chunk.

  * Similar to the paint chunk level, the visual rects (mapped to the space of
    the composited layer) of appearing, disappearing, reordering display items
    are fully raster invalidated.

  * If a new paint chunk in-order matches an old paint chunk, if the display
    item client has been [paint invalidated](../../../core/paint/README.md#paint-invalidation),
    we will do full raster invalidation (which invalidates the old visual rect
    and the new visual rect in the composted layer) or incremental raster
    invalidation (which invalidates the difference between the old visual rect
    and the new visual rect) according to the paint invalidation reason.

## Geometry routines

The [`GeometryMapper`](geometry_mapper.h) is responsible for efficiently computing
visual and transformed rects of display items in the coordinate space of ancestor
[`PropertyTreeState`](property_tree_state.h)s.

The transformed rect of a display item in an ancestor `PropertyTreeState` is
that rect, multiplied by the transforms between the display item's
`PropertyTreeState` and the ancestors, then flattened into 2D.

The visual rect of a display item in an ancestor `PropertyTreeState` is the
intersection of all of the intermediate clips (transformed in to the ancestor
state), with the display item's transformed rect.
