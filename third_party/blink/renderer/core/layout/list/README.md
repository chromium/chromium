# LayoutNG list #

This directory contains the list implementation
of Blink's new layout engine "LayoutNG".

This README can be viewed in formatted form [here](https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/layout/list/README.md).

Other parts of LayoutNG is explained [here](../layout_ng.md).

## Outside list marker

By default, list markers are at the outside of the list item.
The [css-lists-3] defines [marker positioning],
but it's not ready for implementation
due to the lack of details and interoperability testing.

### The box tree of outside list marker ##

Because CSS2 [defines](https://drafts.csswg.org/css2/visuren.html#box-gen):

> A block container box either contains only block-level boxes or
establishes an inline formatting context and thus contains only inline-level boxes.

and to generate [anonymous block boxes] if children are mix of inline-level and block-level,
list markers can generate anonymous block boxes
if they are inline-level or block-level.
[css-lists-3] defines outside list markers are absolutely positioned to avoid anonymous block boxes,
but doing so affects more than box generations,
such as changing its contaning block or creating [PaintLayer].

In LayoutNG, list markers are out-of-flow only for box generation purposes
because anonymous block boxes can affect layout in unexpected ways,
but other side effects are not desired.

When the content is inline level and therefore generates line boxes:

```html
<li>sample text</li>
```

generates a box tree of:

- LayoutListItem
  - LayoutOutsideListMarker
    - LayoutText (1.)
  - LayoutText (sample text)

Since children except the list marker are inline-level,
an inline formatting context is created, and therefore
[InlineLayoutAlgorithm] lays out this [LayoutListItem].

When the content is block level:

```html
<li><div>sample text</div></li>
```

- LayoutListItem
  - LayoutOutsideListMarker
    - LayoutText (1.)
  - LayoutBlockFlow (div)
    - LayoutText (sample text)

Since children except the list marker are block-level,
[BlockLayoutAlgorithm] lays out this [LayoutListItem].

When the content is mixed:

```html
<li>
  inline text
  <div>block text</div>
</li>
```

- LayoutListItem
  - LayoutOutsideListMarker
    - LayoutText (1.)
  - LayoutBlockFlow (anonymous)
    - LayoutText (inline text)
  - LayoutBlockFlow (div)
    - LayoutText (block text)

Children are block-level in this case and therefore
[BlockLayoutAlgorithm] lays out this [LayoutListItem].

### Propagating unpositioned list markers

List markers can be processed either in [InlineLayoutAlgorithm]
if it appears within an inline formatting context,
or in [BlockLayoutAlgorithm]
if it appears within a block formatting context,
but its positioning is determined when [LayoutListItem] is laid out.

To do this, algorithms can set an unpositioned list marker to [LayoutResult],
which will be propagated to the nearest [LayoutListItem],
similar to absolute positioned objects propagate to its containing blocks.

### The spec and other considerations

The [CSS Lists and Counters Module Level 3],
which is not ready for implementation as of now,
defines that a list marker is an out-of-flow box.
A discussion is going on to define this better
in [csswg-drafts/issues/1934].

Logically speaking,
making the list marker out-of-flow looks the most reasonable
and it can avoid anonymous blocks at all.
However, doing so has some technical difficulties, such as:

1. The containing block of the list marker is changed to
the nearest ancestor that has non-static position.
This change makes several changes in the code paths.

2. We create a layer for every absolutely positioned objects.
This not only consumes memory,
but also changes code paths and make things harder.
Making it not to create a layer is possible,
but it creates a new code path for absolutely positioned objects without layers,
and more efforts are needed to make it to work.

In [csswg-drafts/issues/1934],
Francois proposed zero-width in-flow inline block,
while Cathie proposed zero-height in-flow block box.

It will need further discussions to make this part interoperable
and still easy to implement across implementations.

[anonymous block boxes]: https://drafts.csswg.org/css2/visuren.html#anonymous-block-level
[css-lists-3]: https://drafts.csswg.org/css-lists-3/
[csswg-drafts/issues/1934]: https://github.com/w3c/csswg-drafts/issues/1934
[list-style-position]: https://drafts.csswg.org/css-lists-3/#propdef-list-style-position
[marker positioning]: https://drafts.csswg.org/css-lists-3/#positioning

[BlockLayoutAlgorithm]: ../block_layout_algorithm.h
[InlineItem]: ../inline/inline_item.h
[InlineLayoutAlgorithm]: ../inline/inline_layout_algorithm.h
[LayoutListItem]: layout_list_item.h
[LayoutInsideListMarker]: layout_inside_list_marker.h
[LayoutOutsideListMarker]: layout_outside_list_marker.h
[LayoutResult]: ../layout_result.h
[PaintLayer]: ../../paint/paint_layer.h
