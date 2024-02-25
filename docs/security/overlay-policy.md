# No Cross-Origin Full-Page Overlays

Chrome features should not include cross-origin overlays that fully obscure an
active web page's content area, even temporarily.

If the active page is partially obscured, we should provide sufficient cues to
the user that the active page is still present and active. Partial overlays can
be dangerous or confusing as well and should be designed carefully with input
from security and Chrome UX.

## Why?

Chrome has had multiple features in the past that attempt to show a full page,
cross-origin overlay above an active web page.  In practice, these have created
dangerous situations with a large variety of bugs and security issues that are
difficult to address in general, so we have adopted a security UX policy that
disallows full page overlays.

Features that have used or proposed full page overlays:
* **Interstitial Pages**: Security interstitial pages (e.g., for TLS or Safe
    Browsing warnings, as listed at `chrome://interstitials`) used an overlay
    approach for many years, with [dozens of bugs](https://crbug.com/392354)
    due to the active page underneath. This feature eventually migrated to use
    committed error pages instead in [448486](https://crbug.com/448486), as
    documented
    [here](https://chromium.googlesource.com/chromium/src/+/main/docs/navigation_concepts.md#Interstitial-Pages).
  * Example: [392354](https://crbug.com/392354): Interstitials are weird and do
    weird things. This lists 68 dependent bugs (!), spanning a wide variety of
    affected features.
* **Prerendering v1**: The original version of prerendering loaded the next page
    in a hidden active state, and it had many challenges with detecting any
    observable behavior and discarding the page. This created
    [complexity](https://docs.google.com/presentation/d/1Ag_LJW_OoIZI--YcZIvv6JicGjECTmrM5xKk8zd_wZg/edit#slide=id.g146ba3c676_0_2),
    [maintenance challenges](https://docs.google.com/document/d/16VCYGGWau483IMSxODpg5faZny1FJ6vNK2v-BuM5EhU/edit#heading=h.tp04wz80qf7b),
    and posed "a burden requiring constant investment of 1-2 SWEs." The feature
    was eventually removed and replaced by Prerendering v2, which uses
    restricted Mojo interfaces to limit what can occur before it is displayed.
  * Example: [439545](https://crbug.com/439545) - Desktop notifications
  * Example: [520275](https://crbug.com/520275) - Voice synthesis audio
  * Example: [247848](https://crbug.com/247848) - Infobars
* **Google Lens screenshots**: Google Lens showed a screenshot overlay above an
    active page, and encountered security bugs where the page changed
    underneath the screenshot. This feature changed to semi-transparent masks.
  * Example: [1242424](https://crbug.com/1242424): Lens Screenshot URL spoof. A
    Google Lens screenshot overlay above the active page was not dismissed for
    all types of navigations, creating a mismatch between the displayed
    content's origin and the URL in the omnibox. Due to a full-page overlay
    concern in comment 4, Lens was changed in
    [1247917](https://crbug.com/1247917) to use a semitransparent mask to show
    underlying page content.
* **Cross-Origin Paint Holding**: Chrome shows the last paint of the previously
    committed page after a new page commits for up to 4 seconds, until the new
    page paints. This reduces the amount of time the user is looking at a blank
    page, but also creates a discrepancy between the address bar and the
    content shown in the tab. The stale content is not interactive, but it has
    been used in many URL spoof reports for which we haven't had a great
    response. It is also possible for the new active page to show prompts or
    other disruptive behavior before painting, while the stale paint is still
    visible (see [1447077](https://crbug.com/1447077)). This feature is still
    enabled, with attempts to mitigate the issues (e.g., disabling input). We
    may choose whether to continue to mitigate these issues or find a safer way
    to approach this feature.
  * Discussion of concerns: [721145](https://crbug.com/721145)
  * URL spoofs requiring fixes to the 4 second timer:
    [497588](https://crbug.com/497588), [672847](https://crbug.com/672847),
    [739621](https://crbug.com/739621), [844881](https://crbug.com/844881),
    [1152894](https://crbug.com/1152894)
  * URL spoof reports we choose not to fix: [776402](https://crbug.com/776402)
    (comment 14 discusses our desire to find a safer UX),
    [755058](https://crbug.com/755058), [784395](https://crbug.com/784395),
    [1218366](https://crbug.com/1218366), [1237742](https://crbug.com/1237742),
    [1304041](https://crbug.com/1304041)
    * In several WontFix cases, the bug was exploitable for a year or more
      before the repro independently stopped working, but we had no means of
      addressing it, and the underlying issues remain present even if
      particular repro steps stop working.  In other cases, we had to close
      issues explaining that we can't do any better due to paint holding, which
      can undermine our credibility with vulnerability reporters.
* **Default Navigation Transitions**: Proposed to show a full-page screenshot of
    a cross-origin page above an active page while a navigation is in progress.
    Pursuing alternative approaches on most Chrome platforms, though a full
    screen overlay is currently in use on Chrome for iOS.

## Examples of Overlay Problems

There are many things that an active but hidden page can do to pose security or
functional issues while the overlay is displayed, including but not limited
to:
* Sensors continue to record
  * Camera and microphone continue to record (sometimes without or with
    inaccurate attribution)
  * Geolocation
* Input events may be surprisingly sent to the hidden active page
* Prompts may appear above the overlay, out of context
* Popups may appear, if a user activation is available
* Fullscreen mode may activate, obscuring the overlay
* Audio continues to play
* The page may continue to update unexpectedly
  * Videos continue to play past the point the user observed
* Built-in Chrome features or extensions may not be aware of the overlay and do
  surprising things
  * Accessibility
  * DevTools
  * Saving or printing pages
* Many Chrome features use content/ APIs to access the last committed URL or
  origin for functional or security purposes, being unaware of the cross-origin
  overlay.

This list is not exhaustive and the capabilities of web pages continue to expand
over time, so it is difficult to prevent all observable or dangerous behavior
while a user is unaware that a page is still present. This would be a fail-open
approach requiring us to diagnose and fix all problematic cases. Attempts to do
so in the past (e.g., Prerendering v1) have resulted in an unsustainable
maintenance burden.

A hidden active page also affects the user's mental model: they are more likely
to think the page has been unloaded, ending up confused if Chrome later reveals
it has been active but hidden.

Even temporary cases, such as showing an overlay until a timeout, can pose risks
to users, such as a "hot mic" situation if a video call appears to end but is
still in progress.

## Comparisons to Non-Overlay Cases

There are other situations where active web pages may continue to run while not
visible to the user, such as in a background tab.  Chrome's UI indicates the
change of security context to the user in these cases (e.g., using a different
tab and address bar contents), and continues to indicate the previous page
exists via the tab strip or tab switcher.  In contrast, full-page overlays
within a given tab imply to the user that the active page is no longer present,
even though it continues to run.

BFcache and Prerendering are two other features that keep pages in a hidden but
frozen or restricted state.  These pages are not fully active as in the overlay
case above.  However, not all pages can be put into this frozen or restricted
state, depending on which APIs are in use, whether the page opts out, which
other pages are in the same process, etc.  In such cases, BFcache and
Prerendering evict or discard the page.  As a result, freezing a page is not a
general technique that can be used when showing an overlay above any arbitrary
page.
