# BFCache Overview

## TL;DR

Without back/forward-cache (BFCache),
when the browser navigates away from a page,
it discards the DOM and all of the JS context.
If it returns to that page,
even if every resource is served from the HTTP cache,
it has to parse everything from scratch
and execute the JS in a fresh JS context.
With BFCache,
when the browser navigates away from a page,
it might just pause that page,
keeping everything fully intact.
If it returns to that page,
it can simply unpause it,
giving the user a much fast user-experience.

## Contact

Please reach out to [bfcache-dev@chromium.org](mailto:bfcache-dev@chromium.org) for advice.

## Your feature and BFCache

Ignoring BFCache can result in serious privacy problems.
The performance benefit provided by BFCache are enormous.
As a result,
it is not acceptable to launch a feature
and figure out BFCache later.
TAG, privacy and API owners
are all be on the look-out for APIs
that have not properly considered BFCache.

There are many pre-existing features that are not compatible with BFCache.
Some are fundamentally incompatible given their current API/UX.
Some are potentially compatible
but predate BFCache in Chrome.
We have ensured that BFCache is disabled
when these features are active.
This avoids privacy problems
at the expense of performance.
We do not want to add to this list.

### Specs

If your feature is part of the web platform,
you should ensure that BFCache
(and other non-fully-active states like prerendering)
are accounted for in the spec.

The two main docs you need to read are
- https://www.w3.org/TR/design-principles/#support-non-fully-active
- https://w3ctag.github.io/bfcache-guide/

These are essential reading for all new WP features.
The privacy and security questionnaire for TAG review
contains a section on non-fully-active behaviour.
Chrome's privacy team also requires
specific consideration of BFCache
(and other non-fully-active documents)
during privacy reviews.

### Implementation

Even if your feature is only a part of the browser,
you may still need to worry about BFCache.
If your feature interacts with pages
or reacts to page events or navigation
then it may need logic to cope with the fact that
pages are not necessarily destroyed
when navigation occurs
(an assumption that was correct for decades).

[This doc](https://docs.google.com/document/d/1kR9QHWXXpoXsP3Y6JEDpIgZ5p7EgTTvdDN2kIrYdXcg/edit?tab=t.0#heading=h.b5k2sf3jhu99)
provides advice on how to make a feature behave nicely with BFCache.
Please also [contact us](mailto:bfcache-dev@chromium.org)
for explainer, design or code reviews.
