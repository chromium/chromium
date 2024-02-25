# Block fragmentation tutorial #

*How to add block fragmentation support to a layout algorithm*

This tutorial can be viewed in formatted form
[here](https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/layout/block_fragmentation_tutorial.md).

Main spec: https://www.w3.org/TR/css-break-3/

---

## Overview ##

Any layout algorithm for block nodes takes ConstraintSpace, BlockNode and
BlockBreakToken as input, and writes output to BoxFragmentBuilder, which
will eventually generate an PhysicalBoxFragment wrapped inside an
LayoutResult. This will serve as input to the parent algorithm, which will
eventually add the child fragment to their output, i.e. BoxFragmentBuilder (or
abort / finish without doing so). Rather than having each layout algorithm
implement block fragmentation on its own, we have a shared fragmentation
machinery, which mainly consists of utility functions in
[fragmentation_utils.cc](fragmentation_utils.cc). This way each layout
algorithm can easily hook up with the various stages or aspects of block
fragmentation of a node. The utility functions will perform the relevant
operations on these core NG structures (ConstraintSpace, BlockNode,
BlockBreakToken, LayoutResult, and so on).

The purpose of the fragmentation machinery is to find the ideal (most appealing)
places to break in one
[fragmentainer](https://www.w3.org/TR/css-break-3/#fragmentainer) and resume in
the next. Finding the ideal place to break means identifying each possible break
point, and picking the best one. Essentially, there's a [valid
breakpoint](https://www.w3.org/TR/css-break-3/#possible-breaks) *between* each
*sibling* in the block direction, but generally not between a *parent* and its
first or last *child*. How appealing each of them are, and whether we're going
to break there or not, depends on various CSS properties that control breaking.

The golden rule of breaking (if we need to break at all):

* **Break at the breakpoint with the highest appeal (primary criterion) that
  also fits as much content as possible (secondary criterion).**

(more about *break appeal* later)

There are some very suitable breakpoints, such as between two block children,
between two lines (block container layout only), or between two table rows
(table layout only), as long as there are no breaking restrictions. There may be
breaking restrictions imposed by certain CSS properties (['orphans' and
'widows'](https://www.w3.org/TR/css-break-3/#widows-orphans) (block container
layout only), or the 'avoid' value in the ['break-after',
'break-before'](https://www.w3.org/TR/css-break-3/#break-between),
['break-inside'](https://www.w3.org/TR/css-break-3/#break-within) properties),
which will decrease the appeal of breaking right there.

Then there are some bad (actually invalid) breakpoints, such as breaking after
the block-start border edge of one block and before a first child that's flush
with the block-start border edge of said block (which we'll nevertheless end up
using in some cases, since it might be better than letting monolithic content
overflow the fragmentainer block-end).

Finally, there are places where it's impossible to break inside. This is what we
call [monolithic](https://www.w3.org/TR/css-break-3/#monolithic) content. Line
boxes and images are examples of monolithic content. We never break inside
those. You don't slice a line so that the top half ends up at the bottom of one
page and the bottom half ends up at the top of the next page. If monolithic
content doesn't fit in the current fragmentainer, it will have to be pushed to
the next fragmentainer. If it doesn't fit there, either, it will overflow the
fragmentainer. This is what we want to avoid at all costs, but if monolithic
content is taller than the fragmentainer, there's really nothing we can do about
it.

[GetConstraintSpace().HasBlockFragmentation()](constraint_space.h) returns
true for anything that *participates* in block fragmentation. A multicol
container *establishes* a [fragmentation
context](https://www.w3.org/TR/css-break-3/#fragmentation-context). It is a
fragmentation context root. Printing also establishes a fragmentation context,
for the entire document. All descendants of a fragmentation context root
participate in the fragmentation context, except when *inside*
[monolithic](https://www.w3.org/TR/css-break-3/#monolithic) (truly unbreakable)
elements. Nested fragmentation contexts are possible (multicol in multicol, for
instance, or, multicol in printing), in which case fragmentation of the
innermost descendants will be affected by all the enclosing fragmentation
contexts.

```html
<body>
  <div style="columns:3;">
    <div class="participant">
      <div style="columns:3;" class="participant">
        <div style="contain:size;" class="participant">
          <div style="height:50px;">
            <div style="columns:3;">
              <div style="height:50px;" class="participant">
```

The elements tagged with class="participant" here participate in one or more
fragmentation contexts. The other elements don't. Note that [contain:size makes
an element
monolithic](https://www.w3.org/TR/css-contain-1/#size-containment-box).

## Before child layout ##

The first thing to do for a node that is to participate in block fragmentation
(and thus become fragmented if needed) is for its parent layout algorithm to
hook it up with the fragmentation machinery when setting up the constraint space
for that node. Every algorithm (fragmented or not) needs to set up a constraint
space for each child somehow, and this is done by setting up an
ConstraintSpaceBuilder. This is where we need to add a call to
[SetupSpaceBuilderForFragmentation()](fragmentation_utils.h) as the final
step before constructing the constraint space. This sets up the
[fragmentainer](https://www.w3.org/TR/css-break-3/#fragmentainer) (column /
page) block-size, and block-offset into the fragmentainer, so that we can tell
how much space we can use before we need to insert a break. See
[FragmentainerSpaceLeft()](fragmentation_utils.h). If content is
[monolithic](https://www.w3.org/TR/css-break-3/#monolithic) (i.e. truly
unbreakable), [LayoutInputNode::IsMonolithic()](layout_input_node.h) will
return true. Being monolithic means that we cannot break inside the element (but
we may break before or after it).

([Fragment builders](box_fragment_builder.h) also need to be
fragmentation-aware. This is automatically taken care of by the
[LayoutAlgorithm](layout_algorithm.h) constructor, so algorithm
implementors need not worry about this.)

**Important:** When laying out a child that participates in block fragmentation,
we always need to know the block-offset (somehow relatively to the
fragmentainer) of that child *before* entering layout, in order to tell how much
space we have available. This requires special attention in some algorithms. For
instance, regular block container layout [collapses the
margins](https://www.w3.org/TR/CSS22/box.html#collapsing-margins) of certain
descendants. Margin collapsing affects the block-offset of a child, and it
therefore needs to take place *before* block fragmentation can start. The block
layout algorithm has the concept of
[BFC](https://www.w3.org/TR/CSS22/visuren.html#block-formatting) block-offset,
which will be unknown until we're done with margin collapsing. At that point the
algorithm will abort, so that nodes can be laid out at their correct
block-offset. In addition to block fragmentation, this is also important for
positioning of floats. There's a parameter to
SetupSpaceBuilderForFragmentation() for specifying the BFC block-offset.

## During child layout ##

Figuring out exactly where to break is a rather complex task that's based on
various input and state, such as available space, break avoidance requests /
hints (e.g. break-inside:avoid), orphans and widows. There are also forced
breaks (e.g. break-before:column), which trumps every break avoidance hint, and
will insert a break at the [nearest
valid](https://www.w3.org/TR/css-break-3/#break-propagation) [class
A](https://www.w3.org/TR/css-break-3/#btw-blocks) breakpoint.

This is all handled by the fragmentation machinery, though, and the short story
is that all the layout algorithm needs to do when laying out children, is:

* Figure out where to begin - which child is the first that wasn't fully laid
  out in the previous fragmentainer (if any)? Check the incoming break token for
  the node that was passed to the layout algorithm, by calling
  [GetBreakToken()](layout_algorithm.h). There will be a child break token
  for each unfinished child. Block break tokens form a tree. Children are found
  in [BlockBreakToken::ChildBreakTokens()](block_break_token.h). These will
  need to be resumed and laid out by passing the corresponding child break token
  to [LayoutInputNode::Layout()](layout_input_node.h). When done with all
  the incoming child break tokens, and as long as we didn't break again, proceed
  with the consecutive siblings of the node associated with the last incoming
  child break token.

* After a child is laid out, we need to call into the fragmentation utility
  machinery to determine whether to stop or continue layout. This should be done
  before adding the child to the fragment builder (and we should only add the
  child if we are allowed to continue).

The utility function [BreakBeforeChildIfNeeded()](fragmentation_utils.h) is
typically a suitable hook into the fragmentation machinery which an algorithm
can call after layout of a child, but before adding it to the fragment
builder. This function should be sufficient for most algorithms. However, note
that the block layout algorithm in particular doesn't use it, because it needs
to do some extra work for the calculation of [orphans and
widows](https://www.w3.org/TR/css-break-3/#widows-orphans), which is specific to
block containers. Instead, it has its own implementation, in
[BlockLayoutAlgorithm::BreakBeforeChildIfNeeded()](block_layout_algorithm.cc),
that performs additional logic for handling orphans and widows.

What BreakBeforeChildIfNeeded() returns determines how to proceed. We'll either:
* kContinue: Add the child fragment to the builder, and if there's no in-flow
  break inside, continue to the next sibling. Otherwise, stop and finish layout.
* kBrokeBefore: We broke before the child because we're out of space or because
  there's a forced break. Thus, stop and finish layout, but don't add the child
  to the builder. The fragment generated doesn't fit and will just be dropped on
  the floor, but a break token for it will have been created. This break token
  will be used to resume layout in the next fragmentainer.
* kNeedsEarlierBreak: Abort and re-layout to break at an earlier breakpoint that
  we drove past (i.e. there wasn't room for the child, but breaking before it is
  not ideal, so we won't do that, since we found something better earlier).

If (and only if) BreakBeforeChildIfNeeded() returns kContinue, should the
algorithm add the child to the fragment builder. After the child has been added
to the builder, check the return value of
[BoxFragmentBuilder::HasInflowChildBreakInside()](box_fragment_builder.h)
to determine if there was a same-flow break inside the builder. If there is no
same-flow break, the next child can be laid out. Otherwise, the algorithm should
finish. In this case, a break token has already been created, so the node will
resume layout in the next fragmentainer.

If we decide to break before a child (either directly because of a kBrokeBefore
return value, or, when re-laying out to an earlier break, because of
kNeedsEarlierBreak), the parent needs to [consume all remaining space in the
fragmentainer](https://www.w3.org/TR/css-break-3/#box-splitting). How to do this
is algorithm-specific. BlockLayoutAlgorithm has
ConsumeRemainingFragmentainerSpace(), for instance.

## At the end of layout ##

Unless we decided to abort and retry layout, when finishing layout of a node,
regardless of whether break was inserted or not, an algorithm needs to call
[FinishFragmentation()](fragmentation_utils.h). This may also cause a break,
unless one was already inserted: some nodes have a specified block-size, which
will be applied at the end of layout. If this is larger than what we can fit in
the current fragmentainer, we need to break inside the node. If breaking inside
is unappealing, and we've found something better earlier, though, we need to
behave as if kNeedsEarlierBreak was returned from a BreakBeforeChildIfNeeded(),
which means that we need to rerun layout and break at the best earlier
breakpoint.

When calculating the final size, we need to add the space taken up by previous
fragments to the calculated intrinsic block-size, when passing it to
ComputeBlockSizeForFragment(). Knowing the total block-size from all fragments
allows us to constrain the block-size correctly, since the relevant computed CSS
values know nothing about fragmentation. The space taken up by previous
fragments can be retrieved from
[GetBreakToken()->ConsumedBlockSize()](block_break_token.h).

As mentioned earlier, normally the right thing to do before performing block
fragmentation steps is to check if
[GetConstraintSpace().HasBlockFragmentation()](constraint_space.h) returns
true, but there are situations where we may already have generated fragments
from a node, but need to stop it from fragmenting any further. This happens when
overflow is clipped
[(overflow:clip)](https://www.w3.org/TR/css-overflow-3/#valdef-overflow-clip). In
such situations, GetConstraintSpace().HasBlockFragmentation() will return false,
but we still need to pick up any previous break token to resume layout
correctly. In cases where this distinction matters (like here - since the
previous break token is necessary in order to calculate the final block-size, if
specified), [InvolvedInBlockFragmentation()](fragmentation_utils.h) should be
called instead. This situation is detected in FinishFragmentation(), and
kDisableFragmentation will be returned then, which means that we need to abort
layout and retry without fragmentation.

## Break appeal ##

Some breakpoints are more appealing, while others violate certain breaking
rules. We'll always break at the breakpoint with the highest appeal that's
closer to the end of the fragmentainer (remember the golden rule mentioned
earlier). [BreakAppeal](break_appeal.h) defines the appeal values, ranging
from kBreakAppealPerfect, which is used when no breaking rules are violated at
all, to kBreakAppealLastResort, which means an invalid breakpoint, that we'll
sometimes end up breaking at nevertheless (if there are no valid breakpoints
available), as a last resort to prevent
[monolithic](https://www.w3.org/TR/css-break-3/#monolithic) content from
overflowing.

## Break tokens, resuming in the next fragmentainer ##

When we break before or inside a node, we attach a [break
token](block_break_token.h) to the resulting fragment. The break tokens form
a tree structure for each parent we need to break and resume inside of, which
we'll traverse when resuming layout in the next fragmentainer. See for instance
[BlockChildIterator](block_child_iterator.h), and how it makes use of the
break token tree structure during child layout in
[BlockLayoutAlgorithm::Layout()](block_layout_algorithm.cc).

Note that the mere existence of a break token for a node doesn't imply that any
fragment has been generated for a node, since we also create break tokens
*before* nodes, not just inside them. If we want to know if we're actually
resuming layout of a node, we also need to check that it's not a break-before
break token (BlockBreakToken::IsBreakBefore()). There's a utility function for
this: [IsBreakInside()](fragmentation_utils.h).

Break tokens are attached to each fragment that breaks inside. Child fragments
that broke inside have an entry in the child list
([BlockBreakToken::ChildBreakTokens()](block_break_token.h)). There is also
a child break token for each child node that we need to break *before*.

For multicol layout, a break token tree is built all the way up to the
containing column fragment, and there it will serve as input to the next
column. The column fragments are produced by the regular
[BlockLayoutAlgorithm](block_layout_algorithm.h), while the multicol
container itself produces multicol container fragments (we may be nested inside
of another fragmentation context; otherwise there'll only be one multicol
container fragment) using
[ColumnLayoutAlgorithm](column_layout_algorithm.h).

Example:

```html
<div style="columns:2; height:100px; column-fill:auto;">
  <div id="wrapper1">
    <div id="block1" style="contain:size; height:10px;"></div>
    <div id="wrapper2">
      <div id="block2" style="contain:size; height:70px;"></div>
      <div id="block3" style="contain:size; height:70px;"></div>
      <div id="block4" style="contain:size; height:10px;"></div>
    </div>
  </div>
</div>
```

We'll run out of space in the first column before #block3 (it's monolithic due
to contain:size). We'll create a break token for #block3
([BlockBreakToken::IsBreakBefore()](block_break_token.h) will return true
for that one). We'll finish the fragment for #wrapper2, create a break token for
it, and put the break token for #block3 inside. We'll finish the fragment for
#wrapper1, create a break token for it, and put the break token for #wrapper2
inside. We'll finish the column fragment, create a break token for it, and put
the break token for #wrapper1 inside. Done with the first column!

We have this break token structure as input when laying out the second column:

```
Column
|
+-- #wrapper1
     |
	 +-- #wrapper2
	      |
		  +-- #block3
```

We'll start laying out the second column by first examining the incoming child
break tokens. There's one for #wrapper1. Enter layout of #wrapper1, and pass the
break token for #wrapper1. Examine the child break tokens. There's one for
#wrapper2. We'll ignore #block1 (it was finished in the first column), and enter
layout of #wrapper2, and pass the break token for #wrapper2. Examine the child
break tokens. There's one for #block3. We'll ignore #block2 (it was finished in
the first column), and enter layout of #block3, and pass the break token for
#block3 (this is a break-before break token, so we will start layout from
scratch, not really resume anything). When we're done with #block3 inside
#wrapper2, there are no more break tokens left to process, so we'll proceed to
the siblings of #block3. Lay out #block4. It also fits. And we're done. Nothing
broke, so no break tokens needed this time. Finish the fragments for #wrapper2,
#wrapper1 and the second column.

There are also cases where no child inside broke, but we still need to break
inside the parent, for instance when it has a specified block-size that doesn't
fit:

```html
<div style="columns:2; height:100px; column-fill:auto;">
  <div id="container" style="height:150px;">
	<div id="child" style="height:10px;"></div>
  </div>
  <div id="next" style="height:20px;"></div>
</div>
```

#child fits just fine inside #container in the first column, but when finishing
layout of #container, and calculating the block-size, it turns out that it's too
tall. This takes place in [FinishFragmentation()](fragmentation_utils.h). So
we need a break inside. We end up with this break token structure as input to
the second column:

```
Column
|
+-- #container
```

We'll resume inside #container. It has no child break tokens. What does this
mean? Start from scratch? No. We finished #child in the first column. Laying it
out again here would be bad (infinite loop generating an infinite amount of
columns not getting anywhere, proabably). This is where
[BlockBreakToken::HasSeenAllChildren()](block_break_token.h) comes into
play. No child break tokens either means that we're done with the children, or
that we haven't started yet. HasSeenAllChildren() will tell us what to do. In
this case it returns true, so we'll just finish layout.
[GetBreakToken()->ConsumedBlockSize()](block_break_token.h) will be used to
tell us how much space we actually need to fit in this column. We were able to
fit 100px in the previous column, so that's our previously consumed block-size.
The specified height is 150px, so we need room for another 50px, which will
fit. Create another fragment for #container and return to the parent layout
algorithm (the BlockLayoutAlgorithm for the second column). The column only
had one break token child, i.e. the one for #container. We'll proceed with its
sibling #next, which will also fit in the second column. And we're done.

## Aborting and re-running layout ##

### Re-running to an earlier breakpoint ###

If BreakBeforeChildIfNeeded() or FinishFragmentation() returns
kNeedsEarlierBreak, we ran out of space at an unappealing breakpoint. It also
means that we have actually found a better earlier breakpoint (further up in the
fragmentainer). When this happens, we need to abort and rerun layout, but this
time with a parameter that says exactly where to stop and break. Early breaks
are stored in LayoutResult when found. See LayoutResult::GetEarlyBreak() and
the [EarlyBreak](early_break.h) structure.

An algorithm can be rerun to break at the appealing early breakpoint by passing
said early breakpoint to the algorithm's constructor. This can be done in the
algorithm by calling and returning the result from
[LayoutAlgorithm::RelayoutAndBreakEarlier()](layout_algorithm.h). This will
set the early_break_ member and relayout. Any compliant algorithm needs to
check if an early break has been set, and, if so, break when reaching that
node.

EarlyBreak forms a container chain, so that we know where to enter, and where
to stop in the second layout pass. If there's an early break set, before
entering every child, [IsEarlyBreakTarget()](fragmentation_utils.h) may be
called (as long as the algorithm only deals with block nodes). If it returns
true, this is where to break. Otherwise, before laying out a child,
[EnterEarlyBreakInChild()](fragmentation_utils.h) should be called, and
the return value should be passed to the layout algorithm of the child.

### Re-running without block fragmentation ###

If [FinishFragmentation()](fragmentation_utils.h) returns
kDisableFragmentation, it means that a clipped node isn't allowed to fragment
any further, because we've reached the end, and anything past that point will be
clipped (and we don't want any additional fragmentainers generated just to hold
empty clipped fragments). When we've reached the end, but some child still
claims more space than available in the current fragmentainer, we need to abort
and relayout without block fragmentation, so that the node from this point will
be treated as monolithic.

The correct response to this is to abort layout of the node and relayout using
[LayoutAlgorithm::RelayoutWithoutFragmentation()](layout_algorithm.h).

## Parallel flows ##

With kContinue from BreakBeforeChildIfNeeded(), we may continue as long as there
are no breaks, or if all breaks found so far occur in [parallel
flows](https://www.w3.org/TR/css-break-3/#parallel-flows), such as inside a
float. If a float breaks inside, we will have stopped laying out descendants of
the float, but that's a parallel flow, and there may be more content to fit in
the same flow as the node.

Consider this example:

```html
<div style="columns:2; column-fill:auto; height:100px; line-height:20px;">
  <div id="container">
    <div id="block1">
      LINE 1<br>
      LINE 2<br>
      LINE 3<br>
    </div>
    <div id="float" style="float:left;">
      line 1<br>
      line 2<br>
      line 3<br>
      line 4<br>
      line 5<br>
    </div>
    <div id="block2">
      LINE 1<br>
      LINE 2<br>
    </div>
  </div>
</div>
```

In the first column of #container, we'll fit all of #block1. The #float will
break inside, between line 2 and line 3. Layout of the *float* in the first
column will stop right there, since we ran out of space in the *flow established
by the float*. However, since a float establishes a flow parallel to its parent,
we won't stop layout of #container, but rather just proceed to #block2, all of
which will actually fit in the first column.

Compare to how it works when everything is in the same flow (i.e. no parallel
flows):

```html
<div style="columns:2; column-fill:auto; height:100px; line-height:20px;">
  <div id="container">
    <div id="block1">
      line 1<br>
      line 2<br>
      line 3<br>
    </div>
    <div id="block2">
      line 1<br>
      line 2<br>
      line 3<br>
      line 4<br>
    </div>
    <div id="block3">
      line 1<br>
      line 2<br>
    </div>
  </div>
</div>
```

In the first column of #container, we'll fit all of #block1, and proceed to
#block2, which will break before its line 3. This will also stop layout of
#container (we will NOT proceed to #block3), since it's in the same flow.
We'll resume layout of #block2 inside #container in the second column, and then
proceed to #block3.

There are many examples of parallel flows, some unique to certain layout
types. Sibling table cells, for instance are in parallel flows within the same
table row. Inserting a break (even a forced one) inside one cell doesn't affect
where we insert breaks inside the other cells. Similar for flex items on the
same line (when flex-flow is 'row').

## The end? ##

That concludes this tutorial, and the previous chapter was pretty boring. But if
it really wasn't too much, here's a bonus chapter:

## Gory machinery details, with a walk-through ##

While callers really only need to worry about passing the right input and acting
on the return value from BreakBeforeChildIfNeeded(), it might be useful to take
a "quick" look at what that function does:

In short: BreakBeforeChildIfNeeded() examines the possible breakpoint
established before the child that is about to be added, and assesses the need
and appeal of breaking before the child. Child layout is depth-first traversal,
so that when we get to BreakBeforeChildIfNeeded() for some child, the child will
already be broken inside if something didn't fit there.

The first thing that takes place: if there's a forced break at the break point,
insert a break before the child and return kBrokeBefore. Otherwise, we'll
attempt to move past the break point
([MovePastBreakpoint()](fragmentation_utils.h)). If we can't do that
(i.e. we're out of space), we need to break.

Details:

* Make sure that the break-before value of the first child and the break-after
  value of the last child are propagated to the nearest container that isn't a
  first / last child, which is where the [property will have an
  effect](https://www.w3.org/TR/css-break-3/#break-propagation).

* Without actually checking whether we're out of space, calculate the appeal of
  breaking at the breakpoint before the child fragment. If it's the first child,
  that will typically result in the lowest possible appeal -
  kBreakAppealLastResort (remember, we should always break *between*
  *siblings*). Any break-before or break-after properties that apply to the
  break point will be considered, and may affect the appeal (break-after:avoid
  on the previous sibling, for instance, would decrease the appeal).

* If the child fragment fits in the current fragmentainer, and the child didn't
  break inside, and breaking before has the same appeal or higher than the
  previously found "best" break point, store this breakpoint as the current
  early break candidate (in the fragment builder). This may end up becoming the
  actual place to break, if we don't find anything better. This is stored as an
  [EarlyBreak](early_break.h) structure, which forms a container chain,
  which we will have to enter if we need an early break. Return kContinue for
  now.

* If the child fragment doesn't fit in the current fragmentainer (this means
  that there's something monolithic (a line box or replaced content, for
  instance) preventing us from breaking freely), and the break appeal is the
  same or higher than all previous breakpoints, this (before the child) is where
  we're going to break. Return kBrokeBefore.

* If the child fragment fits in the current fragmentainer, but the child broke
  inside, check the appeal of breaking inside. If it's at least as high as the
  previously found most appealing break (including the opportunity of breaking
  *before* the child), we'll allow that break inside and return
  kContinue. Otherwise, if the appeal of breaking before the child is at least
  as high as that of the previously found break point, break before the child
  and return kBrokeBefore.

* If we're out of space, and the appeal of breaking before and inside are both
  lower than the appeal of an earlier breakpoint, we need to use said earlier
  breakpoint, which is stored in the fragment builder (and propagated from
  descendants via their [LayoutResult](layout_result.h)). Return
  kNeedsEarlierBreak. We'll need to re-run layout in a special mode, where the
  algorithm is told to break at a specified node (automatic breaking will be
  disabled). There may be nested situations to consider here. For example:

```html
<div style="columns:3; column-fill:auto; height:90px; background:yellow;">
  <div id="first" style="height:2px;"></div>
  <div id="problem">
    <div id="herring" style="height:2px;"></div>
    <div id="wrapper1">
      <div id="wrapper2">
        <div id="a" style="contain:size; height:40px;"></div>
        <div id="b" style="contain:size; height:40px;"></div>
      </div>
    </div>
    <div id="c" style="break-before:avoid; contain:size; height:40px;"></div>
  </div>
</div>
```

#a, #b and #c all have contain:size to make them monolithic, i.e. unbreakable
inside. There are four valid breakpoints: Between #first and #problem, between
#herring and #wrapper1, between #a and #b, and between #wrapper1 and
#c. Then there are some last-resort breaks, typically between each parent and
their first/last child. #first, #herring, #a and #b can all fit in the first
column, but we prefer not to break before #c (since it has break-before:avoid),
so (**spoiler alert!**) we'll end up breaking between #a and #b.

When laying out this multicol container, the following will happen:
1. Begin layout of the first column.
1. Lay out #first. There's no break point before it, since it's at the top of
   the fragmentainer, and is even the first child of the multicol container.
1. Enter #problem
1. Lay out #herring. There's a last-resort breakpoint before it (since it's a
   first child). Store it as a possible early break (even though it's really
   unappealing). Add #herring to the fragment builder of #problem.
1. Enter #wrapper1.
1. Enter #wrapper2.
1. Lay out #a. There's a last-resort breakpoint before it, and it's further down
   than the previously found last-resort breakpoint, so it's better (however
   unappealing it is). Add #a to the #wrapper2 fragment builder.
1. Lay out #b. There's a perfect breakpoint before it. Overwrite the previously
   found early (last-resort) break. Add #b to the builder. We still have room.
1. Finish #wrapper2. Its LayoutResult will contain the early break pointing
   before #b. There's a last-resort breakpoint before #wrapper2 (as the first
   child of #wrapper1), but we already have something better - the perfect one
   before #b.
1. Finish #wrapper1. Its LayoutResult will contain the early break pointing
   inside #wrapper2, which has an early break pointing before #b. There's a
   perfect breakpoint before #wrapper1, but the possible break inside (before
   #b) is further down, so keep that one.
1. Lay out #c. There's a valid but suboptimal (because of break-before:avoid)
   breakpoint before #c, so we won't use it. ...AND we're out of space! #c
   doesn't fit in the first column. We've already used 2px+2px+40px+40px =
   84px. #c needs 40px, but the fragmentainer size is 90px, so we only have room
   for another 6px. We need a break. If this had been a perfect place to break,
   we could have done just that, and finished #problem and then the first column
   entirely, but the place to break is before #b inside #wrapper2 inside
   #wrapper1.
1. Restart layout of #problem, and provide the EarlyBreak pointing at #b
   inside #wrapper2 inside #wrapper1.
1. Lay out #herring normally, and just add it to the fragment builder, since
   it's not the early break we're looking for.
1. Enter #wrapper1. Our early break is inside it. Peel off one layer of the
   EarlyBreak onion. Lay out #wrapper1 with an EarlyBreak pointing before #b
   inside #wrapper2.
1. Enter #wrapper2. Our early break is inside it. Peel off one layer of the
   EarlyBreak onion. Lay out #wrapper2 with an EarlyBreak pointing before
   #b.
1. Lay out #a, and just add it to the fragment builder, since it's not the early
   break we're looking for.
1. Found the early break - before #b. Break before it. Finish #wrapper2, then
   #wrapper1, then #problem, then the first column itself. The column fragment
   will now have a break token with one child: #problem. The break token for
   #problem will have one child break token for #wrapper1, which will have a
   child break token for #wrapper2, which will have a child break token for #b.
1. Continue in the second column, and pass the break token for #problem.
1. Skip anything preceding #problem (i.e. #first, which we finished in the
   previous column), and descend into #problem.
1. The #problem break token has a child break token for #wrapper1. Enter
   #wrapper1.
1. The #wrapper1 break token has a child break token for #wrapper2. Enter
   #wrapper2.
1. The #wrapper2 break token has a child break token for #b. Lay out #b (skip
   #a - we finished it in the previous column). There's no valid breakpoint
   before #b here, since it's the first piece of content in the (second)
   column. Add #b to the fragment builder.
1. Finish #wrapper2. No valid breakpoint before it, since we actually resumed
   *inside* it.
1. Finish #wrapper1. No valid breakpoint before it, since we actually resumed
   *inside* it.
1. No more child break tokens for #problem - we only had one for
   #wrapper1. Proceed to the next sibling, i.e. #c, and lay it out. Add it to
   the fragment builder.
1. Finish #problem
1. Finish the second column.
1. Done!
