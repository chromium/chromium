# LayoutNG Paint #

This directory contains the paint system to work with
the Blink's new layout engine [LayoutNG].

This README can be viewed in formatted form
[here](https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/core/paint/ng/README.md).

## NGPaintFragment ##

LayoutNG produces a tree of [NGPhysicalFragment].

One of its goals is to share a sub-tree of NGPhysicalFragment across frames,
or even within a frame where possible.
This goal enforces a few characteristics:

* It must be immutable.
* It must be relative within the sub-tree, ideally only to its parent.

A [NGPaintFragment] owns a NGPhysicalFragment by `scoped_refptr` in n:1 relation.
For instance, NGPhysicalFragment can be shared across frames,
but different NGPaintFragment instance can be created for each frame.

It has following characteristics when compared to NGPhysicalFragment:

* It can have mutable fields, such as `VisualRect()`.
* It can use its own coordinate system.
* Separate instances can be created when NGPhysicalFragment is shared.
* It can have its own tree structure, differently from NGPhysicalFragment tree.

### The tree structure ###

In short, one can think that the NGPaintFragment tree structure is
exactly the same as the NGPhysicalFragment tree structure.

LayoutNG will be launched in phases. In the phase 1 implementation,
only boxes with inline children are painted directly from fragments, and
NGPaintFragment is generated only for this case.
For other cases, LayoutNG copies the layout output to the LayoutObject tree
and uses the existing paint system.

If `LayoutBlockFlow::PaintFragment()` exists,
it's a root of a NGPaintFragment tree.
It also means, in the current phase, it's a box that contains only inline children.

Note that not all boxes with inline children can be laid out by LayoutNG today,
so the reverse is not true.

### Multiple NGPaintFragment tree ###

Because not all boxes are painted by the fragment painter,
and even not all boxes are handled by LayoutNG,
one LayoutObject tree can generate multiple NGPaintFragment trees.

For example, if a block contains an inline block that is not handled by LayoutNG,
an NGPaintFragment tree is created with the inline block as a leaf child,
even though the inline block LayoutObject has children.

If the inline block has another inline block which LayoutNG can handle,
another NGPaintFragment tree is created for the inner inline block.

### NGPhysicalFragment traversal ###

When possible (when sufficiently transitioned to LayoutNG), we'll paint and
hit-test by traversing the physical fragment tree, rather than traversing the
LayoutObject tree. This is important for block fragmentation, where a CSS layout
box (LayoutObject) may be split into multiple fragments, and it's the
relationship between the fragments (not the layout objects) that determines the
offsets. In LayoutNG, there are also fragments that have no corresponding layout
object - e.g. a column (or other types of [fragmentainer]s too).

Traditionally, when doing block fragmentation (multicol) in legacy layout, we
have to perform some complicated calculations, where we map and slice layout
objects into fragments during pre-paint. In LayoutNG this job is now as a
natural part of layout. So, all we have to do for painting and hit-testing, is
traverse the fragments. A fragment holds a list of child fragments and their
offsets. The offsets are relative to the parent fragment. As such, it's a rather
straight-forward job for pre-paint to calculate the offsets and bounding box.

[LayoutNG]: ../../layout/ng/README.md
[NGPaintFragment]: ng_paint_fragment.h
[NGPhysicalFragment]: ../../layout/ng/ng_physical_fragment.h
[fragmentainer]: https://drafts.csswg.org/css-break/#fragmentation-container
