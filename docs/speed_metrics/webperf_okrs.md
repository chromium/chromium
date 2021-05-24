# Web Performance Objectives

[TOC]

## 2021 Q2 Objectives

* Publish public proposal on **smoothness**.
* **Responsiveness**:
  * Socialize publicly our definition of interactions and normalization options.
  * Implement [interactionID](https://docs.google.com/presentation/d/1nxNFwsGqYy7WmIZ3uv_0HsSIQMSXQA9_PqlOD3V74Us/edit#slide=id.p).
* Enable exposing information about **Single Page Apps** via
  [navigation IDs](https://docs.google.com/presentation/d/1snPIu2GJ_JO7esJ_3YmOAiXC_g4PeQW3jclYx1u3Qzs/edit#slide=id.p).
* Expose performance information about **back-forward cache** navigations to performance timeline.
* Specify and launch redefined (window-based) **Cumulative Layout Shift**.
* Ship **JS Sampling Profiler**.
* Facilitate **Resource Timing** integration into Fetch and change implementation as needed.
* Obtain more accurate **abandonment** rates on Chrome to determine what to do about the problem.
* Implement and ship [droppedEntriesCount](https://w3c.github.io/performance-timeline/#dom-performanceobservercallbackoptions-droppedentriescount).

## 2021 Q1 Progress

* **performance.measureMemory** was shipped! See the blog [post](https://web.dev/monitor-total-page-memory-usage/).
* **Single Page Apps**:
  * Provided feedback to the AppHistory [explainer](https://github.com/WICG/app-history/blob/main/README.md).
  * Discussed how to extend performance timeline to include SPAs in the WebPerf WG.
* Improved **abandonment** data on Chrome to account for FCP more accurately.
* Continued work on **smoothness** and **responsiveness** proposals.
* Proposed way to include **back-forward cache** performance information to the WebPerf WG.
* Organized **A/B testing workshop** to better understand the problem space: see
  [notes](https://docs.google.com/document/d/1rmVjH7-5hGk_VB0EwErM1tcEVz100XZDYlaSd75WbRE/edit?ts=601c4a9c).
* Completed GC integration work for the **JS Sampling Profiler**.
* Various improvements for **Layout Instability** which especially impact carousels. See
  [changelog](https://chromium.googlesource.com/chromium/src/+/main/docs/speed/metrics_changelog/cls.md).
* Decided redefinition of **Cumulative Layout Shift** and announced in [post](https://web.dev/evolving-cls/).

## 2021 Q1 Objectives

* **performance.measureMemory**: ship the API.
* **Single Page Apps**:
  * Publish an explainer about SPA issues.
  * Determine whether User Timing hints conventions are still useful.
* **Abandonment**:
  * Gather concrete feedback form analytics providers and other potential users of this API.
  * Improve confidence on the abandonment rates computed by implementing a renderer-side flushing.
  * Update analysis on rates when the above fix has reached Chrome Stable.
* **Smoothness**:
  * Continue refining the definition of dropped frames.
  * Move proposal to WICG.
* **Back-forward cache**:
  * Expand scope to include FCP and FID in the values reported after back-forward navigations.
  * Investigate backwards compatibility of adding new entries and propose an API shaped based on the outcome.
* **Responsiveness**:
  * Investigate correctness of existing internal metrics and implement fixes as needed.
  * Further investigate scrolling performance and how it should be integrated with metric.
  * Define user interactions that we care about for this API.
  * Create a manual test corpus to test ideas about the 'end time' of a user interaction.
* **First Contentful Paint**: improve implementation to pass more
    [tests](https://wpt.fyi/results/paint-timing?label=master&label=experimental).
* **Longtasks**: add system time, including garbage collection
  ([bug](https://bugs.chromium.org/p/chromium/issues/detail?id=1091754)).
    * Present proposal to security team, and begin socializing the proposal externally.
* **A/B testing**: organize workshop on client-side A/B testing.
* **JS Sampling Profiler**:
  * Complete the GC integration work.
  * Ship the API.

## 2020 Q4 Progress

### New web performance APIs

* **performance.measureMemory**: added support for cross-origin iframes in the same process and sent
  [Intent to Ship](https://groups.google.com/a/chromium.org/g/blink-dev/c/RExJ9a3SmQw).
* **Page abandonment**: made some data available publicly and socialized it in a
  [blogpost](https://calendar.perfplanet.com/2020/abandonment/).
* **JS Sampling Profiler**:
  * Implemented the API so it requires COOP/COEP and gated it behind Document Policy.
  * Finished a prototype of GC integration for the V8 sampling profiler (which will help reduce profiler startup time).
  * Landed some initial support for code object refcounting.
* **Smoothness**: published a proposal around dropped frames and presented it at TPAC.
* **Back-forward cache**: determined that it is backwards compatible to expose a PerformanceNavigationTiming
  entry for back-forward navigations.
* **Responsiveness**:
  * Investigated some internal metrics, but found some metric quality issues that need to be investigated.
  * Started brainstorm on capturing asynchronous work as well as which user interactions to capture.
  * Did investigation on scrolling and determined that in most cases pages do not seem to suffer from poor
    scrolling performance.

### Existing web performance API improvements

* **Largest Contentful Paint**: include removed nodes ([bug](https://bugs.chromium.org/p/chromium/issues/detail?id=1045640))
  and ignored images occupying the full viewport ([bug](https://bugs.chromium.org/p/chromium/issues/detail?id=1133883)).
* **Cumulative Layout Shift**: implemented various fixes, see
  [changelog](https://chromium.googlesource.com/chromium/src/+/main/docs/speed/metrics_changelog/README.md).

## 2020 Q4 Objectives

### New web performance APIs

  * {#measure-memory-20204}**performance.measureMemory**:
    * Add support for cross-origin iframes.
    * Send Intent to Ship and ship --- API would become available early next year.
  * {#spas-20204}**Single Page Apps**:
    * Publish document for feedback on measurement issues, attributions issues, and other issues specific
      to SPAs.
    * Land support for User Timing hints in Chrome, and get 2+ frameworks to start using such hints.
  * {#page-abandonment-20204}**Page abandonment**: publish data on abandonment rates, making a case for or against
    an abandonment API.
  * {#js-profiler-20204}**JS Sampling Profiler**:
    * Implement the API so it requires COOP/COEP.
    * Add support for warm codemap initialization.
    * Add web platform tests.
    * _(Stretch)_ Send Intent to Ship.
  * {#smoothness-20204}**Smoothness** (FrameTiming):
    * Discuss and socialize API shape.
    * Propose API on WICG.
    * Start a TAG review.
  * {#bf-cache-20204}**Back-forward cache**: document and socialize a concrete proposal on a web API that supports
    monitoring performance of sites on browsers that may perform back-forward navigations.
  * {#responsiveness-20204}**Responsiveness**:
    * Investigate internal metrics and potentially add new metrics to capture end-to-end responsiveness.
    * Document how popular frameworks handle user interactions.
    * Brainstorm on how to expand Event Timing to capture user handling for asynchronous work and to handle multiple
      events referring to a single user interaction.
    * Complete on-going investigation on whether scroll performance is also a problem in the web that needs a web API.

### Existing web performance API improvements

  * {#lcp-20204}**Largest Contentful Paint**:
    * Complete [investigation](https://bugs.chromium.org/p/chromium/issues/detail?id=1045640) on removed nodes
      and if needed update the API.
    * [Ignore](https://bugs.chromium.org/p/chromium/issues/detail?id=1133883) images that occupy the full viewport.
  * {#cls-20204}**Cumulative Layout Shift**: evaluate the impact of triggering on empty or invisible content and update
    [spec](https://github.com/WICG/layout-instability/issues/61) and implementation accordingly.
  * {#fcp-20204}**First Contentful Paint**: improve implementation to pass more
    [tests](https://wpt.fyi/results/paint-timing?label=master&label=experimental).
  * {#rt-worker-20204}**Navigation Timing**: [Fix](https://crbug.com/925239) encoded/decoded body sizes when going
    through service workers.

## 2020 Q3 Progress

### New web performance APIs

  * [performance.measureMemory](#measure-memory-20203):
    * Spec was reviewed and polished.
    * API support was added for workers (available from Chrome 87).
  * [Page abandonment](#page-abandonment-20203): improved data was gathered, but more accuracy improvements are needed.
  * [VisibilityStateEntry](#page-visibility-20203):
    [explainer](https://docs.google.com/document/d/1l5kHiJRkdQwEN-CYI5_mUNODhQVB5rCyjN4jHDdXDHA/edit#),
    [discussion](https://github.com/w3c/performance-timeline/issues/105), and TAG
    [review](https://github.com/w3ctag/design-reviews/issues/534) kicked off, but no consensus yet on API shape.
  * [FrameTiming](#frame-timing-20203): a lot of research on defining a good metric to capture smoothness.
  * [isInputPending](#fb-driven-20203): shipped and available from Chrome 87!

### Existing web performance API improvements

  * [LargestContentfulPaint](#lcp-20203):
    * Did analysis on how LCP would change when removed content is included.
    * Ignored paints occurring with opacity 0.
  * CumulativeLayoutShift [fixes](https://chromium.googlesource.com/chromium/src/+/main/docs/speed/metrics_changelog/cls.md):
    * Ignored shifts from video thumb sliders.
    * Fixed computations for ink overflow and transforms.
    * Updated computations when child moves alongside their parent element.

### Interop

  * [Web vitals specs](#vitals-specs-20203): triaged new
    [issues](https://github.com/search?q=is%3Aissue+created%3A2020-06-01..2020-09-30+repo%3Awicg%2Flayout-instability+repo%3Awicg%2Flargest-contentful-paint+repo%3Awicg%2Fevent-timing&type=issues)
    as well as existing ones.
  * [Paint Timing](#paint-timing-20203): fixed two FCP tests by improving our implementation.

## 2020 Q3 Objectives

### New web performance APIs

  * {#measure-memory-20203}Continue [previous work](#measure-memory-20202) on **performance.measureMemory**.
    * Extend the scope of the API to workers and cross-site iframes.
    * Do a review over the spec and polish it.
  * {#spas-20203}Continue [previous work](#spas-20202) on **Single Page Apps** (SPAs).
    * Complete partnerships with frameworks and begin gathering data using the strategy based on User Timing annotations.
  * {#page-abandonment-20203}Continue [previous work](#page-abandonment-20202) on **page load abandonment**.
    * Decide a path forward for a potential new web API based on data from the metric we recently implemented.
  * {#page-visibility-20203}Continue [previous work](#page-visibility-20202) on exposing full
    **[Page Visibility](https://github.com/w3c/page-visibility/)** history. We intend to spec and ship the
    [VisibilityStateEntry](https://docs.google.com/document/d/1l5kHiJRkdQwEN-CYI5_mUNODhQVB5rCyjN4jHDdXDHA/edit).
  * {#frame-timing-20203}Work on the
    **[Frame Timing](https://docs.google.com/document/d/1t3A56iTN01ReEELJ18_jLYrvc13L3hDTXWK46AafKwE/edit)** proposal.
    * Draft a spec.
    * _(Stretch)_ Land a prototype of the API.
  * {#fb-driven-20203}Continue [previous work](#fb-driven-20202) on Facebook-driven APIs.
    * Based on the iframe layerization experiment results, send Intent to Ship for **isInputPending**.
    * _(Stretch)_ Implement warm initialization logic for the **JS Self-Profiling** API.

### Existing web performance API improvements

  * {#lcp-20203}Improve quality of **[Largest Contentful Paint](https://github.com/WICG/largest-contentful-paint)**.
    * Decide whether LCP including removals is better than the current definition (which excludes removals).
    * Address problems with LCP not correctly accounting for opacity in some cases due to optimizations in paint code: see
      relevant [bug](https://bugs.chromium.org/p/chromium/issues/detail?id=1092473).
  * {#normalization-20203}Brainstorm ideas around better **normalization** techniques for existing web performance metrics and
    socialize these ideas with the W3C Web Perf WG.

### Interop

  * {#vitals-specs-20203}Triage spec issues filed on GitHub. We intend to give higher priority to feeedback on
    [web vitals](https://web.dev/vitals/) specs.
  * {#event-timing-20203}Reduce WPT test flakiness on **[Event Timing](https://github.com/WICG/event-timing)**.
  * {#paint-timing-20203}Change more of Chrome's **[Paint Timing](https://github.com/w3c/paint-timing)** implementation to pass more
    of the currently failing WPTs.

## 2020 Q2 Progress

* [performance.measureMemory](#measure-memory-20202):
  * Crafted a plan to enable extending the API to workers as well as to cross-site iframes with the appropriate COEP headers.
  * Drafted a [spec](https://ulan.github.io/misc/measure-memory-spec/), but it requires review.
* [Single Page Apps](#spas-20202):
  * Designed a strategy for routing frameworks to report the start and end of SPAs via User Timing.
  * Began conversations with such frameworks but haven't started gathering data with the strategy we designed.
  * Presented our data-driven approach to the W3C Web Perf WG.
* [Page abandonment](#page-abandonment-20202): Socialized a proposal for how to define abandonment and brainstormed ways in which
  we could expose the data to web developers. However, we have not yet reached a decision on how we plan to expose the data because
  there is no one actively working on solving design issues with [Origin Policy](https://wicg.github.io/origin-policy/).
* [Event Timing](#event-timing-20202): Shipped the API, which should be available on Chrome 85 (see chromestatus
  [entry](https://www.chromestatus.com/feature/5167290693713920)).
* [Page Visibility](#page-visibility-20202): Socialized our proposal and based on feedback will work on shipping
  VisibilityStateEntry (see the [explainer](https://docs.google.com/document/d/1l5kHiJRkdQwEN-CYI5_mUNODhQVB5rCyjN4jHDdXDHA/edit)).
* [Layout Instability sources](#cls-sources-20202): Shipped the `sources` attribute, which should be available on Chrome 84 (see
  chromestatus [entry](https://www.chromestatus.com/feature/5712483207610368)).
* Fixed layout shifts being reported from controls in the video element, see relevant [bug](https://crbug.com/1088311). The fix is
  available on Chrome 85.
* [Largest Contentful Paint removal](#lcp-removal-20202): Added a metric that computes an experimental version of LCP which
  includes content that is removed from the page. We're waiting to get Stable data in order to decide whether this is an
  improvement over the existing LCP metric.
* [Final Largest Contentful Paint](#final-lcp-20202): Wrote a
  [proposal](https://docs.google.com/document/d/1a24lxTmSycox5HS1mdtj09Sinf1mJCifdv-oREUJQ1A/edit) on how to surface the final LCP
  candidate to a page, but this work has been deprioritized for the time being.
* [Paint Timing](#paint-timing-20202): Implemented a couple of fixes and now pass two more tests, but more work is needed to pass
  all of them.
* [Documentation](#documentation-20202): Submitted feedback on some of the metrics content on web.dev, most of which was actioned
  upon. Updated MDN entries for core web vitals metrics.
* [Facebook-driven APIs](#fb-driven-20202): Landed support for predicting touch and gesture events for isInputPending.

## 2020 Q2 Objectives

### New web performance APIs

  * {#measure-memory-20202}Work towards shipping
    **[performance.measureMemory](https://github.com/WICG/performance-measure-memory)**.
    This API intends to provide memory measurements for web pages without
    leaking information. It will replace the non-standard performance.memory and
    provide more accurate information, but will require the website to be
    [cross-origin
    isolated](https://developer.mozilla.org/en-US/docs/Web/API/WindowOrWorkerGlobalScope/crossOriginIsolated).
    Try it out with the Origin Trial
    [here](https://web.dev/monitor-total-page-memory-usage/#using-performance.measurememory())!
    Deliverables for this quarter:
    * Extend the scope of the API to workers.
    * Draft a spec.

  * {#spas-20202}Work towards web perf support for **Single Page Apps** (SPAs). SPAs have
    long been mistreated by our web performance APIs, which mostly focus on the
    initial page load for ‘multi-page apps’. It will be a long process to
    resolve all measurement gaps, but we intend to start making progress on
    better performance measurements for SPAs by using a data-driven approach.
    Deliverables for this quarter:
    * Implement a strategy for measuring the performance of SPA navigations in
      RUM, based on explicit navigation annotations via User Timing.
    * Partner with some frameworks to gather data using said strategy.
    * Socialize an explainer with our ideas.

  * {#page-abandonment-20202}Work towards web perf support for **page abandonment**. Currently, our APIs
    are blind to a class of users that decide to leave the website very early
    on, before the performance measurement framework of the website is set into
    place. This quarter, we plan to create and socialize a proposal about
    measuring early page abandonment.

  * {#event-timing-20202}Ship the full **[Event Timing](https://github.com/WICG/event-timing)** API.
    Currently, Chrome ships only ‘first-input’ to enable users to measure their
    [First Input Delay](https://web.dev/fid/). We intend to ship support for
    ‘event’ so that developers can track all slow events. Each entry will
    include a ‘target’ attribute to know which was the EventTarget. We’ll
    support a durationThreshold parameter in the observer to tweak the duration
    of events being observed. Finally, we’ll also have performance.eventCounts
    to enable computing estimated percentiles based on the data received.

  * {#page-visibility-20202}Ship a **[Page Visibility](https://github.com/w3c/page-visibility/)**
    observer. Right now, the Page Visibility API allows registering an event
    listener for future changes in visibility, but any visibility states prior
    to that are missed. The solution to this is having an observer which enables
    ‘buffered’ entries, so a full history of the visibility states of the page
    is available. An alternative considered was having a boolean flag in the
    PerformanceEntry stating that the page was backgrounded before the entry was
    created, but there was overwhelming
    [support](https://lists.w3.org/Archives/Public/public-web-perf/2020Apr/0005.html)
    for the observer instead.

  * {#fb-driven-20202}Provide support for two Facebook-driven APIs:
    [isInputPending](https://github.com/WICG/is-input-pending) and [JavaScript
    Self-Profiling](https://github.com/WICG/js-self-profiling). The
    **isInputPending** API enables developers to query whether the browser has
    received but not yet processed certain kinds of user inputs. This way, work
    can be scheduled on longer tasks while still enabling the task to stopped
    when higher priority work arises. The **JS Self-Profiling** API enables
    developers to collect JS profiles from real users, given a sampling rate and
    capacity. It enables measuring the performance impact of specific JS
    functions and finding hotspots in JS code.

### Existing web performance API improvements

* {#cls-sources-20202}Ship the
  [sources](https://github.com/WICG/layout-instability#Source-Attribution)
  attribute for the
  **[LayoutInstability](https://github.com/WICG/layout-instability)** API. The
  Layout Instability API provides excellent information about content shifting
  on a website. This API is already shipped in Chrome. However, it’s often hard
  to figure out which content is shifting. This new attribute will inform
  developers about the shifting elements and their locations within the
  viewport.

* {#lcp-removal-20202}**[LargestContentfulPaint](https://github.com/WICG/largest-contentful-paint)**:
  gather data about LCP without excluding DOM nodes that were removed. The
  Largest Contentful Paint API exposes the largest image or text that is painted
  in the page. Currently, content removed from the website is also removed as a
  candidate for LCP. However, this negatively affects some websites, for
  instance those with certain types of image carousels. This quarter, we’ll
  gather data internally to determine whether we should start including removed
  DOM content. The API itself will not change for now.

* {#final-lcp-20202}_(Stretch)_ Work on exposing the **‘final’ LargestContentfulPaint** candidate.
  Currently LCP just emits a new entry whenever a new candidate is found. This
  means that a developer has no way to know when LCP is ‘done’, which can happen
  early on if there is some relevant user input in the page. We could consider
  surfacing an entry to indicate that LCP computations are finished and
  including the final LCP value, when possible. There’s also an
  [idea](https://github.com/WICG/largest-contentful-paint/issues/43#issuecomment-608569132)
  to include some heuristics to get a higher quality signal regarding whether
  the LCP obtained seems ‘valid’. If we have time this quarter, we’d be happy to
  do some exploration on this.

* _(Stretch)_ **[ResourceTiming](https://github.com/w3c/resource-timing)**:
  outline a plan to fix the problem of TAO (Timing-Allow-Origin) being an opt-in
  for non-timing information such as transferSize. This may mean using a new
  header or relying on some of the upcoming new security primitives in the web.
  If we have time this quarter, we’d like to begin tackling this problem by
  socializing a concrete proposal for a fix.

### Interop and documentation

* {#paint-timing-20202}**[Paint Timing](https://github.com/w3c/paint-timing)**: change the Chromium
  implementation so it passes [new web platform
  tests](https://wpt.fyi/results/paint-timing/fcp-only?label=experimental&label=master&aligned).
  These tests are based on the feedback from WebKit. They intend to ship First
  Contentful Paint in the near future!

* {#documentation-20202}Improve the **documentation** of our APIs on MDN and web.dev. We have been
  busily shipping new web perf APIs, but some of the documentation of them has
  lagged behind. For instance, we’ll make sure that there’s MDN pages on all of
  the new APIs we’ve shipped, and we’ll collaborate with DevRel to ensure that
  the documentation on web.dev is accurate.
