# Tips for productive Chromium code reviews

This page is meant to help both CL authors and reviewers have a more productive,
efficient, and mutually beneficial code review. **None of these tips represent
formal policy**, but following this guidance should help you get your changes
reviewed and landed faster.

Please also read [Respectful Changes](cl_respect.md) and [Respectful Code
Reviews](cr_respect.md).

## Keep changes under 500 LoC

Large changes take longer to review than smaller changes. Reviewers generally
need to familiarize themselves with the content of a CL after each round of
review, so the larger a CL is, the longer that process takes. Large CLs can also
fatigue a reviewer who goes line-by-line through a CL. Try to keep changes below
500 lines of code – including tests. There is a balance here, though: 200 lines
of code (LoC) of production code with 600 LoC of tests might be fine, especially
if the test code follows a regular pattern. Conversely, 400 LoC of production
code with 200 LoC of test code may not provide enough coverage.

If your CL is larger than that, seriously consider splitting it into smaller,
reviewable units. When splitting CLs, you should tag each CL with the same
tracking bug, so that the association is clear. You can also use the [relation
chain of dependent CLs](contributing.md#uploading-dependent-changes) to allow
the reviewer to see the progression before it is landed.

## Share context for the CL

Providing context for the review is important for understanding the motivation
behind a change. The amount of context to share depends on the scale of the
change: a thorough CL description can be sufficient for a single, independent
patch. But sometimes it may be better to provide the context on a linked bug,
that e.g. documents the investigation that led to the proposed fix. If your
change is large, it is helpful to provide reviewers context for the series of
small-to-medium-sized CLs via a [design
doc](https://docs.google.com/document/d/14YBYKgk-uSfjfwpKFlp_omgUq5hwMVazy_M965s_1KA/edit#heading=h.7nki9mck5t64).
Highlight the problem that needs solving, an overall description of the proposed
solution, and any alternatives you considered.

Your CL description should always document **what** you are changing and
**why**. CL descriptions are stored in the repository history, so they should be
written to stand the test of time. Ask yourself, "if another engineer, five
years from now, needed to understand why this CL landed based on the
description, would they be able to?"

## Guide the reviewer though complex CLs

While the CL description goes on record, you can also leave comments on the CL
as well: If your CL contains one major change and a lot of fallout from that
change, you can point out where to start the review process. If you made a
design decision or trade-off that does not justify a comment in the source code,
you may still proactively leave a comment on the CL to inform the reviewer.

## Separate behavior changes from refactoring

CLs should only effect one type of change. If you need to both refactor
something and change its behavior, it is best to do so over two separate CLs.
Refactoring generally should not change behavior. This benefits the reviewer,
who can more quickly evaluate a refactoring as a move-code-only change that does
not change behavior, and the author, who potentially avoids unnecessary reverts
and re-lands due to regressions caused by the behavior change.

## Encapsulate complexity, but don’t over-abstract

One way to keep changes small is to build up composable units (functions,
classes, interfaces) that can be independently tested and reviewed. This helps
manage the overall change size, and it creates a natural progression for
reviewers to follow. However, do not over-design abstractions for an unknown
future. Allowing for extensibility when it’s not necessary, creating
abstractions where something concrete would suffice, or reaching for a design
pattern when something simpler would work equally well adds unnecessary
complexity to the codebase. The codebase is inherently mutable and additional
abstractions can be added _if and when_ they are needed.

## Optimize for reducing timezone latency

The Chromium project has contributors from around the world, and it is very
likely that you will not be in the same timezone as a reviewer. You should
expect a reviewer to be responsive, per the code review policy, but keep in mind
that there may be a significant timezone gap. Also see the advice about
[minimizing lag across
timezones](https://www.chromium.org/developers/contributing-code/minimizing-review-lag-across-time-zones/).

## Get a full review from a single, main reviewer, before asking many OWNERs

If your CL requires the approval from 3+ OWNERs, get a small number of main
reviewers (most commonly 1) to review the entire CL so that OWNERs don’t need to
deal with issues that anybody can detect. This is particularly useful if OWNERs
are in a different timezone.

## Depend on more-specific owners

Wherever possible, choose reviewers from the deepest OWNERS files adjacent to
the most significant aspects of your change. Once their review is complete, add
OWNERS from parent/less-specific directories for getting approvals for any API
change propagations. The parent-directory reviewers can typically defer to the
more-specific reviewers’ LGTM and simply stamp-approve the CL.

Avoid adding multiple reviewers from the same OWNERS file to review a single
change. This makes it unclear what the responsibilities of each reviewer are.
Only one OWNERS LGTM is needed, so you only need to select one. You can use the
file revision history to see if one reviewer has been more recently active in
the area.
