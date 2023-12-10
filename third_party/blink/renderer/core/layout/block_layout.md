# Block Layout #

This document can be viewed in formatted form [here](https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/layout/block_layout.md).

## BFC & BFC Offsets ##

A block formatting context (BFC) can be thought of as a completely new and
separate block flow layout. BFCs cannot interfere with other BFCs within the
page.

Concretely this means that margins do not collapse across BFC boundaries (see
below) and floats do not intrude, the exclusion space is completely separate.

Our block layout implementation is based on the principle that children place
*themselves* within the block formatting context. This information is
communicated with the optional [LayoutResult::BfcOffset](layout_result.h).

A child's BfcOffset is optional as empty blocks cannot place themselves within
the BFC. They may be affected by siblings.

Additionally when we start performing layout on a child, we don't know if it
will be an empty block.

---

When a child can determine its BfcOffset, (we call this informally "resolving
the BFC offset"), a parent can also determine its BfcOffset. Once a BfcOffset
is resolved it *might* "bubble" up the ancestor chain.

The calculation we perform to when the BfcOffset is "bubbled up" to the
ancestor which already knows its BfcOffset.

## Floats ##

Floats add a lot of complexity to block layout. Broadly there are two types of
floats that we need to consider.

 1. Unpositioned floats - floats which we cannot position immediately.

 2. Positioned floats - floats which we can position immediately.

```html
<div id="bfc" style="width: 100px;">
  <div id="float1" style="float: right; width: 50px; height: 50px;"></div>
  <div id="container">
    <div id="float2" style="float: right; width: 50px; height: 50px;"></div>
    <!-- margin-top here affects where #float2 will be placed. -->
    <div id="inflow" style="margin-top: 40px; margin-top: 60px;">
       text
    </div>
  </div>
</div>
```

In the above example we can position #float1 immediately as its parent knows
its BfcOffset.

We can't position #float2 immediately as #inflow's margins will affect
where #container is placed. This is an *unpositioned float*. Depending on
what #inflow's margins will be, #float2 may be positioned beside #float1, or
below #float1.

Once something resolves its BfcOffset (in the above case the "text" will be the
first node which can resolve its BfcOffset) we abort the layout telling the
ancestor chain about the new BFCOffset, and restart layout. Inside this second
pass, all unpositioned floats now position themselves immediately based on a
FloatsBFCOffset.

---

Once a float is positioned, everything else float related is handled by the
[ExclusionSpace](exclusions/exclusion_space.h).

Mutating the exclusion space only happens by adding additional exclusions.

The exclusion space supports a few queries related to floats.

  1. Finding a layout opportunity of a minimum size. This is used for
     positioning other floats inside the exclusion space, placing line boxes,
     and positioning new formatting contexts (things which avoid floats).

  2. The clearance block offset, for clearing left, right, or both floats.

  3. The start block offset of the last float positioned in the exclusion
     space. This is used for the "top edge alignment rule" for floats.

## An introduction to margin collapsing ##

A simple way to think about [margin collapsing](https://www.w3.org/TR/CSS2/box.html#collapsing-margins)
is that it takes the maximum margin between two elements. For example:

```html
<!-- The divs below are 20px apart -->
<div style="margin-bottom: 10px;">Hi</div>
<div style="margin-top: 20px;">there</div>
```

This is complicated by _negative_ margins. For example:

```html
<!-- The divs below are 10px apart -->
<div style="margin-bottom: 20px;">Hi</div>
<div style="margin-top: -10px;">there</div>

<!-- The divs below are -20px apart -->
<div style="margin-bottom: -20px;">Hi</div>
<div style="margin-top: -10px;">there</div>
```

The rule here is: `max(pos_margins) + min(neg_margins)`. This rule we'll refer
to as the _margin collapsing rule_. If this only happened between top level
elements it would be pretty simple, however consider the following:

```html
<!-- The top-level divs below are -2px apart -->
<div style="margin-bottom: 3px">
  <div style="margin-bottom: -5">
    <div style="margin-bottom: 7px">Hi</div>
  </div>
</div>
<div style="margin-top: 11px">
  <div style="margin-top: -13px">there</div>
</div>
```

In the above example as there isn't **anything** separating the edges of two
fragments the margins stack together (e.g. no borders or padding). There are
known as **adjoining margins**.  If we apply our formula to the above we get:
`max(3, 7, 11) + min(-5, -13) = -2`.

A useful concept is a **margin strut**. This is a pair of margins consisting of
one positive and one negative margin.

A margin strut allows us to keep track of the largest positive and smallest
negative margin. E.g.
```cpp
struct MarginStrut {
  LayoutUnit pos_margin;
  LayoutUnit neg_margin;

  void Append(LayoutUnit margin) {
    if (margin < 0)
      neg_margin = std::min(margin, neg_margin);
    else
      pos_margin = std::max(margin, pos_margin);
  }

  LayoutUnit Sum() { return pos_margin + neg_margin; }
}
```

A naïve algorithm for the adjoining margins case would be to _bubble_ up
margins. For example each fragment would have a **margin strut** at the
block-start and block-end edge. If the child fragment was **adjoining** to its
parent, you simply keep track of the margins by calling `Append` on the margin
strut. E.g.

```cpp
// fragment1 is the first child.
MarginStrut s1 = fragment1.block_start_margin_strut;
s1.Append(node1.style.margin_start);

builder.SetStartMarginStrut(s1);

// fragment2 is the last child.
MarginStrut s2 = fragment2.block_end_margin_strut;
s2.Append(node2.style.margin_start);

builder.SetEndMarginStrut(s2);
```

When it comes time to collapse the margins you can use the margin collapsing
rule, e.g.
```cpp
MarginStrut s1 = fragment1.block_end_margin_strut;
MarginStrut s2 = fragment2.block_start_margin_strut;
LayoutUnit distance =
    std::max(s1.pos_margin, s2.pos_margin) +
    std::min(s1.neg_margin, s2.neg_margin);
```

This would be pretty simple - however it doesn't work. As we discussed in the
floats section a _child_ will position _itself_ within the BFC. If we did margin
collapsing this way we'd create a circular dependency between layout and
positioning. E.g. we need to perform layout in order to determine the
block-start margin strut, which would allow us to position the fragment, which
would allow us to perform layout.

We **invert** the problem. A fragment now only produces an _end_ margin strut.
The _start_ margin strut becomes an input as well as where the margin strut is
currently positioned within the BFC.  For example:

```cpp
Fragment* Layout(LogicalOffset bfc_estimate, MarginStrut input_strut) {
  MarginStrut curr_strut = input_strut;
  LogicalOffset curr_bfc_estimate = bfc_estimate;
  
  // We collapse the margin strut which allows us to compute our BFC offset if
  // we have border or padding. I.e. we don't have an adjoining margin.
  if (border_padding.block_start) {
    curr_bfc_estimate += curr_strut.Sum();
    curr_strut = MarginStrut();

    fragment_builder.SetBfcOffset(curr_bfc_estimate);
    curr_bfc_estimate += border_padding.block_start;
  }

  for (const auto& child : children) {
    curr_strut.Append(child.margins.block_start);
    const auto* fragment = child.Layout(curr_bfc_estimate, curr_strut);

    curr_strut = fragment->end_margin_strut;
    curr_strut.Append(child.margins.block_end);

    curr_bfc_estimate = fragment->BfcOffset() + fragment->BlockSize();
  }

  fragment_builder.SetEndMarginStrut(curr_strut);

  return fragment_builder.ToFragment();
}
```

It isn't immediately obvious that this works, but if you try and work through an
example manually, it'll become clearer.

There are lots of different things which can "resolve" the BFC offset of an
element. For example inline content (text, atomic inlines), border and padding,
if a child _might_ be affected by clearance.

## Zero block-size fragments ##

TODO.

