This directory contains the Prerender2 implementation
(https://crbug.com/1126305).

If you're interested in relevant code changes, join the
prerendering-reviews@chromium.org group.

# Summary

Prerendering is "pre"-rendering, it's about pre-loading and rendering a page
before the user actually navigates to it. The main goal of prerendering is to
make the next page navigation faster, or ideally nearly instant.

The Prerender2 is the new implementation of prerendering.

# Terminology

- **Trigger**: "Trigger" is an entry point to start prerendering. Currently,
  `<script type="speculationrules">` is the only trigger.
- **Activate**: The Prerender2 runs navigation code twice: navigation for
  prerendering a page, and navigation for displaying the prerendered page.
  "Activate" indicates the latter navigation.
- **Legacy Prerender**: "Legacy Prerender" is the previous implementation of
  prerendering that does not use NoStatePrefetch. This is already deprecated
  (https://crbug.com/755921).
- **NoStatePrefetch**: An internal mechanism for speculative prefetching of
  pages and their subresources that are on a critical path of page loading
  without executing any JavaScript or creating a complex state of the web
  platform (https://www.chromestatus.com/feature/5928321099497472). This
  mechanism is not purely "no state" because the HTTP cache allows to create
  cookies and other state related to validating cache entries. The current
  prerendering uses this mechanism, that is, it does not actually render pages,
  while the Prerender2 renders pages.
- **[Activation-gated APIs](https://html.spec.whatwg.org/C/#user-activation-gated-apis)**:
  Web platform APIs that are dependent on user activation. Prerendered pages
  never have user activation, so the activation-gated APIs automatically fail or
  no-op in the prerendered pages. The known activation-gated APIs are listed
  [here](https://wicg.github.io/nav-speculation/prerendering.html#activation-gated).

# UKM Source Ids

As previously mentioned, there are two navigations involved in Prerender2: the
prerendering navigation and the activation navigation. As a result, there are
two UKM Source IDs that are currently used for a prerendered page; one derived
from each navigation's ID.

The SourceId for the prerendering navigation is currently returned by
[RenderFrameHost::GetPageUkmSourceId()](https://source.chromium.org/search?q=symbol:%5Econtent::RenderFrameHost::GetPageUkmSourceId$),
and by [NavigationHandle::GetNextPageUkmSourceId()](https://source.chromium.org/search?q=symbol:%5Econtent::NavigationHandle::GetNextPageUkmSourceId$)
when called on the `NavigationHandle` for the prerendering navigation. This
source ID is not associated with a URL by [SourceUrlRecorder](https://source.chromium.org/search?q=symbol:%5Eukm::internal::SourceUrlRecorderWebContentsObserver$) (the data
collection policy disallows recording it before prerender activation), so any
metrics recorded using it will not be associated with a URL.

The SourceId for the activation navigation is associated with a URL by
`SourceUrlRecorder`. The value is currently returned by
`NavigationHandle::GetNextPageUkmSourceId()` when called on the
`NavigationHandle` for the activation navigation, and by
[PageLoadMetricsObserverDelegate::GetPageUkmSourceId](https://source.chromium.org/search?q=symbol:page_load_metrics::PageLoadMetricsObserverDelegate::GetPageUkmSourceId$)
(and is used by all PLMOs that record UKMs for prerendered pages).

There are 2 patterns recording metrics for prerendering pages used currently in
content/:

1. Use the source ID of the most recently navigated primary page (obtained from
   `NavigationHandle::GetNextPageUkmSourceId()`, which uses the activation
   navigation ID for prerender activations). This UKM source ID is always
   associated with a URL. This approach is currently used by Precog (the UKM
   infrastructure in content/browser/preloading) to record the
   `Preloading_Prediction` and `Preloading_Attempt` UKMs.

2. Use the triggering page's UKM source ID to record metrics for a prerendering
   page. [PrerenderHost](https://source.chromium.org/search?q=symbol:content::PrerenderHost$)
   uses this approach currently to record the `PrerenderPageLoad` UKM, which it
   reports even if the prerendered page isn't activated. Precog also uses this
   approach to record the `Preloading_Attempt_PreviousPrimaryPage` UKM. They
   both currently use `RenderFrameHost::GetPageUkmSourceId` to retrieve the
   source ID, which will be associated with a URL in most cases, except when the
   triggering page itself was prerendered.

Another possible approach is to collect/remember metrics while prerendering and
only record them using the activation navigation's source ID after the prerender
is activated (a similar pattern is used by `PageLoadMetricsObserver`s outside
content/). In the future, we may want to support recording UKMs after activation
with both source IDs. This will require registering the prerender navigation
source ID with a URL after activation.

# References

The date is the publication date, not the last updated date.

- [Prerender2](https://docs.google.com/document/d/1P2VKCLpmnNm_cRAjUeE-bqLL0bslL_zKqiNeCzNom_w/edit?usp=sharing) (Oct, 2020): Introduces how Prerender2 works and more detailed designs such as Mojo Capability Control.
