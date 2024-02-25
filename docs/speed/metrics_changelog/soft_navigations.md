# Soft Navigation Heuristics Changelog

This is a list of changes to [Soft Navigation Heuristics](https://wicg.github.io/soft-navigations/).

## Why write this doc?

The soft navigation heuristics API is intended to automatically detect and
report on interactions within a running web page which appear to the user as a
navigation, but which do not involve a new page load. This typically involves
elements on the page changing, and the page's visible URL changing, in response
to some user action. The API is currently (as of January 2024) under
development, and has been running as an [Origin
Trial](https://developer.chrome.com/docs/web-platform/origin-trials) since
Chrome 110.

Since its original release, the heuristics themselves have been changing, as we
take into account both new patterns which *should* be counted as soft
navigations, as well as removing patterns which *should not* be counted. Because
these can significantly affect the number and type of soft navigation events
recorded on any given page load, it can be useful for developers to understand
the state of the heuristics in a particular version of Chrome.

We expect this API to stabilize before it is finally launched, but in the
meantime, this document summarizes the major user-visible changes to the soft
navigation detection algorithms.

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

* Chrome 110: Initial Origin Trial release of soft navigation heuristics.
  * Soft navigation requires a click event to trigger both a URL change and an
    addition to the DOM, in any order.
  * Soft navigation entries are exposed to performance observers, as well as FP,
    FCP and LCP after each soft navigation.
  * Navigation id is included, and is a simple incrementing integer.

