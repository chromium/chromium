# CSS Style Calculation in Blink

[Rendered](https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/core/css/style-calculation.md)


# About this document

This is is a description of how Blink calculates which style rules apply to each
element. It is not a comprehensive guide to the whole style computation/update
process but it should be accurate for what it does cover. Further additions are
welcome.

It ignores all V0 Shadow DOM logic.


# High-level view


## Process

The process of calculating styles for the elements is broken into 3 phases:

*   Gathering, partitioning and indexing the style rules present in all of the
    style sheets
*   Visiting each element and finding all of the rules which apply to that
    element
*   Combining those rules and other information to produce the final computed
    style


## Main classes

A catalogue of the classes involved. Read their class docs.

The following are long-lived objects that remain static during the calculation
of each element's style.

* [`Element`](https://cs.chromium.org/?q=symbol:%5Eblink::Element$) See also
[dom/README.md](https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/core/dom/README.md)
* [`TreeScope`](https://cs.chromium.org/?q=symbol:%5Eblink::TreeScope$)
Represents a tree of elements for a document or shadow root. Gives fast access
to various things inside the tree of elements. Holds a
[`ScopedStyleResolver`](https://cs.chromium.org/?q=symbol:%5Eblink::ScopedStyleResolver$)
for this scope. See
[dom/README.md](https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/core/dom/README.md#treescope)
* [`StyleEngine`](https://cs.chromium.org/?q=symbol:%5Eblink::StyleEngine$)
* [`StyleResolver`](https://cs.chromium.org/?q=symbol:%5Eblink::StyleResolver$)
* [`ScopedStyleResolver`](https://cs.chromium.org/?q=symbol:%5Eblink::ScopedStyleResolver$)
* [`TreeScopeStyleSheetCollection`](https://cs.chromium.org/?q=symbol:%5Eblink::TreeScopeStyleSheetCollection$)
* [`StyleRule`](https://cs.chromium.org/?q=symbol:%5Eblink::StyleRule$)
* [`RuleData`](https://cs.chromium.org/?q=symbol:%5Eblink::RuleData$)
* [`RuleSet`](https://cs.chromium.org/?q=symbol:%5Eblink::RuleSet$)

The following are short-lived objects that are used when computing a single
element's style.

* [`ElementResolveContext`](https://cs.chromium.org/?q=symbol:%5Eblink::ElementResolveContext$)
* [`StyleResolverState`](https://cs.chromium.org/?q=symbol:%5Eblink::StyleResolverState$)
* [`MatchRequest`](https://cs.chromium.org/?q=symbol:%5Eblink::MatchRequest$)
* [`ElementRuleCollector`](https://cs.chromium.org/?q=symbol:%5Eblink::ElementRuleCollector$)
* [`SelectorCheckingContext`](https://cs.chromium.org/?q=symbol:%5Eblink::SelectorChecker::SelectorCheckingContext$)
* [`SelectorChecker`](https://cs.chromium.org/?q=symbol:%5Eblink::SelectorChecker$)

# Compiling and indexing

When changes occur in the style sheet, either in an existing `TreeScope` or with
the introduction of a new `TreeScope`, the
[`ScopedStyleResolver`](https://cs.chromium.org/?q=symbol:%5Eblink::ScopedStyleResolver$)
for that scope must be updated. This is done by calling
[`AppendActiveStyleSheets`](https://cs.chromium.org/?q=symbol:%5Eblink::ScopedStyleResolver::AppendActiveStyleSheets$)
and passing in a collection of style sheets. These style sheets are appended to
the list of active style sheets in the `TreeScope` and also partitioned and
indexed by
[`FindBestRuleSetAndAdd`](https://cs.chromium.org/?q=symbol:%5Eblink::RuleSet::FindBestRuleSetAndAdd$). Within
each [`RuleSet`](https://cs.chromium.org/?q=symbol:%5Eblink::RuleSet$) are several maps of maps of `RuleData`
objects. `FindBestRuleSetAndAdd` looks at the right-most compound selector of each
selector and chooses which of these maps to hold this `RuleData`. E.g. the
selector `p.cname`'s right-most simple selector matches against `class="cname"`,
so this is added to the `ClassRules` map with a key of `"cname"`.

At the end of this process, each `TreeScope` in the document has a `ScopedStyleResolver` containing all of the style rules defined directly in that scope, partitioned into various `RuleSet`s.


# Calculating styles for each element - WIP

This guide starts with the simplest operation and works backwards.


## [`SelectorChecker::MatchSelector`](https://cs.chromium.org/?q=symbol:%5Eblink::SelectorChecker::MatchSelector$) - Checking if a rule applies to an element

[`Match`](https://cs.chromium.org/?q=symbol:%5Eblink::SelectorChecker::Match$+file:.h$)
is the public entrypoint and
[`MatchSelector`](https://cs.chromium.org/?q=symbol:%5Eblink::SelectorChecker::MatchSelector$+file:.h$)
is the recursive core of checking if a rule applies to an element. Read their docs and also
[`SelectorCheckingContext`](https://cs.chromium.org/?q=symbol:%5Eblink::SelectorChecker::SelectorCheckingContext$)
and
[`CheckOne`](https://cs.chromium.org/?q=symbol:%5Eblink::SelectorChecker::CheckOne$+file:.h$).

The whole process is started by
[`ElementRuleCollector::CollectMatchingRulesForList`](https://cs.chromium.org/?q=symbol:%5Eblink::ElementRuleCollector::CollectMatchingRulesForList)
which creates the initial `SelectorCheckingContext`, pointing to the element we
are considering and to first simple selector of the CSSSelector array. Read
[`CSSSelector`](https://cs.chromium.org/?q=symbol:%5Eblink::CSSSelector$)'s
class description to understand how complex selectors are represented by arrays
of `CSSSelector`s.


## [`StyleForLayoutObject`](https://cs.chromium.org/?q=symbol:%5Eblink::Element::StyleForLayoutObject$) - Calculating the computed style for an element

If there are no custom style callbacks or animations
[`StyleForLayoutObject`](https://cs.chromium.org/?q=symbol:%5Eblink::Element::StyleForLayoutObject$)
leads to
[`StyleResolver::ResolveStyle`](https://cs.chromium.org/?q=symbol:%5Eblink::StyleResolver::ResolveStyle$)
which is where the bulk of the work occurs. First by finding all of the rules
which match the element and then using that and other info to compute the final
style.


### Finding all rules which match the element

Blink considers all of the relevant style sheets for each element by
partitioning and indexing the rules in each stylesheet inside the
[`RuleSet`](https://cs.chromium.org/?q=symbol:%5Eblink::RuleSet$) object, Blink
is able to avoid considering many irrelevant rules for the current
element. E.g. if the element has `class="cname"` then Blink looks in the
RuleSet's `ClassRules` map under the key "cname" and considers all of the
`RuleData` objects found there. This allows it to avoid considering any rules
with selectors that end in `".othercname"` which would have been under
`"othercname"` in the `ClassRules` map.

In this way, Blink works through various lists of `RuleData` for the element
calling
[`CollectMatchingRulesForList`](https://cs.chromium.org/?q=symbol:%5Eblink::ElementRuleCollector::CollectMatchingRulesForList$)
on each list, how that works is described
[below](#CollectMatchingRulesForList).

Inside this method, context is set up that is used while calculating the style.

*   [`ElementResolveContext`](https://cs.chromium.org/?q=symbol:%5Eblink::ElementResolveContext$)
*   [`StyleResolverState`](https://cs.chromium.org/?q=symbol:%5Eblink::StyleResolverState$)
*   [`ElementRuleCollector`](https://cs.chromium.org/?q=symbol:%5Eblink::ElementRuleCollector$)

With all of this context set up, it calls
[`MatchAllRules`](https://cs.chromium.org/?q=symbol:%5Eblink::StyleResolver::MatchAllRules$)
which matches the rules from the

*   User Agent
*   User
*   Author (document's style) via
    [`MatchAuthorRules`](https://cs.chromium.org/?q=symbol:%5Eblink::StyleResolver::MatchAuthorRules$)

[`MatchAuthorRules`](https://cs.chromium.org/?q=symbol:%5Eblink::StyleResolver::MatchAuthorRules$)
splits applies the following steps (read the method docs):

- [`MatchHostRules`](https://cs.chromium.org/?q=symbol:%5Eblink::MatchHostRules$)
- [`MatchSlottedRules`](https://cs.chromium.org/?q=symbol:%5Eblink::MatchSlottedRules$)
- [`MatchElementScopeRules`](https://cs.chromium.org/?q=symbol:%5Eblink::MatchElementScopeRules$)
- [`MatchPseudoPartRules`](https://cs.chromium.org/?q=symbol:%5Eblink::StyleResolver::MatchPseudoPartRules$)


#### <a name="CollectMatchingRulesForList"></a>[`CollectMatchingRulesForList`](https://cs.chromium.org/?q=symbol:%5Eblink::ElementRuleCollector::CollectMatchingRulesForList$) - testing some rules against an element

This is at the core of all the code paths. It takes

*   list of [`RuleData`](https://cs.chromium.org/?q=symbol:%5Eblink::RuleData$)
*   [`MatchRequest`](https://cs.chromium.org/?q=symbol:%5Eblink::MatchRequest$)

This creates a `SelectorChecker` and `SelectorCheckerContext` for the element
and uses it to check try match, one by one, against each `RuleData` object in
the input list. If `checker.Match(context, result)` returns true then this rule
applies to this element and it is added to the collection with
[`DidMatchRule`](https://cs.chromium.org/?q=symbol:%5Eblink::ElementRuleCollector::DidMatchRule$).


### Computing style from matched rules

TODO


## Descending the DOM trees

[`Document::UpdateStyleAndLayoutTree`](https://cs.chromium.org/?q=symbol:%5Eblink::Document::UpdateStyleAndLayoutTree$)
is the starting point for computing or recomputing the styles of elements in the
document. This calls
[`UpdateActiveStyle`](https://cs.chromium.org/?q=symbol:%5Eblink::Document::UpdateActiveStyle$)
which calls
[`UpdateActiveStyle`](https://cs.chromium.org/?q=symbol:%5Eblink::Document::UpdateActiveStyle$)
and leads into the compiling and index [above](#compiling-and-indexing). Then it
calls
[`UpdateStyleInvalidationIfNeeded()`](https://cs.chromium.org/?q=symbol:%5Eblink::Document::UpdateStyleInvalidationIfNeeded$)
(see [here](README.md#style-invalidation)) and then
[`UpdateStyle`](https://cs.chromium.org/?q=symbol:%5Eblink::Document::UpdateStyle$)
which is what starts the traversal of the Element tree.

The tree is traversed in [shadow-including tree
oreder](https://www.w3.org/TR/shadow-dom/#concept-shadow-including-tree-order). There
are 2 recursive paths that can be taken. The simpler one is in the case where
the
[change](https://chromium.googlesource.com/chromium/src/+/lkcr/third_party/blink/renderer/core/style/stylerecalc.md)
being applied is
[`ComputedStyleConstants::kReattach`](https://cs.chromium.org/?q=symbol:%5Eblink::StyleRecalcChange::kReattach$). It
recurses through
[`ContainerNode::RecalcDescendantStylesForReattach`](https://cs.chromium.org/?q=symbol:%5Eblink::ContainerNode::RecalcDescendantStylesForReattach$)
and involves methods with names like
[`RecalcFooStyleForReattach`](https://cs.chromium.org/search/?q=symbol:%5Eblink::.*::Recalc.*Styles?ForReattach$+file:dom/). The
more complex recursion is similar. It recurses through
[`ContainerNode::RecalcDescendantStyles`](https://cs.chromium.org/?q=symbol:%5Eblink::ContainerNode::RecalcDescendantStyles$)
and involves methods with names like
[`RecalcFooStyle`](https://cs.chromium.org/search/?q=symbol:%5Eblink::.*::Recalc.*Styles?$+file:dom/)
but it can enter the reattach code also. In both cases, the actual style
calculation is performed by
[`Element::StyleForLayoutObject`](https://cs.chromium.org/?q=symbol:%5Eblink::Element::StyleForLayoutObject$).


# Omissions

*   Caching, fast reject,
    [`NthIndexCache`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/dom/nth_index_cache.h?l=36&gsn=NthIndexCache)
*   Ordering, e.g. the comment at the start of
    [`MatchScopedRules`](https://cs.chromium.org/?q=symbol:%5Eblink::StyleResolver::MatchScopedRules$)
*   How the collected set of rules that match an element are combined with
    inline style and parent style to get computed-style.
*   How the [various types of
    failure](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/css/selector_checker.h?l=153)
    are used to optimize application of `MatchSelector`
*   Other entry points that lead into rule collection.
*   Animation.
*   [`CalculateBaseComputedStyle`](https://cs.chromium.org/?q=symbol:%5Eblink::CalculateBaseComputedStyle$)
