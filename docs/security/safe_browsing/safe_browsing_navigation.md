# How Safe Browsing Lookup Interacts with Navigation

## Overview

During navigation, Chrome checks the Safe Browsing reputation of each URL and
decides whether to show a warning to the user. This document describes how Safe
Browsing lookup interacts with navigation and how Safe Browsing lookups affect
the speed of navigation.

## Background

When a user navigates to a URL, Chrome checks the Safe Browsing reputation of
the URL before the URL is loaded. If Safe Browsing believes that the URL is
dangerous, Chrome shows a warning to the user:

![warning page](warning_screenshot.png)

Chrome can perform two types of Safe Browsing checks during navigation:

*   The well-known "hash-based" check
    ([Safe Browsing Update API (v4)](https://developers.google.com/safe-browsing/v4/update-api)).
*   The advanced
    ["real-time" check](https://source.chromium.org/chromium/chromium/src/+/main:components/safe_browsing/core/browser/realtime/).

Both of these checks are on the blocking path of navigation. Before the check is
completed, the navigation is not committed, the page body is not read by the
renderer, and the user won’t see any page content in their browser.

NOTE: There is another type of Safe Browsing check called Client Side Phishing
Detection (CSD). It also checks the reputation of the page. However, this check
is performed after the navigation is committed and it doesn’t block the
navigation, so it is out-of-scope for this doc.

## Navigation Basics

[Life of a Navigation](https://chromium.googlesource.com/chromium/src/+/main/docs/navigation.md)
gives a high level overview of a navigation from the time a URL is typed in the
URL bar to the time the web page is completely loaded. It breaks down a frame
navigation into two phases:

*   **Navigation phase**: From the time the network request is sent to the time
    the a navigation is committed. Note that at this point, nothing is rendered
    on the page.
    *   "Hash-based" check and "real-time" check can both be performed in this
        phase, depending on the user consent. "Real-time" check is only
        performed if the user has agreed to share URLs with Google.
*   **Loading phase**: Consists of reading the response body from the server,
    parsing it, rendering the document so it is visible to the user, executing
    any script, and loading any subresources (images, scripts, CSS files)
    specified by the document.
    *   Safe Browsing will also check URLs that fetch subresources at this
        stage, but only “hash-based” checks are performed.

[Navigation Concepts](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/navigation_concepts.md)
covers a set of important topics to understand navigation, such as:

*   Same-document and cross-document navigation. Same-document navigation keeps
    the same document and changes states associated with it. Some examples of
    same-document navigation are fragment navigation
    (`https://foo.com/1.html#fragment`) and using the
    [history.pushState](https://developer.mozilla.org/en-US/docs/Web/API/History/pushState)
    API.
    *   Same-document navigation can change the URL, but Safe Browsing doesn’t
        check these navigation. Same-document navigation doesn’t pose a security
        risk because no new content is loaded from the network.
*   Server redirects and client redirects. A server redirect happens when the
    browser receives a 300-level HTTP
    [response code](https://developer.mozilla.org/en-US/docs/Web/HTTP/Status#redirection_messages)
    before the document commits, telling it to request a different URL, possibly
    cross-origin. A client redirect happens after a document has been committed,
    when the HTML in the document instructs the browser to request a new
    document (e.g., via
    [meta tags](https://www.w3schools.com/tags/att_meta_http_equiv.asp) or
    [JavaScript](https://www.w3schools.com/howto/howto_js_redirect_webpage.asp)).
    *   Redirect URLs are all checked by Safe Browsing. Server redirects are
        checked in the navigation phase and client redirects are checked after a
        document is committed.

## Workflow

![workflow](safe_browsing_navigation_flowchart.png)

As illustrated above, Safe Browsing may block navigation in two phases:

*   In the navigation phase, Safe Browsing blocks the navigation before
    committing. Safe Browsing needs to finish checking all URLs (including
    redirect URLs) before committing the navigation. These checks are initiated
    from the browser process.
*   In the loading phase, Safe Browsing blocks loading subresources (images,
    scripts,...) before finishing the check for URLs that fetch them. These
    checks are initiated from the renderer process, but the check is still
    performed within the browser process. They are connected by the
    [SafeBrowsing mojo interface](https://source.chromium.org/chromium/chromium/src/+/main:components/safe_browsing/content/common/safe_browsing.mojom;l=14;drc=9d6b0dd8dc0ca9364cc74fe5df855466bb15ef67)
    and
    [SafeBrowsingUrlChecker mojo interface](https://source.chromium.org/chromium/chromium/src/+/main:components/safe_browsing/core/common/safe_browsing_url_checker.mojom;l=12;drc=efb0f6367a11f87129195032753f332b925ecf5f).
    If you see a page is rendered first and then a warning page is shown, it is
    likely triggered by subresources.

If one of the URLs (initial URL, redirect URLs, subresource URLs) is classified
as dangerous, a warning page will be shown and the navigation will be cancelled.
Note that some of the subresource URLs may not trigger a full page interstitial.
For example, when the threat type is “URL unwanted”
([code](https://source.chromium.org/chromium/chromium/src/+/main:components/safe_browsing/content/base_ui_manager.cc;l=207-221;drc=40a8d5fea781057ff74a699247bc699f71ca4d5d)).

## Speed

Safe Browsing checks and network requests are performed in parallel. Performing
a Safe Browsing check doesn’t block the start of network requests or the fetch
of response header and body. It doesn’t block redirects either.

However, completion of the Safe Browsing check does block the browser from
reading or parsing the response body. When the response header is received, Safe
Browsing will block the navigation if the check is not completed.

Safe Browsing won’t slow down the navigation if it is completed before the
response header is received. If Safe Browsing is not completed at this point,
the response body will still be fetched but the renderer won’t read or parse it.

We have two metrics to measure the speed of Safe Browsing checks on the
browser-side (include both hash-based check and real-time check):

*   SafeBrowsing.BrowserThrottle.IsCheckCompletedOnProcessResponse
    *   Measures whether the Safe Browsing check is completed when the response
        header is received. If completed, it won’t delay the navigation.
*   SafeBrowsing.BrowserThrottle.TotalDelay
    *   The total delayed time if Safe Browsing delays the navigation.

Similarly, we have two metrics to measure the speed of Safe Browsing checks on
the renderer-side (only include hash-based check because real-time check is not
available on the renderer-side):

*   SafeBrowsing.RendererThrottle.IsCheckCompletedOnProcessResponse
*   SafeBrowsing.RendererThrottle.TotalDelay

## Implementation Details

Safe Browsing blocks navigation by implementing the
[URLLoaderThrottle](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/common/loader/url_loader_throttle.h;l=43;drc=0e45c020c43b1a9f6d2870ff7f92b30a2f03a458)
interface. This interface provides several phases to defer URL loading:

*   `WillStartRequest(request, defer)`
*   `WillRedirectRequest(request, defer)`
*   `WillProcessResponse(request, defer)`

The throttle can mark `defer` as true if it wants to defer the navigation and
can call Resume to resume the navigation.

The throttle on the browser side is
[BrowserUrlLoaderThrottle](https://source.chromium.org/chromium/chromium/src/+/main:components/safe_browsing/content/browser/browser_url_loader_throttle.h;drc=b9e2c27ca6debc7796367f295888ba4701df62bf)
and the throttle on the render side is
[RendererUrlLoaderThrottle](https://source.chromium.org/chromium/chromium/src/+/main:components/safe_browsing/content/renderer/renderer_url_loader_throttle.cc;l=107;drc=40a8d5fea781057ff74a699247bc699f71ca4d5d;bpv=1;bpt=1).
The throttles only mark `defer` as true in
[WillProcessResponse](https://source.chromium.org/chromium/chromium/src/+/main:components/safe_browsing/content/browser/browser_url_loader_throttle.cc;l=264;drc=40a8d5fea781057ff74a699247bc699f71ca4d5d).

Note that another way of deferring and blocking navigation is by implementing
the
[NavigationThrottle](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/navigation_throttle.h;drc=40a8d5fea781057ff74a699247bc699f71ca4d5d;l=18)
interface. This interface doesn’t fit the Safe Browsing use case because it
cannot defer loading subresources.

Safe Browsing doesn’t defer navigation forever. The current timeout is set to 5
seconds. If the check is not completed in
[5 seconds](https://source.chromium.org/chromium/chromium/src/+/main:components/safe_browsing/core/browser/safe_browsing_url_checker_impl.cc;l=33;drc=c40dd9cd443bc4ca05647f8d582c38d5bdfb6814),
the navigation will resume.
