# CSS Style Invalidation in Blink

[Rendered](https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/core/css/style-invalidation.md)

# About this document

The doc gives a high-level overview of the style invalidation process.
It does not cover [sibling invalidation](https://goo.gl/z0Z9gn).

# Overview of invalidation

Invalidation is the process
of marking which elements need their style recalculated
in response to a change in the DOM.
The simplest possible approach
is to invalidate everything in response to every change.

Invalidation sets give us a way
to find a smaller set of elements which need recalculation
They are not perfect,
they err on the side of correctness,
so we invalidate elements that do not need recalculation
but this are significantly better than recalculating everything.
An invalidation set represents

*   criteria for matching against a node
*   instructions for whether/how to descend the tree into the node's children

If we have a style rule `".c1 div.c2 { ... }"`
then style recalculation is needed
when a `c1`-class is added or removed
as an ancestor of a `c2`-class
or when a `c2`-class is added or removed
from a div that is a descendant of a `c1`-class element
(here adding/removing can be adding/removing an element
or just adding/removing these classes on existing elements).
We don't want to do full style recalc
at the time of adding/removing
because there may be more mutations coming.
If we can tell immediately that a change forces style recalc
then we mark the node as that,
otherwise we collect everything as pending invalidation sets.

In the example,
if a `c1`-class is added to an element in the tree,
we need to invalidate all of its descendants which have class `c2`.
Rather than perform a search right now,
we just mark the element with a pending invalidation set
that matches against `c2`
and descends into all light-descendants.
(We won't invalidate all div descendants, as class `c2` is more specific.)

If class `c2` is added to an element in the tree,
then it needs recalculation
if it has a `c1`-class ancestor.
We never search _up_ the tree at this point
or during style invalidation,
we only do that during recalculation,
so this becomes an immediate invalidation,
even though it may be unnecessary.
Similarly, we don't check if the `c2`-class element is a div.

Eventually all DOM changes have been turned into immediate invalidations
or pending invalidation sets.
At this point,
we apply all the pending invalidations
and then recalculate style for all of the invalidated elements.

For more details see the original [invalidation sets design doc](https://goo.gl/3ane6s)
and the [sibling invalidation design doc](https://goo.gl/z0Z9gn).

# State involved

## RuleFeatureSet

The data in RuleFeatureSet is built from the style rules
and does not change unless the style rules change.

## DOM Node's style states

DOM nodes have several bits of style-related state
that control style invalidation and recalculation.
These are accessed through:

* [`Node::NeedsStyleInvalidation`](https://cs.chromium.org/?q=symbol:%5Eblink::Node::NeedsStyleInvalidation$)
* [`Node::ChildNeedsStyleInvalidation`](https://cs.chromium.org/?q=symbol:%5Eblink::Node::ChildNeedsStyleInvalidation$)
* [`Node::NeedsStyleRecalc`](https://cs.chromium.org/?q=symbol:%5Eblink::Node::NeedsStyleRecalc$)
* [`Node::ChildNeedsStyleRecalc`](https://cs.chromium.org/?q=symbol:%5Eblink::Node::ChildNeedsStyleRecalc$)



## PendingInvalidationsMap

This is a map from
DOM Node to
invalidation sets that should be applied due to updates to that node.


# Processes


## Overview

The overview of style invalidation and recalculation is that

* Style rules are compiled down to a collection of InvalidationSets
  and other data
  in [`RuleFeatureSet`](https://cs.chromium.org/?q=symbol:%5Eblink::RuleFeatureSet$)
* The following process is then applied continuously
  * Changes to the DOM cause nodes or subtrees to be immediately invalidated
    or to accumulate pending invalidations.
  * A point is reached where style is being read
    (e.g. in order to render a new frame)
  * Style invalidation process finds all pending invalidations
    and decides on what will actually be recalculated
  * Style recalculation process recalculates style
    for all nodes that need it


## Building the RuleFeatureSet

Each [`RuleSet`](https://cs.chromium.org/?q=symbol:%5Eblink::RuleSet$)
produces its own
[`RuleFeatureSet`](https://cs.chromium.org/?q=symbol:%5Eblink::RuleFeatureSet$)
by calling [`CollectFeaturesFromRuleData`](https://cs.chromium.org/?q=symbol:%5Eblink::RuleFeatureSet::CollectFeaturesFromRuleData$)
for each [`RuleData`](https://cs.chromium.org/?q=symbol:%5Eblink::RuleData$).
These contain several indexed collections of [`InvalidationSet`](https://cs.chromium.org/?q=symbol:%5Eblink::InvalidationSet$)s
and some miscellaneous properties.

All of these are merged together to form a final [`RuleFeatureSet`](https://cs.chromium.org/?q=symbol:%5Eblink::RuleFeatureSet$)
which used for style purposes.


## Turning DOM changes into pending/immediate invalidations

Changes in the DOM that require updates to styles
may get turned into either immediate invalidations or pending invalidations.
When a DOM change that could impact style occurs inside a [`Node`](https://cs.chromium.org/?q=symbol:%5Eblink::Node$)
(e.g. a change in class name)
this leads to a call into [`StyleEngine`](https://cs.chromium.org/?q=symbol:%5Eblink::StyleEngine$)
to record this style-impacting change via one of several [`FooChangedForElement`](https://cs.chromium.org/?q=symbol:%5Eblink::StyleEngine::.*ChangedForElement$) methods.

Depending on the type of change,
[`StyleEngine`](https://cs.chromium.org/?q=symbol:%5Eblink::StyleEngine$) gathers the relevant [`InvalidationSet`](https://cs.chromium.org/?q=symbol:%5Eblink::InvalidationSet$)s
and calls [`PendingInvalidations::ScheduleInvalidationSetsForNode`](https://cs.chromium.org/?q=symbol:%5Eblink::PendingInvalidations::ScheduleInvalidationSetsForNode$)
which will do one or both of

* call [`Node::SetNeedsStyleInvalidation`](https://cs.chromium.org/?q=symbol:%5Eblink::Node::SetNeedsStyleInvalidation$)
  which ensures that the invalidation process will consider this node
  and add InvalidationSets for this node to the pending invalidation set map.
* call [`Node::SetNeedsStyleRecalc`](https://cs.chromium.org/?q=symbol:%5Eblink::Node::SetNeedsStyleRecalc$)
  with either [`kLocalStyleChange`](https://cs.chromium.org/?q=symbol:%5Eblink::StyleChangeType::kLocalStyleChange$) or [`kSubtreeStyleChange`](https://cs.chromium.org/?q=symbol:%5Eblink::StyleChangeType::kSubtreeStyleChange$)


## Pushing the pending invalidations

When style is about to be read,
the map of pending invalidations which has been built up
needs to be pushed.
For each [`ContainerNode`](https://cs.chromium.org/?q=symbol:%5Eblink::ContainerNode$) in the DOM tree
we have 0 or more descendant [`InvalidationSet`](https://cs.chromium.org/?q=symbol:%5Eblink::InvalidationSet$) waiting to be applied.
The invalidation process starts with a call to [`StyleInvalidator::Invalidate`](https://cs.chromium.org/?q=symbol:%5Eblink::StyleInvalidator::Invalidate$)
which recurses down the tree, depth first.
Read the method's inline documentation to understand more about the process.

# See Also

[Invalidation sets design doc](https://goo.gl/3ane6s)

[Sibling invalidation design doc](https://goo.gl/z0Z9gn)
