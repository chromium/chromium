# LayoutNG Inline Layout #

This directory contains the inline layout implementation
of Blink's new layout engine "LayoutNG".

This README can be viewed in formatted form [here](https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/layout/inline/README.md).

Other parts of LayoutNG is explained [here](../layout_ng.md).

## What is Inline Layout ##

Inline layout is one of [CSS normal flow] layout models.

From the CSS2 spec on [inline formatting context]:
an inline formatting context is established by a block container box
that contains no [block-level] boxes.

[block-level]: https://drafts.csswg.org/css2/#block-level
[CSS normal flow]: https://drafts.csswg.org/css2/#normal-flow
[inline formatting context]: https://drafts.csswg.org/css2/#inline-formatting
[inline-level]: https://drafts.csswg.org/css2/#inline-level

*** note
Current code determines [inline formatting context]
using slightly different criteria,
but this is still to be discussed.
See crbug.com/734554.
***

Following DOM tree is transformed to fragment tree
as in the following.

|||---|||
### DOM ###

```html
<div>
  <span>
    Hello
  </span>
</div>
```

### LayoutInputNode ###

* BlockNode
  - InlineNode
    - InlineItem (open tag, span)
    - InlineItem (text, "Hello")
    - InlineItem (close tag, span)

### Fragment tree ###

* PhysicalBoxFragment
  - PhysicalBoxFragment (anonymous wrapper)
    - PhysicalLineBoxFragment
      - PhysicalBoxFragment (span, may be omitted)
        - NGPhysicalTextFragment ("Hello")
|||---|||

## Inline Layout Phases ##

Inline layout is performed in the following phases:

1. **[Pre-layout]** converts LayoutObject tree to a concatenated string
   and a list of [InlineItem].
2. **[Line breaking]** breaks it into lines and
   produces a list of [InlineItemResult] for each line.
3. **[Line box construction]** orders and positions items on a line.
4. **[Generate fragments]** generates physical fragments.

| Phase | Input | Output |
|---|---|---|
| Pre-layout | LayoutObject | [InlineItem] |
| Line Breaking | [InlineItem] | [InlineItemResult] |
| Line box construction | [InlineItemResult] | [LogicalLineItem] |
| Generate fragments | [LogicalLineItem] | [PhysicalFragment] / [FragmentItem] |

