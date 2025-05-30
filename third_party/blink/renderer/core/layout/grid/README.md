<!---
  The live version of this document can be viewed at:
  https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/layout/grid/README.md
-->

# Grid Layout

This document walks through high level concepts used in Blink’s implementation.
For an in depth walkthrough of the implementation details, please see the
following two BinkOn Talks:

- [Grid BlinkOn talk](https://www.youtube.com/watch?v=TicsklsAZOE) presented
by Ethan Jimenez
- [Subgrid BlinkOn talk](https://www.youtube.com/watch?v=ML6DPHSRoRE)
presented by Ethan Jimenez

## Grid Tree

One of the core adjustments made to the Grid Layout module to support Subgrid
was the introduction of the [Grid Tree](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_subtree.h;bpv=0;bpt=1).
There exists both a [`GridSizingTree`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_sizing_tree.h)
and a [`GridLayoutTree`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_data.h;l=177),
which will be discussed in more detail in later sections. However, the basic
rules surrounding the `GridTree` structure is an important place to start
in our journey through the rabbit hole of Grid.

![Diagram showing that subgrids form a tree structure under the root grid](
resources/grid-tree.png)

A `GridTree` matches the structure of the DOM tree where each node is a grid
container.

As pictured in the diagram above, one of the core rules to the `GridTree`
structure is that a grid with both standalone columns and rows establishes
the root for a new `GridTree`, and it’s called a “root grid”.

Any grid with one or two subgridded axes (i.e., tracks inherited from its
parent) will be parented to another grid. Such grid elements are called
subgrids and, recursively, can have their own subgrid children.

The reason we required to use a tree structure for Grid was as a result
of a [tricky edge case](
https://wpt.fyi/results/css/css-grid/subgrid/auto-track-sizing-001.html?label=master&label=experimental&aligned).

![Image depecting a test case that resulted in the need for a GridTree](
resources/auto-track-sizing.png)

The image above roughly outlines the problem posed by [auto-track-sizing-001.html](
https://github.com/web-platform-tests/wpt/blob/13056cf344/css/css-grid/subgrid/auto-track-sizing-001.html).
In this example, we have a root grid with auto-sized rows, with two columns, one
of 100px wide, and the other auto sized. Within this grid, we have a grid with
standalone columns and subgridded rows. If the standalone axis for this grid is
auto sized, when we go to run sizing for the columns, we would not have any
knowledge of the 100px constraint of the root grid. We would then use the min
size to size the columns. When we go to re-run everything with the known column
size of 100px, we will end up with a taller inner grid than expected given that
more text could fit per line with the 100px space.

As a result of this particular use case, we decided to refactor the grid layout
algorithm to use a tree format, such that the root grid is in control of layout
for its entire tree, allowing us to properly constrain standalone axes in
nested subgrids.

## `GridTreeNode`, `GridSizingTree`, and `GridLayoutTree`

Below is a high level outline of the data structures that are involved when it
comes to the Grid Tree.

[`GridTreeNode`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_sizing_tree.h;l=84;drc=a169ebabcd533a7f5489ac17c1df80f8b89c6fee?q=gridsizingtree&ss=chromium)
which is made up of:
- [`GridLayoutData`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_data.h;drc=a169ebabcd533a7f5489ac17c1df80f8b89c6fee;bpv=1;bpt=1;l=80)
- [`GridItems`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_item.h;l=261;drc=a169ebabcd533a7f5489ac17c1df80f8b89c6fee)*
- Subtree size

NOTE: *`GridItems` is only a member of `GridTreeNode` in the sizing tree.

[`GridSizingTree`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_sizing_tree.h;l=80?q=gridsizingtree&ss=chromium%2Fchromium%2Fsrc):
a tree made up of `GridTreeNode`(s)
- `GridLayoutData` might be mutable.
- Subgridded items are included.

[`GridLayoutTree`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_data.h;l=191?q=gridlayouttree&ss=chromium%2Fchromium%2Fsrc):
a tree made up of `GridTreeNode`(s)
- Pared down through the `ContsraintSpace`.
- `GridLayoutData` is **immutable**.

## Key Rule in Grid Layout

One important rule to note when it comes to grid is that **Placement must come
before Sizing**. Sizing depends on placement for ALL items, including those
that are subgridded.

## Grid Layout Implementation

Outlined below is the Grid Layout Algorithm as implemented in Chromium.

![Process diagram of the Grid Layout Algorithm](
resources/grid-layout.png)

The [`ConstraintSpace`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/constraint_space.h;l=1?q=ConstraintSpace&ss=chromium)
is one of the key inputs to all Layout algorithms. It contains several pieces
of information, including the inline and block size of the parent node.
If the inline size is indefinite, we will end up entering the Grid Layout
Algorithm through [`ComputeMinAndMaxContentContributionForSelf()`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/length_utils.h;l=717;bpv=0;bpt=1).

If we entered the grid layout algorithm via
`ComputeMinAndMaxContentContributionForSelf()`, this means that the inline size
is indefinite, and that we must [`ComputeMinMaxSizes()`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_layout_algorithm.h;drc=4e673771b1ee61f0e9f854e2d1420f353c67c401;bpv=1;bpt=1;l=29?q=gridlayoutalgor&ss=chromium).
This can happen if, for example, the `width` was `min-content`, `auto`, or a
percentage.

If the inline size is definite, though, we will enter [`Layout()`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_layout_algorithm.h;l=28?q=gridlayoutalgor&ss=chromium).
The `GridLayoutAlgorithm` stores the `available_inline_size_`,
`available_block_size_`, along with `min_available_size_` and
`max_available_size_` if the size is indefinite and min-width/height is
defined.

If we have an inherited `GridLayoutTree`, just copy the finalized
`LayoutData` and perform `Layout()` on your children. This will **always**
happen for subgrids.

Otherwise, we will build a `GridSizingTree`, which is the output of the
Grid Sizing pipeline. We will first construct Grid items with the
[`GridLineResolver`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_line_resolver.h;l=1?q=gridlineresolver&ss=chromium).
This involves running [`GridPlacement`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_placement.h;l=19?q=gridplacement&ss=chromium)
(which takes the `GridLineResolver` and `ComputedStyle` as inputs), and outputs
[`GridPlacementData`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_data.h;l=17?q=GridPlacementData&ss=chromium)
(either from the cache or from computing it).

We use the `GridPlacementData` to construct [`GridItems`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_item.h;l=283?q=griditems&ss=chromium),
which is a vector of [`GridItemData`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_item.h;drc=e686c80feb21c29957282a8a4505a86b4feaca1f;l=30?q=griditems&ss=chromium).
This includes a range of spanned tracks, track span properties (e.g. is orthogonal,
is spanning intrinsic tracks, is sugrid) (This data is cached for perf reasons),
and baseline properties. Note that these properties are relative to the root
grid’s writing mode.

Once we have built track ranges, we are left with a `GridSizingTree` for each grid
in the tree. This contains both `GridLayoutData` and `GridItems`.

Next, we will initialize track sizes, which is recursive. This involves
initializing `GridLayoutData`, which includes building/rebuilding [`GridSet`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_track_collection.h;l=310?q=GridSet&ss=chromium)(s)
(we might need to rebuild if there are `ConstraintSpace` changes), and reset
initial values for sizing (step 1 of the Grid layout algorithm).

Next, compute the baseline shims via [`ComputeGridItemBaselines`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_layout_algorithm.h;l=98?q=computegriditem%20baseline&ss=chromium),
which is also recursive.

The final step of the sizing algorithm is [`CompleteTrackSizingAlgorithm()`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_layout_algorithm.h;l=130?q=CompleteTrackSizingAlgorithm&ss=chromium).
This is recursive and is steps 2-5 of the Grid layout algorithm.

Step 2: [`ResolveIntrinsicTrackSizes()`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_track_sizing_algorithm.h;l=89?q=ResolveIntrinsicTrackSizes&ss=chromium),
which involves determining the [`ContributionSizeForGridItem`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_layout_algorithm.h;l=80?q=resolveintrinsic&ss=chromium)(s)
(including the intrinsic size, margin, baseline shim, and accumulated subgrid margins).
Step 3: [`MaximizeTracks()`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_track_sizing_algorithm.h;l=102?q=maximizetracks&ss=chromium).
Step 4: [`ExpandFlexibleTracks()`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_track_sizing_algorithm.h;l=106?q=maximizetracks&ss=chromium).
Step 5: [`StretchAutoTracks()`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_track_sizing_algorithm.h;l=104?q=maximizetracks&ss=chromium).

In the Sizing algorithm, we can have up to 4 phases. 1. Size columns. 2. Size rows.
3. Resize columns (if needed) 4. Resize rows (if needed). If we are in
`ComputeMinMaxSizes`, we can only ever have up to 3 phases. Steps 3 and 4 are known
as the second pass, which is needed if the final size of the columns is different
than what was estimated in the first pass.

If no second pass is required and we are not running `Layout()`, then we will
compute the total track size for columns as part of `ComputeMinMaxSizes`.

Otherwise, we will finalize grid geometry (see [`PlaceGridItems`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/grid/grid_layout_algorithm.h;l=199?q=PlaceGridItems&ss=chromium)).
This includes discarding subgrid items, finalizing the `GridLayoutTree`, laying
out direct children and applying align/justify properties, and passing the
`GridLayoutTree` down through the `ConstraintSpace` (recursing into Layout
calls).
