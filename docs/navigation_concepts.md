# Navigation Concepts

This documentation covers a set of important topics to understand related to
navigation. For a timeline of how a given navigation proceeds, see [Life of a
Navigation](navigation.md).

[TOC]


## Same-Document and Cross-Document Navigations

Chromium defines two types of navigations based on whether the navigation
results in a new document or not. A _cross-document_ navigation is one that
results in creating a new document to replace an existing document. This is
the type of navigation that most users are familiar with. A _same-document_
navigation does not create a new document, but rather keeps the same document
and changes state associated with it. A same-document navigation does create a
new session history entry, even though the same document remains active. This
can be the result of one of the following cases:

* Navigating to a fragment within an existing document (e.g.
  `https://foo.com/1.html#fragment`).
* A document calling the `history.pushState()` or `history.replaceState()` APIs.
* A new document created via `document.open()`, which may change the URL to
  match the document that initiated the call (possibly from another frame).
* A session history navigation that stays in the same document, such as going
  back/forward to an existing entry for the same document.


## Browser-Initiated and Renderer-Initiated Navigations

Chromium also defines two types of navigations based on which process started
the navigation: _browser-initiated_ and _renderer-initiated_. This distinction
is useful when making decisions about navigations, for example whether an
ongoing navigation needs to be cancelled or not when a new navigation is
starting. It is also used for some security decisions, such as whether to
display the target URL of the navigation in the address bar or not.
Browser-initiated navigations are more trustworthy, as they are usually in
response to a user interaction with the UI of the browser. Renderer-initiated
navigations originate in the renderer process, which may be under the control of
an attacker. Note that some renderer-initiated navigations may be considered
user-initiated, if they were performed with a [user
activation](https://mustaqahmed.github.io/user-activation-v2/) (e.g., links),
while others are not user-initiated (e.g., script navigations).


## Last Committed, Pending, and Visible URLs

Many features care about the URL or Origin of a given document, or about a
pending navigation, or about what is showing in the address bar. These are all
different concepts with different security implications, so be sure to use the
correct value for your use case.

See [Origin vs URL](security/origin-vs-url.md) when deciding whether to check
the Origin or URL. In many cases that care about the security context, Origin
should be preferred.

The _last committed_ URL or Origin represents the document that is currently in
the frame, regardless of what is showing in the address bar. This is almost
always what should be used for feature-related state, unless a feature is
explicitly tied to the address bar (e.g., padlock icon). This is empty if no
navigation is ever committed. e.g. if a tab is newly open for a navigation but
then the navigation got cancelled. See
`RenderFrameHost::GetLastCommittedOrigin` (or URL) and
`NavigationController::GetLastCommittedEntry`.

The _pending_ URL exists when a main frame navigation has started but has not
yet committed. This URL is only sometimes shown to the user in the address bar;
see the description of visible URLs below. Features should rarely need to care
about the pending URL, unless they are probing for a navigation they expect to
have started. See `NavigationController::GetPendingEntry`.

The _visible_ URL is what the address bar displays. This is carefully managed to
show the main frame's last committed URL in most cases, and the pending URL in
cases where it is safe and unlikely to be abused for a _URL spoof attack_ (where
an attacker is able to display content as if it came from a victim URL). In
general, the visible URL is:

 * The pending URL for browser-initiated navigations like typed URLs or
   bookmarks, excluding session history navigations. This becomes empty if the
   navigation is cancelled.
 * The last committed URL for renderer-initiated navigations, where an attacker
   might have control over the contents of the document and the pending URL.
   This is also used when there is no ongoing navigations, and it is empty when
   no navigation is ever committed.
 * A renderer-initiated navigation's URL is only visible while pending if it
   opens in a new unmodified tab (so that an unhelpful `about:blank` URL is not
   displayed), but only until another document tries to access the initial empty
   document of the new tab. For example, an attacker window might open a new tab
   to a slow victim URL, then inject content into the initial `about:blank`
   document as if the slow URL had committed. If that occurs, the visible URL
   reverts to `about:blank` to avoid a URL spoof scenario. Once the initial
   navigation commits in the new tab, pending renderer-initiated navigation URLs
   are no longer displayed.


## Virtual URLs

Virtual URLs are a way for features to change how certain URLs are displayed to
the user (whether visible or committed). They are generally implemented using
BrowserURLHandlers. Examples include:

 * View Source URLs, where the `view-source:` prefix is not present in the
   actual committed URL.
 * DOM Distiller URLs, where the original URL is displayed to the user rather
   than the more complex distiller URL.


## Redirects

Navigations can redirect to other URLs in two different ways.

A _server redirect_ happens when the browser receives a 300-level HTTP response
code before the document commits, telling it to request a different URL,
possibly cross-origin. The new request will usually be an HTTP GET request,
unless the redirect is triggered by a 307 or 308 response code, which preserves
the original request method and body. Server redirects are managed by a single
NavigationRequest. No document is committed to session history, but the original
URL remains in the redirect chain.

In contrast, a _client redirect_ happens after a document has committed, when
the HTML in the document instructs the browser to request a new document (e.g.,
via meta tags or JavaScript). Blink classifies the navigation as a client
redirect based partly on how much time has passed. In this case, a session
history item is created for the redirecting document, but it gets replaced when
the actual destination document commits. A separate NavigationRequest is used
for the second navigation.


## Concurrent Navigations

Many navigations can be in progress simultaneously. In general, every frame is
considered independent and may have its own navigations(s), with each tracked by
a NavigationRequest. Within a frame, it is possible to have multiple concurrent
navigations:

 * **A cross-document navigation waiting for its final response (at most one per
   frame).** The NavigationRequest is owned by FrameTreeNode during this stage,
   which can take several seconds. Some special case navigations do not use a
   network request and skip this stage (e.g., `about:blank`, `about:srcdoc`,
   MHTML).
 * **A queue of cross-document navigations that are between "ready to commit"
   and "commit," while the browser process waits for a commit acknowledgement
   from the renderer process.** While rare, it is possible for multiple
   navigations to be in this stage concurrently if the renderer process is slow.
   The NavigationRequests are owned by the RenderFrameHost during this stage,
   which is usually short-lived.
 * **Same-document navigations.** These can be:
    * Renderer-initiated (e.g., `pushState`, fragment link click). In this case,
      the browser process creates and destroys a NavigationRequest in the same
      task.
    * Browser-initiated (e.g., omnibox fragment change). In this case, the
      browser process creates a NavigationRequest owned by the RenderFrameHost
      and immediately tells the renderer to commit.

Note that the navigation code is not re-entrant. Callers must not start a new
navigation while a call to `NavigateWithoutEntry` or
`NavigateToExistingPendingEntry` is on the stack, to avoid a CHECK that guards
against use-after-free for `pending_entry_`.


## Rules for Canceling Navigations

We generally do not want an abusive page to prevent the user from navigating
away, such as by endlessly starting new navigations that interrupt or cancel the
user's attempts. Generally, a new navigation will cancel an existing one in a
frame, but we make the following exception: a renderer-initiated navigation is
ignored iff there is an ongoing browser-initiated navigation and the new
navigation lacks a user activation. (This is implemented in
`Navigator::ShouldIgnoreIncomingRendererRequest`.)

NavigationThrottles also have an ability to cancel navigations when desired by a
feature. Keep in mind that it is problematic to simulate a redirect by canceling
a navigation and starting a new one, since this may lose relevant context from
the original navigation (e.g., ReloadType, CSP state, Sec-Fetch-Metadata state,
redirect chain, etc), and it will lead to unexpected observer events and metrics
(e.g., extra navigation starts, inflated numbers of canceled navigations, etc).
Feature authors that want to simulate redirects may want to consider using a
URLLoaderRequestInterceptor instead.


## Error Pages

There are several types of error pages that can be displayed when a navigation
is not successful.

The server can return a custom error page, such as a 400 or 500 level HTTP
response code page. These pages are rendered much like a successful navigation
to the site (and go into an appropriate process for that site), but the error
code is available and `NavigationHandle::IsErrorPage()` is true.

If the navigation fails to get a response from the server (e.g., the DNS lookup
fails), then Chromium will display an error page. For main frames, this error
page will be in a special error page process, not affiliated with any site or
containing any untrustworthy content from the web. In these failed cases,
NetErrorHelperCore may try to reload the URL at a later time (e.g., if a network
connection comes back online), to load the document in an appropriate process.

If instead the navigation is blocked (e.g., by an extension API or a
NavigationThrottle), then Chromium will similarly display an error page in a
special error page process. However, in blocked cases, Chromium will not attempt
to reload the URL at a later time.


## Interstitial Pages

Interstitial pages are implemented as committed error pages. (Prior to
[issue 448486](https://crbug.com/448486), they were implemented as overlays.)
The original in-progress navigation is canceled when the interstitial is
displayed, and Chromium repeats the navigation if the user chooses to proceed.

Note that some interstitials can be shown after a page has committed (e.g., when
a subresource load triggers a Safe Browsing error). In this case, Chromium
navigates away from the original page to the interstitial page, with the intent
of replacing the original NavigationEntry. However, the original NavigationEntry
is preserved in `NavigationControllerImpl::entry_replaced_by_post_commit_error_`
in case the user chooses to dismiss the interstitial and return to the original
page.
