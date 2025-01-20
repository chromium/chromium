# C++ Style Changes

*The following policy was approved by Chrome ATLs.*

## Purpose

Style decisions should lead to a better product: a safer codebase, more
productive engineers, fewer bugs. Decisions should promote these outcomes, not
pursue merely theoretical benefit. Decisions or policies (including this one)
found to be detrimental to them can and will be revisited.

## Scope

C++ style decisions include anything in the
[Chromium C++ style guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++.md)
or
[C++ allowed features doc](https://chromium.googlesource.com/chromium/src/+/HEAD/styleguide/c++/c++-features.md),
as well as meaningful changes to the
[Google C++ style guide](https://google.github.io/styleguide/cppguide.html) to
the degree cxx@ becomes aware of them. Not included are minor shifts in
clang-format behavior from roll to roll (e.g. due to bugfixes). In practice, the
majority of the decisions here are around allowing or banning newer C++
features, or dealing with significant stylistic shifts like allowing mutable
reference parameters.

Changes with large functional impact (e.g. banning pointer arithmetic) are not
just style questions but have technical concerns and migration plans that must
be discussed elsewhere, e.g. with ATLs. This policy doesn't grant cxx@ (or any
other group) the right to impose unbounded technical cost.

## Authority

C++ style decisions for Chromium are made by rough consensus of the
[cxx@](mailto:cxx@chromium.org) mailing list. Unanimity is not necessary, but if
clear consensus is not reachable, decisions should be escalated to the
[Chrome ATLs](mailto:chrome-atls@google.com).

cxx@ is a moderated list, but only for the purpose of avoiding spam; all
Chromium contributors are welcome to join and opine regardless of perceived C++
expertise. Discussions take place here, not on the general chromium-dev@ list,
to limit visibility to those who express interest.

List members are encouraged to opine (even with simple "I agree" comments) to
make consensus (or lack thereof) clear. If a proposal has few objections but
also few assents, its value looks questionable. If possible, strive for at least
three "LGTM"-type comments on proposals (in addition to waiting for sufficient
time for objections to surface) before declaring an issue to have achieved
consensus.

If possible, any postmortem that includes fallout from a style change should
include contributions from the folks who proposed/approved/authored that change.

Since cxx@ members are fellow developers, be gracious around violations of the
policies here or elsewhere; in most cases, the right first step to addressing
them is to raise a thread on cxx@ describing the problem. The
[community@](mailto:community@chromium.org) mailing list and ATLs are good
alternate contacts or escalation points for personal or technical issues,
respectively.

## Relationship to Google Style

As the
[Chromium C++ style guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++.md)
states, Chromium style is based on Google style, so in the absence of explicit
guidance otherwise, Google style rules apply.

However, such exceptions do exist, and cxx@ may change and add to them. Common
justifications to diverge from Google's (internally-focused) guidance include:
* Different build environment (greater emphasis on compile time costs)
* Different running environment (different types of security concerns)
* Open source, with many external contributors (tech islands have a higher
penalty)
* Fewer automated refactoring tools (automated refactorability is less valuable)
* Smaller, more focused codebase (can ignore some types of theoretical use or
misuse)

At the same time, be mindful that:
* Exceptions have a cognitive cost, and like all style rules, should
["pull their weight"](https://google.github.io/styleguide/cppguide.html#Goals)
* Google style arbiters are experienced and draw from a large codebase, so their
decisions should not be discarded lightly

## Enforcement

Where feasible, style rules should be checked (better yet, automatically
implemented) via tooling. For example, we should strive to implement PRESUBMIT
checks (or similar) to prevent use of banned features. Changes that extend or
improve such automation, or tell authors how to comply in a more convenient way
(e.g. instructions to integrate clang-format with popular editors) are welcome.

Changes should not knowingly violate the style guide (even with local OWNER
approval) without at least raising a cxx@ thread about the issue (if for no
other reason than so that list can bless the use and consider changing the style
rules to formally allow it). This includes changes that do not ensure their
modifications are clang-format clean. If author and reviewers agree that an
exception is warranted while cxx@ discussion is ongoing, such changes may land
without blocking on the outcome, with the caveat that cxx@ may later require
them to be rolled back or reworked.

Subtree OWNERS may impose additional local style requirements (but not
exceptions to the global rules) if they do not impose surprising burdens on
contributions. Such requirements should be clearly documented and (ideally)
enforced via tooling of their own.

## Consistency

When style rules change, consider whether to update existing code. The goal of
such updates should be to maximize clarity and velocity. Use the following principles:
* Do the right thing going forward: assume the codebase will be long-lived, so
the total value of small but ongoing benefits is large
* Reduce discussion: updating is more valuable if it is likely to prevent lots
of future discussion and debate on code reviews

Lower priorities are worth considering, and it's reasonable to delay for short
times to achieve them, but should not indefinitely block higher ones.

Given Chromium's current tooling, hindering blame trawling is a significant
concern. However, the challenges here are not specific to style issues, so while
important, they should be thoughtfully considered, not treated as hard blockers.
A continual trickle of unrelated changes over years may be more disruptive on
net than a single LSC, and certainly cannot be skipped via
`.git-blame-ignore-revs`.

Subtree OWNERS are welcome (indeed, encouraged) to update code to comply with
(and enforce) new style rules more rapidly than is happening for the codebase as
a whole.

## LSCs

Large-scale changes to update code for style changes may use the
[LSC process](https://chromium.googlesource.com/chromium/src/+/main/docs/process/lsc/large_scale_changes.md)
if it's beneficial, but are not required to. The process is designed to vet
proposals for merit (which cxx@ consensus can judge in these cases) and bestow
OO+1 powers to global approvers where needed (which many cxx@ members already
have). Thus in most cases this would just be extra overhead.

If possible, style-change LSCs should aim to fully land in a short time window
(a few days or less) so committers can fix merge failures all at once. Any LSCs
that need to close the tree should be landed over weekends or holidays.

## Communications

Mailing cxx@ with a proposal is sufficient to request a change to either Chrome
style or the
[C++ allowed features list](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++-features.md).
Most proposals are discussed fairly actively, and reach consensus fairly
quickly. However, if there aren't many replies or discussion stalls without
unaddressed objections, feel free to request an explicit decision, and provide a
reasonable timeframe (generally one calendar week during non-holiday periods)
after which the request can be considered tacitly approved.

At least quarterly, if there are any changes to style rules (including allowed
C++ features), someone from cxx@ should summarize after-the-fact in an email to
chromium-dev@.

Pre-announcing changes or LSCs should be rare. chromium-dev@ has 83000+ members;
polling or even notifying it is usually more cost than benefit. For
exceptionally disruptive changes that deserve notification (exact definition up
to cxx@ discretion), announce at least two weeks in advance, linking to an
explainer doc with a scheduled date and a contact point for concerns and
escalations.