Note: There is [an idea](https://docs.google.com/document/d/1dxzIHl1dwBtgeKgWd2cKcog8AyydN5rduQvXthMOMD0/edit?usp=sharing)
to merge [InlineItemResult] and [LogicalLineItem], but this hasn't been happened yet.

This is similar to [CSS Text Processing Order of Operations],
but not exactly the same,
because the spec prioritizes the simple description than being accurate.

[CSS Text Processing Order of Operations]: https://drafts.csswg.org/css-text-3/#order

### <a name="pre-layout">Pre-layout</a> ###
[pre-layout]: #pre-layout

For inline layout there is a pre-layout pass that prepares the internal data
structures needed to perform line layout.

The pre-layout pass, triggered by calling `InlineNode::PrepareLayout()`, has
three separate steps or stages that are executed in order:

  - `CollectInlines`: Performs a depth-first scan of the container collecting
    all non-atomic inlines and `TextNodes`s. Atomic inlines are represented as a
    unicode object replacement character but are otherwise skipped.
    Each non-atomic inline and `TextNodes` is fed to a
    [InlineItemsBuilder](inline_items_builder.h) instance which collects
    the text content for all non-atomic inlines in the container.

    During this process white-space is collapsed and normalized according to CSS
    white-space processing rules.

    The CSS [text-transform] is already applied in LayoutObject tree.
    The plan is to implement in this phase when LayoutNG builds the tree from DOM.

  - `SegmentText`: Performs BiDi segmentation and resolution.
    See [Bidirectional text] below.

  - `ShapeText`: Shapes the resolved BiDi runs using HarfBuzz.
    TODO(eae): Fill out

[text-transform]: https://drafts.csswg.org/css-text-3/#propdef-text-transform

### <a name="line-breaking">Line Breaking</a> ###
[line breaking]: #line-breaking

[LineBreaker] takes a list of [InlineItem],
measure them, break into lines, and
produces a list of [InlineItemResult] for each line.

[InlineItemResult] keeps several information
needed during the line box construction,
such as inline size and [ShapeResult],
though the inline position is recomputed later
because [Bidirectional text] may change it.

This phase:
1. Measures each item.
2. Breaks text [InlineItem] into multiple [InlineItemResult].
   The core logic of this part is implemented in [ShapingLineBreaker].
3. Computes inline size of borders/margins/paddings.
   The block size of borders/margins/paddings are ignored
   for inline non-replaced elements, but
   the inline size affects layout, and hence line breaking.
   See [CSS Calculating widths and margins] for more details.
4. Determines which item can fit in the line.
5. Determines the break opportunities between items.
   If an item overflows, and there were no break opportunity before it,
   the item before must also overflow.

[ShapingLineBreaker] is... TODO(kojii): fill out.

[CSS Calculating widths and margins]: https://drafts.csswg.org/css2/#Computing_widths_and_margins

### <a name="line-box-construction">Line Box Construction</a> ###
[line Box Construction]: #line-box-construction

`InlineLayoutAlgorithm::CreateLine()` takes a list of [InlineItemResult] and
produces a list of [LogicalLineItem].

This phase consists of following sub-phases:

1. Create a [LogicalLineItem] for each [InlineItemResult]
   and determine the positions.

   The inline size of each item was already determined by [LineBreaker],
   but the inline position is recomputed
   because [BiDi reordering](#bidi) may change them.

   In block direction,
   [LogicalLineItem] is placed as if the baseline is at 0.
   This is adjusted later, possibly multiple times,
   for [vertical-align] and the block offset of the parent inline box.

   An open-tag item pushes a new stack entry of [InlineBoxState],
   and a close-tag item pops a stack entry.
   This stack is used to determine the size of the inline box,
   for [vertical-align], and for a few other purposes.
   Please see [Inline Box Tree] below.

2. Process all pending operations in [Inline Box Tree].
3. [Bidirectional reordering](#bidi):
   Reorder the list of [LogicalLineItem]
   according to [UAX#9 Reordering Resolved Levels].
   See [Bidirectional text] below.

   After this point forward, the list of [LogicalLineItem] is
   in _visual order_; which is from [line-left] to [line-right].
   The block direction is still logical,
   but the inline direction is physical.
4. Applies [ellipsizing] if needed.
5. Applies the CSS [text-align] property.
6. Moves the baseline to the correct position
   based on the height of the line box.

[ellipsizing]: https://drafts.csswg.org/css-ui-3/#overflow-ellipsis
[line-left]: https://drafts.csswg.org/css-writing-modes-3/#line-left
[line-right]: https://drafts.csswg.org/css-writing-modes-3/#line-right
[text-align]: https://drafts.csswg.org/css-text-3/#propdef-text-align

#### Inline Box Tree ####
[Inline Box Tree]: #inline-box-tree

A flat list structure is suitable for many inline operations,
but some operations require an inline box tree structure.
A stack of [InlineBoxState] is constructed
from a list of [InlineItemResult] to represent the box tree structure.

This stack:
1. Caches common values for an inline box.
   For instance, the primary font metrics do not change within an inline box.
2. Computes the height of an inline box.
   The [height of inline, non-replaced elements depends on the content area],
   but CSS doesn't define how to compute the content area.
3. Creates [PhysicalBoxFragment]s when needed.
   CSS doesn't define when an inline box should have a box,
   but existing implementations are interoperable that
   there should be a box when it has borders.
   Also, background should have a box if it is not empty.
   Other cases are where paint or scroller will need a box.
4. Applies [vertical-align] to shift baselines.
   Some values are applicable immediately.
   Some values need the size of the box, or the parent box.
   Some values need the size of the root inline box.
   Depends on the value,
   the operation is queued to the appropriate stack entry.

Because of index-based operations to the list of [InlineItemResult],
the list is append-only during the process.
When all operations are done,
`OnEndPlaceItems()` turns the list into the final fragment tree structure.

[height of inline, non-replaced elements depends on the content area]: https://drafts.csswg.org/css2/#inline-non-replaced
[vertical-align]: https://drafts.csswg.org/css2/#propdef-vertical-align

#### Box Fragments in Line Box Fragments ####

Not all [inline-level] boxes produces [PhysicalBoxFragment]s.

[InlineLayoutAlgorithm] determines
whether a [PhysicalBoxFragment] is needed or not,
such as when a `<span>` has borders,
and calls [InlineBoxState]`::SetNeedsBoxFragment()`.

Since [PhysicalBoxFragment] needs to know its children
and size before creating it,
`InlineLayoutStateStack::AddBoxFragmentPlaceholder()`
first creates placeholders.
We then add children,
and adjust positions both horizontally and vertically.

Once all children and their positions and sizes are finalized,
`InlineLayoutStateStack::CreateBoxFragments()`
creates [PhysicalBoxFragment] and add children to it.

### <a name="generate-fragments">Generate Fragments</a> ###
[generate fragments]: #generate-fragments

When all [LogicalLineItem]s are ordered and positioned,
they are converted to fragments.

Without [FragmentItem] enabled,
each [LogicalLineItem] produces a [PhysicalFragment],
added to the [PhysicalLineBoxFragment].

With [FragmentItem] enabled,
each [LogicalLineItem] produces a [FragmentItem],
added to the [FragmentItems] in the containing block of the inline formatting context.

## Miscellaneous topics ##

### Baseline ###

Computing baselines in LayoutNG goes the following process.

1. Users of baseline should request what kind and type of the baseline
   they need by calling [ConstraintSpaceBuilder]`::AddBaselineRequest()`.
2. Call [LayoutInputNode]`::Layout()`,
   that calls appropriate layout algorithm.
3. Each layout algorithm computes baseline according to the requests.
4. Users retrieve the result by [PhysicalBoxFragment]`::Baseline()`,
   or by higher level functions such as
   [LogicalBoxFragment]`::BaselineMetrics()`.

Algorithms are responsible
for checking [ConstraintSpace]`::BaselineRequests()`,
computing requested baselines, and
calling [BoxFragmentBuilder]`::AddBaseline()`
to add them to [PhysicalBoxFragment].

[NGBaselineRequest] consists of [BaselineAlgorithmType] and [FontBaseline].

In most normal cases,
algorithms should decide which box should provide the baseline
for the specified [BaselineAlgorithmType] and delegate to it.

[FontBaseline] currently has only two baseline types,
alphabetic and ideographic,
and they are hard coded to derive from the CSS [writing-mode] property today.

We plan to support more baseline types,
and allow authors to specify the baseline type,
as defined in the CSS [dominant-baseline] property in future.
[PhysicalLineBoxFragment] and [NGPhysicalTextFragment] should be
responsible for computing different baseline types from font metrics.

[dominant-baseline]: https://drafts.csswg.org/css-inline/#dominant-baseline-property
[writing-mode]: https://drafts.csswg.org/css-writing-modes-3/#propdef-writing-mode

### <a name="culled"></a>Culled Inline ###
[Culled inline]: #culled

For performance and memory consumption,
Blink ignores some inline-boxes during inline layout
because they don't impact layout nor they are needed for paint purposes.
An example of this is
```html
<span style="border: 1px solid blue"><b>Text</b></span>
```
The `<b>` has the same size as the inner text-node
so it can be ignored for layout purpose,
while the `<span>` has borders, which impacts paint,
so it produces a box fragment.
This optimization is called "culled inline" in the code.

LayoutNG uses similar criteria as legacy engine
on whether to generate a fragment or not,
but LayoutNG generates fragments in a little more cases than legacy
in order to improve the accuracy of hit-testing and bounding rects.
This is determined by `ShouldCreateBoxFragment()`,
which is primarily computed during style recalc,
but layout may also turn it on when the need is discovered during layout.

One downside of this optimization is that we have extra work to do when
querying post-layout information; e.g., hit-testing or asking for bounding rects.

Another downside is making a culled inline box not to be culled requires re-layout
even if the change does not affect layout; e.g., adding background.
To mitigate multiple layouts by like hover highlighting,
`ShouldCreateBoxFragment()` will not be reset once it's set.

### <a name="bidi"></a>Bidirectional Text ###
[Bidirectional Text]: #bidi

[UAX#9 Unicode Bidirectional Algorithm] defines
processing algorithm for bidirectional text.

The core logic is implemented in [BidiParagraph],
which is a thin wrapper for [ICU BiDi].

In a bird's‐eye view, it consists of two parts:

1. Before line breaking: Segmenting text and resolve embedding levels
   as defined in [UAX#9 Resolving Embedding Levels].

   The core logic uses [ICU BiDi] `ubidi_getLogicalRun()` function.

   This is part of the Pre-layout phase above.
2. After line breaking:  Reorder text
   as defined in [UAX#9 Reordering Resolved Levels].

   The core logic uses [ICU BiDi] `ubidi_reorderVisual()` function.

   This is part of the Line Box Construction phase above.

Initial design doc: [Using ICU BiDi in LayoutNG](https://docs.google.com/document/d/182H1Sj_FCEHcl6eC69J4KcIc5m3ohSzgo297KYB0S_c/edit?usp=sharing)

### Interface for Editing ###

[OffsetMapping] provides functions for converting between offsets in the text
content of an inline formatting context (computed in [pre-layout]) and DOM
positions in the context. See [design doc](https://goo.gl/CJbxky) for details.

[ICU BiDi]: http://userguide.icu-project.org/transforms/bidi
[UAX#9 Unicode Bidirectional Algorithm]: http://unicode.org/reports/tr9/
[UAX#9 Resolving Embedding Levels]: http://www.unicode.org/reports/tr9/#Resolving_Embedding_Levels
[UAX#9 Reordering Resolved Levels]: http://www.unicode.org/reports/tr9/#Reordering_Resolved_Levels

[BaselineAlgorithmType]: ../constraint_space.h
[BidiParagraph]: ../../../platform/text/bidi_paragraph.h
[BlockNode]: ../block_node.h
[BoxFragmentBuilder]: ../box_fragment_builder.h
[ConstraintSpace]: ../constraint_space.h
[ConstraintSpaceBuilder]: ../constraint_space_builder.h
[FontBaseline]: ../../../platform/fonts/font_baseline.h
[FragmentItem]: fragment_item.h
[FragmentItems]: fragment_items.h
[InlineBoxState]: inline_box_state.h
[InlineItem]: inline_item.h
[InlineItemResult]: inline_item_result.h
[InlineLayoutAlgorithm]: inline_layout_algorithm.h
[InlineNode]: inline_node.h
[LayoutInputNode]: ../layout_input_node.h
[LineBreaker]: line_breaker.h
[LogicalBoxFragment]: ../logical_box_fragment.h
[LogicalLineItem]: logical_line_item.h
[LogicalLineItems]: logical_line_items.h
[NGBaselineRequest]: ng_baseline.h
[NGPhysicalTextFragment]: ng_physical_text_fragment.h
[OffsetMapping]: offset_mapping.h
[PhysicalBoxFragment]: ../physical_box_fragment.h
[PhysicalFragment]: ../physical_fragment.h
[PhysicalLineBoxFragment]: physical_line_box_fragment.h
[ShapeResult]: ../../../platform/fonts/shaping/shape_result.h
[ShapingLineBreaker]: ../../../platform/fonts/shaping/shaping_line_breaker.h
