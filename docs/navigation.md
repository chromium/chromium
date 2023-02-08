# Life of a Navigation

Navigation is one of the main functions of a browser. It is the process through
which the user loads documents. This documentation traces the life of a
navigation from the time a URL is typed in the URL bar to the time the web page
is completely loaded. This is one example of many types of navigations, some of
which may start in different places (e.g., in the renderer process).

See also:
 * [Life of a Navigation tech talk](https://youtu.be/mX7jQsGCF6E) and
   [slides](https://docs.google.com/presentation/d/1YVqDmbXI0cllpfXD7TuewiexDNZYfwk6fRdmoXJbBlM/edit),
   for an overview from Chrome University.
 * [Navigation Concepts](navigation_concepts.md), for useful notes on
   navigation-related concepts in Chromium.

[TOC]


## BeforeUnload

Once a URL is entered, the first step of a navigation is to execute the
beforeunload event handler of the previous document, if a document is already
loaded. This allows the previous document to prompt the user whether they want
to leave, to avoid losing any unsaved data. In this case, the user can cancel
the navigation and no more work will be performed.


## Network Request and Response

If there is no beforeunload handler registered, or the user agrees to proceed,
the next step is making a network request to the specified URL to retrieve the
contents of the document to be rendered. (Note that not all navigations will go
to the actual network, for cases like ServiceWorkers, WebUI, cache, data:, etc.)
Assuming no network error is encountered (e.g. DNS resolution error, socket
connection timeout, etc.), the server will respond with data, with the response
headers coming first. The parsed headers give enough information to determine
what needs to be done next.

The HTTP response code allows the browser process to know whether one of the
following conditions has occurred:

* A successful response follows (2xx)
* A redirect has been encountered (response 3xx)
* An HTTP level error has occurred (response 4xx, 5xx)

There are two cases where a navigation network request can complete without
resulting in a new document being rendered. The first one is HTTP response code
204 or 205, which tells the browser that the response was successful, but there
is no content that follows, and therefore the current document must remain
active. The other case is when the server responds with a `Content-Disposition`
response header indicating that the response must be treated as a download
instead of a navigation.

If the server responds with a redirect, Chromium makes another request based on
the HTTP response code and the Location header. The browser continues following
redirects until either an error or a successful response is encountered.

Once there are no more redirects, the network stack determines if MIME type
sniffing is needed to detect what type of response the server has sent. This is
only needed if the response is not a 204/205 nor a download, doesn't already
have a `Content-Type` response header, and doesn’t include a
`X-Content-Type-Options: nosniff` response header. If MIME type sniffing is
needed, the network stack will read a small chunk of the actual response data
before proceeding with the commit.


## Commit

At this point the response is passed from the network stack to the browser
process to be used for rendering a new document. The browser process selects
an appropriate renderer process for the new document based on the origin and
headers of the response as well as the current process model and isolation
policy. It then sends the response to the chosen process, waiting for it to
create the document and send an acknowledgement. This acknowledgement from the
renderer process marks the _commit_ time, when the browser process changes its
security state to reflect the new document and creates a session history entry
for the previous document.

As part of creating the new document, the old document needs to be unloaded.
In navigations that stay in the same renderer process, the old document is
unloaded by Blink before the new document is created, including running any
registered unload handlers. In the case of a navigation that goes
cross-process, any unload handlers are executed in the previous document’s
process concurrently with the creation of the new document in the new process.

Once the creation of the new document is complete and the browser process
receives the commit message from the renderer process, the navigation is
complete.


## Loading

Even once navigation is complete, the user doesn't actually see the new page
yet. Most people use the word navigation to describe the act of moving from
one page to another, but in Chromium we separate that process into two phases.
So far we have described the _navigation_ phase; once the navigation has been
committed, Chromium moves into the _loading_ phase. Loading consists of
reading the remaining response data from the server, parsing it, rendering the
document so it is visible to the user, executing any script accompanying it,
and loading any subresources specified by the document.

The main reason for splitting into these two phases is that errors are treated
differently before and after a navigation commits. Consider the case where the
server responds with an HTTP error code. When this happens, the browser still
commits a new document, but that document is an error page. The error page is
either generated based on the HTTP response code or read as the response data
from the server. On the other hand, if a successful navigation has committed a
real document and has moved to the loading phase, it is still possible to
encounter an error, for example a network connection can be terminated or
times out. In that case the browser displays as much of the new document as it
can, without showing an error page.


## WebContentsObserver

Chromium exposes the various stages of navigation and document loading through
methods on the [WebContentsObserver] interface.

### Navigation

* `DidStartNavigation` - invoked after executing the beforeunload event handler
  and before making the initial network request.
* `DidRedirectNavigation` - invoked every time a server redirect is encountered.
* `ReadyToCommitNavigation` - invoked at the time the browser process has
  determined that it will commit the navigation and has picked a renderer
  process for it, but before it has sent it to the renderer process. It is not
  invoked for same-document navigations.
* `DidFinishNavigation` - invoked once the navigation has committed. The commit
  can be either an error page if the server responded with an error code or a
  successful document.


### Loading

* `DidStartLoading` - invoked once per WebContents, when a navigation is about
  to start, after executing the beforeunload handler. This is equivalent to the
  browser UI starting to show a spinner or other visual indicator for
  navigation and is invoked before the DidStartNavigation method for the
  navigation.
* `DOMContentLoaded` - invoked per RenderFrameHost, when the document itself
  has completed loading, but before subresources may have completed loading.
* `DidFinishLoad` - invoked per RenderFrameHost, when the document and all of
  its subresources have finished loading.
* `DidStopLoading` - invoked once per WebContents, when the top-level document,
  all of its subresources, all subframes, and their subresources have completed
  loading. This is equivalent to the browser UI stop showing a spinner or other
  visual indicator for navigation and loading.
* `DidFailLoad` - invoked per RenderFrameHost, when the document load failed,
  for example due to network connection termination before reading all of the
  response data.


## NavigationThrottles

NavigationThrottles allow observing, deferring, blocking, and canceling a given
navigation. They should not generally be used for modifying a navigation (e.g.,
simulating a redirect), as discussed in
[Navigation Concepts](navigation_concepts.md#rules-for-canceling-navigations).
They are typically registered in
`NavigationThrottleRunner::RegisterNavigationThrottles` or
`ContentBrowserClient::CreateThrottlesForNavigation`.

The most common NavigationThrottles events are `WillStartRequest`,
`WillRedirectRequest`, and `WillProcessResponse`, which allow intercepting a
navigation before sending the network request, during any redirects, and after
receiving the response. These events are only invoked on navigations that
require a URLLoader (see NavigationRequest::NeedsUrlLoader).
A NavigationThrottle that wishes to intercept a non-URLLoader navigation
(same-document navigations, about:blank, etc.) should register itself in
`NavigationThrottleRunner::RegisterNavigationThrottlesForCommitWithoutUrlLoader`,
and will get a single `WillCommitWithoutUrlLoader` event instead of the full
set of events centered on network requests. Page-activation navigations, such
as activating a prerendered page or restoring a page from the back-forward
cache, skip NavigationThrottles entirely.

[WebContentsObserver]: https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/web_contents_observer.h
