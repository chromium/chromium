# LayoutNG #

This directory contains the implementation of Blink's new layout engine
"LayoutNG".

This document can be viewed in formatted form [here](https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/layout/layout_ng.md).

The original design document can be seen [here](https://docs.google.com/document/d/1uxbDh4uONFQOiGuiumlJBLGgO4KDWB8ZEkp7Rd47fw4/edit).

## High level overview ##

CSS has many different types of layout modes, controlled by the `display`
property. (In addition to this specific HTML elements have custom layout modes
as well). For each different type of layout, we have a
[LayoutAlgorithm](layout_algorithm.h).

The input to an [LayoutAlgorithm](layout_algorithm.h) is the same tuple
for every kind of layout:

 - The [BlockNode](block_node.h) which we are currently performing layout for. The
   following information is accessed:

   - The [ComputedStyle](../style/computed_style.h) for the node which we are
     currently performing laying for.

   - The list of children [BlockNode](block_node.h)es to perform layout upon, and their
     respective style objects.

 - The [ConstraintSpace](constraint_space.h) which represents the "space"
   in which the current layout should produce a
   [PhysicalFragment](physical_fragment.h).

 - TODO(layout-dev): BreakTokens should go here once implemented.

The current layout should not access any information outside this set, this
will break invariants in the system. (As a concrete example we intend to cache
[PhysicalFragment](physical_fragment.h)s based on this set, accessing
additional information outside this set will break caching behaviour).

### Box Tree ###

TODO(layout-dev): Document with lots of pretty pictures.

### Inline Layout ###

Please refer to the [inline layout README](inline/README.md).

### Fragment Tree ###

TODO(layout-dev): Document with lots of pretty pictures.

All coordinates and sizes associated with an PhysicalFragment are physical,
i.e. pure left/top offsets from the parent fragment, and sizes are expressed
with widths and heights (not inline-size / block-size). No logical offsets or
sizes. Writing mode and direction are resolved during layout.

### Constraint Spaces ###

TODO(layout-dev): Document with lots of pretty pictures.

## Block Flow Algorithm ##

Please refer to the [block flow layout docs](block_layout.md).

### Collapsing Margins ###

TODO(layout-dev): Document with lots of pretty pictures.

### Float Positioning ###

TODO(layout-dev): Document with lots of pretty pictures.

### Fragment Caching ###

After layout, we cache the resulting fragment to avoid redoing all of layout
the next time. You can read the full [design
document](https://docs.google.com/document/d/1RjH_Ofa8O_ucGvaDCEgsBVECPqUTiQKR3zNyVTr-L_I/edit).

Here's how it works:

* We store the input constraint space and the resulting fragment on the
  [LayoutBlockFlow](layout_ng_block_flow.h). However, we only do that if
  we have no break token to simplify the initial implementation. We call
  `LayoutBlockFlow::SetCachedLayoutResult` from `BlockNode::Layout`.
* Once cached, `BlockNode::Layout` checks at the beginning if we already
  have a cached result by calling `LayoutBlockFlow::CachedLayoutResult`.
  If that returns a layout result, we return it and are done.
* `CachedLayoutResult` will always clone the fragment (but without the offset)
  so that the parent algorithm can position this fragment.
* We only reuse a layout result if the constraint space is identical (using
  `operator==`), if there was no break token (a current limitation, we may
  lift this eventually), and if the node is not marked for layout. We need
  the `NeedsLayout()` check because we currently have no other way to ensure
  that relayout happens when style or children change. Eventually we need to
  rethink this part as we transition away from legacy layout.

### Block fragmentation ###

Design doc [here](https://docs.google.com/document/d/1EJOdFesZKspvrU7uWtGl-8ab2jIrzRF6NKJhwYOs6hU/).

Tutorial [here](block_fragmentation_tutorial.md).

### Printing and hit-testing ###

We'll paint and hit-test by traversing the physical fragment tree, rather than
traversing the `LayoutObject` tree. This is important for block fragmentation,
where a CSS layout box (`LayoutObject`) may be split into multiple fragments,
and it's the relationship between the fragments (not the layout objects) that
determines the offsets. In LayoutNG, there are also fragments that have no
corresponding layout object - e.g. a column (or other types of [fragmentainer]s
too).

Traditionally, when doing block fragmentation (multicol) in legacy layout, we
had to perform some complicated calculations, where we mapped and sliced layout
objects into fragments during pre-paint. In LayoutNG this job is now as a
natural part of layout. So, all we have to do for painting and hit-testing, is
traverse the fragments. A fragment holds a list of child fragments and their
offsets. The offsets are relative to the parent fragment. As such, it's a rather
straight-forward job for pre-paint to calculate the offsets and bounding box.

### Code coverage ###

The latest code coverage (from Feb 14 2017) can be found [here](https://glebl.users.x20web.corp.google.com/www/layout_ng_code_coverage/index.html).
Here is the instruction how to generate a new result.

#### Environment setup ####
 1. Set up Chromium development environment for Windows
 2. Download DynamoRIO from [www.dynamorio.org](www.dynamorio.org)
 3. Extract downloaded DynamoRIO package into your chromium/src folder.
 4. Get dynamorio.git and extract it into your chromium/src folder `git clone https://github.com/DynamoRIO/dynamorio.git`
 5. Install Node js from https://nodejs.org/en/download/
 6. Install lcov-merger dependencies:  `npm install vinyl, npm install vinyl-fs`
 7. Install Perl from https://www.perl.org/get.html#win32
 8. Get lcov-result-merger and extract into your chromium/src folder `git clone https://github.com/mweibel/lcov-result-merger`

#### Generating code coverage ####
* Build the unit tets target with debug information
`chromium\src> ninja -C out\Debug blink_unittests`
* Run DynamoRIO with drcov tool
`chromium\src>DynamoRIO\bin64\drrun.exe -t drcov -- .\out\Debug\blink_unittests.exe --gtest_filter=NG*`
* Convert the output information to lcov format
`chromium\src>for %file in (*.log) do DynamoRIO\tools\bin64\drcov2lcov.exe -input %file -output %file.info -src_filter layout/ng -src_skip_filter _test`
* Merge all lcov files into one file
`chromium\src>node lcov-result-merger\bin\lcov-result-merger.js *.info output.info`
* Generate the coverage html from the lcov file
`chromium\src>C:\Perl64\bin\perl.exe dynamorio.git\third_party\lcov\genhtml output.info -o output`

### Debugging, logging and testing ###
Both layout input node subtrees and layout output physical fragment subtrees
may be dumped, for debugging, logging and testing purposes.

#### For layout input node subtree ####
Call LayoutInputNode::ShowNodeTree() to dump the tree to stderr.

#### For physical fragment subtree ####
Call PhysicalFragment::ShowFragmentTree() to dump the tree to
stderr. Fragments in the subtree are not required to be marked as placed
(i.e. know their offset).

A fragment tree may also be dumped to a String, by calling
PhysicalFragment::DumpFragmentTree(). It takes a flag parameter, so that the
output can be customized to only contain what's relevant for a given purpose.


[fragmentainer]: https://drafts.csswg.org/css-break/#fragmentation-container
