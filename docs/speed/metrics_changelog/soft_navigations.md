# Soft Navigation Heuristics Changelog

This is a list of changes to [Soft Navigation Heuristics](https://developer.chrome.com/docs/web-platform/soft-navigations-experiment).

* Chrome 148
  * Metric bug fix: Use correct `NavigationId` for `InteractionContentfulPaint` entries ([59bc92edc430c](https://chromium-review.googlesource.com/c/chromium/src/+/7704133)).

* Chrome 147 (Major changes after origin trial expired & preparing to extend OT for 3 milestones)
  * **`InteractionContentfulPaint` decoupled from `SoftNavigationEntry` and reported for all interactions**.
    * `InteractionContentfulPaint` entries now emit as soon as they occur, for any interaction, rather than waiting for a Soft Navigation to become detected first ([e8815d0acaf2f](https://chromium-review.googlesource.com/c/chromium/src/+/7512642)).
    * The same semantics used to measure soft-navigation loading performance can be used to measure any dynamic content updates, even when the URL doesn't change.
  * **`SoftNavigationEntry` adds `largestInteractionContentfulPaint` attribute**.
    * `SoftNavigationEntry` entries now report a `largestInteractionContentfulPaint` attribute.  Because `InteractionContentfulPaint` entries are now decoupled from soft-navigations, this makes it more convenient to track soft-LCP for soft navs, specifically.
  * **Aligned "Interaction Detection" with INP**.
    * Changed the implementation to reuse the existing Event Timing API mechanism for detecting interactions.
    * To accomplish this, added support for measuring Event Timing for `navigate`, `popstate`, and `hashchange` events ([c6a49e314dfb5](https://chromium-review.googlesource.com/c/chromium/src/+/7598334)), however, these Event Timing entry types are not yet exposed to the performance timeline.
    * Added `interactionId` attribute to `InteractionContentfulPaint` and `SoftNavigationEntry` performance entries ([704c0222f53a3](https://chromium-review.googlesource.com/c/chromium/src/+/7638455)).
  * **Added support for `replaceState`**.
    * Added support for triggering soft navigations on `replaceState` (in addition to `pushState`).
    * Exposed the `navigationType` ([279eb27b6d2bb](https://chromium-review.googlesource.com/c/chromium/src/+/7530377)).
  * **Refined TimeOrigin (to measure more time)**.
    * Previously, TimeOrigin was a complicated value, something akin to `min(processingEnd, urlChangeTime)` and was meant to match hard-navs cross-document navigationStart, but this model proved confusing and undesired.
    * Changed `InteractionContentfulPaint` and `SoftNavigationEntry` to report `startTime` equal to Interaction startTime ([12b7e08417957](https://chromium-review.googlesource.com/c/chromium/src/+/7563413)).
  * Other implementation changes:
    * Set up paint tracking for inline style and class attribute changes ([50dc1d52a77ca](https://chromium-review.googlesource.com/c/chromium/src/+/7512446)).
    * Improved browser-side soft navigations tracking ([a431cb877c128](https://chromium-review.googlesource.com/c/chromium/src/+/7640511)).

* Chrome 140 - 144 (Addressing bugs during origin trial)
  * Metric bug fix: Refactor pending `InteractionContentfulPaint` entry buffering ([abe0507666c40](https://crrev.com/abe0507666c40)).
  * Metric bug fix: Ensured correct navigation ID is used for entries from `SoftNavigationContext` ([5bdc6551839e7](https://crrev.com/5bdc6551839e7)).
  * Metric bug fix: Improve recording soft navigations in UKM (correct URL) ([5c510d4ea6310](https://crrev.com/5c510d4ea6310)).
  * Metric bug fix: Fixed browser-initiated intercepted back navigations ([b0c3c8bac14d0](https://crrev.com/b0c3c8bac14d0)).

* Chrome 139 (Major revamp completed & new origin trial started)
  * **Major revamp of Soft Navigation Heuristics**.
    * This release included a large overhaul of the detection mechanics to improve accuracy and performance.
    * This work culminated in Chrome 139, and we started a new 6 milestone origin trial: https://developer.chrome.com/blog/new-soft-navigations-origin-trial.
  * **Each Interaction now has independent painted area tracking**.
    * Moved painted area tracking into each Context ([a2a9986393624](https://crrev.com/a2a9986393624)) and removed the global "Initial area" requirement ([7321c9f17fd69](https://crrev.com/7321c9f17fd69)).
    * Explicitly supported dynamic content updates (in `PaintTimingDetectors`), rather than the need to "reset LCP" ([e6d3c7c1267d7](https://crrev.com/e6d3c7c1267d7)).
  * **Introduced `InteractionContentfulPaint` PerformanceEntry**.
    * Added the `InteractionContentfulPaint` entry to observe paint timings directly tied to interactions ([88847f08dc064](https://crrev.com/88847f08dc064)).  This replaces "soft" LCP entries and removes the need for `includeSoftNavigationObservations` flag.
    * Each interaction will stop soft-LCP attribution at the first discrete input or scroll ([cbb9d4e7c5eb3](https://crrev.com/cbb9d4e7c5eb3)).
  * **Integrated `PaintTimingMixin` into `SoftNavigationEntry` for FCP**.
    * See ([7ef307610e5e9](https://chromium-review.googlesource.com/c/chromium/src/+/6657746)).
  * **Expanded DOM modification triggers**.
    * Add `<video>` and image `src` changes as DOM modifications ([82614425d92f0](https://chromium-review.googlesource.com/c/chromium/src/+/6788270), [11dd080af48a9](https://chromium-review.googlesource.com/c/chromium/src/+/6599236)).
  * Other implementation changes:
    * Introduced advanced paint attribution modes (Pre-Paint based and lazy bottom-up tree walk) ([7dacb1ba603ea](https://chromium-review.googlesource.com/c/chromium/src/+/6629428), [9f42b77c0cb66](https://crrev.com/9f42b77c0cb66)).
    * Changed `NavigationID` to report a number instead of a UUID ([c701061e45c73](https://chromium-review.googlesource.com/c/chromium/src/+/6604387)).
    * Replaced `NodeFlags::kModifiedBySoftNavigation` bit (which would persist forever) with per-context maps ([4a275bc93d519](https://crrev.com/4a275bc93d519)).
    * Improved support for multiple URL changes within a context ([eb71cbb21b410](https://crrev.com/eb71cbb21b410)).
    * Optimized `ModifiedDOM` calls and context validation checks ([fe01d34922570](https://crrev.com/fe01d34922570), [1e8e9dc69d467](https://crrev.com/1e8e9dc69d467)).

* Chrome 122
  * The 2% paint threshold now must be reached by each new soft navigation.
  * The start time of the soft navigation entry now marks the processing end
    time of the user interaction which triggered it.
  * Elements painted after an interaction, but before a soft navigation is
    detected will only count as LCP candidates if their paints are attributable
    to the interaction.

* Chrome 121
  * The paint threshold is reduced from 20% to 2%.

* Chrome 120
  * The first soft navigation on a page now also requires, in addition to the
    URL and DOM change, that the user interaction triggers a threshold amount of
    painting on the page.
  * This threshold is set to 20% of the amount which was painted before the
    page's very first user interaction. (Subsequent navigations do not require
    this, due to a bug)

* Chrome 119
  * Keyboard events can now trigger soft navigations.

* Chrome 117
  * FP and FCP entries are no longer emitted after soft navigation (without a
    command-line flag).

* Chrome 115
  * Navigation ID is changed to a random UUID in performance entries.

* Chrome 112
  * Performance observers need to be created with a flag to see FP, FCP and LCP
    events after soft navigation.

* Chrome 110: Initial origin trial release of soft navigation heuristics.
  * Soft navigation requires a click event to trigger both a URL change and an
    addition to the DOM, in any order.
  * Soft navigation entries are exposed to performance observers, as well as FP,
    FCP and LCP after each soft navigation.
  * Navigation id is included, and is a simple incrementing integer.
