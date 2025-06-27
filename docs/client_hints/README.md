# Life of Client Hint

Client hints are a set of headers that a server can proactively request from a
client to get information about the device, network, user preference and
User-Agent specific preference. This document traces the life of a Client Hint,
from the server's request in a response header to the client sending the
corresponding hints in request headers. This allows committers to have a better
understanding the Client Hints mechanism and the underlying implementation
changes, reducing efforts when debugging issues related to Client Hints
architecture.

See also:

*   [HTTP Client Hint RFC](https://datatracker.ietf.org/doc/html/rfc8942)
*   [User-Agent Client Hints Specification](https://wicg.github.io/ua-client-hints/)

[TOC]

## Background

### Client Hints Usage

A site must explicitly announce that it supports Client Hints. One primary way
is using the `Accept-CH` header to specify the hints that the site is interested
in.

```
Accept-CH: Sec-CH-Width, Sec-CH-UA-Bitness
```

When a site sends the `Accept-CH` header with a list of Client Hints, a client
will choose to append some or all of the listed Client Hint headers to
subsequent requests for that origin. For example, `Sec-CH-UA-Bitness`, the
client uses a system library to determine the "bitness" of the underlying CPU
architecture and then appends it to the request header. The browser stores those
preferences on a per-origin basis when the `Accept-CH` header is received in a
response to a secure, top-level navigation request.

Sites can also specify Client Hints in HTML using the `<meta>` element with the
`http-equiv` attribute.

```
<meta http-equiv="Accept-CH" content="Sec-CH-Width, Sec-CH-UA-Bitness" />
```

For User-Agent Client Hints, it has an additional way to expose browser and
platform information via [JavaScript
API](https://developer.mozilla.org/en-US/docs/Web/API/User-Agent_Client_Hints_API):
`NavigatorUAData`, and `NavigatorUAData.getHighEntropyValues()`.

### Client Hints Cache

The Client Hints cache can only be accessed (read and write) via a delegate in
the browser process. Only the main frame can write the Client Hints cache (This
is because having more than one method of affecting the Client Hints storage
introduces far more unnecessary complexity to the implementation, the
specification and the developers). In other words, writing to the Client Hints
cache requires passing several checks: the navigation must be top-frame, in a
secure context, and have the necessary JS permissions and feature flags enabled.

In detail, Client Hints are read and written using the C++
`ClientHintsControllerDelegate` class which is a property of
`content::BrowserContext`. In `//chrome` space, this delegate is implemented as
the `ProfileImpl` and `OffTheRecordProfileImpl`. The delegate provides read and
write Client Hints on a per-origin basis. Each platform has its own overrides
implementation, such as principal implementation `client_hints::ClientHints` in
`//components`, `AwClientHintsControllerDelegate` in Android WebView, and
`InMemoryClientHintsControllerDelegate` for Fuchsia. Client hints values are
read and written to the `PrefService` if the platform supports it, otherwise
using in-memory `std::map` as the cache media. Client Hint preferences are
stored in the preferences service as a origin-based content setting
(`ContentSettingsType::CLIENT_HINTS`).

**Note:** Any storage related to an incognito mode profile is cleared when the
last incognito tab is closed. Incognito profile (an off-the-record profile that
is used for incognito mode) will have its own separate Client Hints storage.

### Client Hints Storage Lifetime

Since Chrome M103, Client hints storage is marked as `SessionModel::Durable`,
meaning the storage no longer has a predefined expiration lifetime. Client Hints
preferences can be read from disk when the browser starts up and loaded into
memory. This also means the Client Hints storage persist until either of the
following happens:

*   when session cookies are cleared.
*   when a user clears site data or cookies for a given origin.
*   when an origin responds with an empty `Accept-CH`.

## Overview

To support Client Hints on a new platform or embedder, the platform should
implement its own `ClientHintsControllerDelegate`. For User-Agent Client Hints,
it also needs to override the `GetUserAgentMetadata()` method to generate the
User-Agent metadata, which usually involves calling `embedder_support
::GetUserAgentMetadata` in the component layer. We will discuss more in detailed
design.

To address the issue that the client may not know what Client Hints to send on
the first page load, we provided two mechanisms (Client Hint Reliability, [RFC
draft](https://github.com/Tanych/http-client-hint-reliability)) to fix it:

*   Critical-CH header (an HTTP header to retry the request)
*   A connection-level optimization (TLS ALPS frame in HTTP2/HTTP3)

To add a new Client Hint, we usually follow [the guide
here](./components/client_hints/README.md) or find the latest example CL of
adding a Client Hint.

## Detailed Design

As mentioned earlier, here are the major ways to access the Client Hints:

*   Ways for sites specify the interested Client Hints
    *   HTTP Accept-CH header
    *   `<meta>` tag
*   Ways for sites getting Client Hints
    *   JS interface for User-Agent Client Hints

### Parse Response Headers

When a site support Client Hints via the HTTP headers, it specifies the Client
Hints preferences in the `Accept-CH` header and can optionally send the
`Critical-CH` header with critical Client Hints. To allow navigation requests to
access those two response headers, we added two optional
`array<WebClientHintsType>` fields : `accept_ch` and `critical_ch`, to the
`ParsedHeaders` struct. These are populated in the `PopulateParsedHeaders`
function by `ParseClientHintsHeader`, which parses `Accept-CH` and `Critical-CH`
headers and returns the parsed representations of Client Hints as
`array<WebClientHintsType>`.

**Notes:** An `Accept-CH` header with empty value (e.g.`Accept-CH:`) is
considered valid. This instructs the browser to clear the Client Hints storage
for the given origin.

### Subresource

By design, when a site enables Client Hints, the top-level document's
preferences are used to determine which hints to delegate to subresource
requests associated with that document. This delegation is governed by settings
like Permissions Policy.

When requests are initiated from a document, the Client Hints are filtered
through [Permission
Policies](https://w3c.github.io/webappsec-permissions-policy/), which allows
origins to control what features are available to third parties within a
document. By default, if an embedded document is same-origin with its embedder,
then Permissions Policy will delegate all available hints. For example, when a
user visits `https://a.com` with Client Hints `sec-ch-ua-bitness`,
`sec-ch-ua-platform-version` in the request header, the subresource request
`https://a.com/b.img` will have the same Client Hints preferences in the request
header.

The policy can specify a list of allowed origins or an asterisk `*` for all
origins (low-entropy hints like `Sec-CH-UA`, `Sec-CH-UA-Mobile`
,`Sec-CH-UA-Platform` and `Save-Data` are considered safe to send to all
subresources by default, so their policy defaults to `*`). Permissions can also
be set in HTML for iframes in the same format through the allow attribute. See
more details in [Permissions Policy and Client
Hints](https://github.com/w3c/webappsec-permissions-policy/blob/main/permissions-policy-client-hints.md).

### Critical-CH

As one of Client Hint Reliability mechanisms, the `Critical-CH` response header
is used to specify important hints for the origin to have on the very first
request. For example, a server may use the `Sec-CH-Device-Memory` Client Hint to
select simple and complex variants of a resource to different user agents. Such
a resource should be fetched consistently across page loads even in the very
first request to prevent inconsistent behavior. The interaction between
`Critical-CH` and `Accept-CH` is discussed in a later section.

### Fenced Frame

Based on [Fenced
Frame](https://developer.chrome.com/en/docs/privacy-sandbox/fenced-frame/)’s
privacy guarantee, it doesn't allow communication between the embedding frame
and the fenced frame. Currently, a fenced frame won’t have access to the Client
Hints storage of the embedding page. There are ongoing discussions regarding how
to handle Client Hints in a fenced frame, see [the github
issue](https://github.com/WICG/fenced-frame/blob/master/explainer/permission_document_policies.md#ua-client-hints-open-question).
If a frame is inside a fenced frame, its origin is considered as opaque when
looking up Client Hint preferences.

### Content-Security-Policy (CSP) Sandbox

Initially, we only considered the request’s URL when creating the origin to
access the Client Hints storage for navigation. However, this might be an issue
when a sandboxed iframe opens a popup (with the attribute `allow-popup`). In
this case, the popup's origin should be considered opaque, not the origin of its
URL. When a new browsing context is created, it can inherit a ["sandboxing flag
set"](https://html.spec.whatwg.org/multipage/origin.html#determining-the-creation-sandboxing-flags)
from the embedding iframe or document. If any of the [sandboxed origin browsing
context
flags](https://html.spec.whatwg.org/multipage/origin.html#sandboxed-origin-browsing-context-flag)
is set, the resulting origin is `opaque`.

## Implementation Details

### Top-level Document

#### Before Receiving Response

The following diagram shows how Client Hints are added to the HTTP request
header before a response is received from the site. We only parse and persist
Client Hints for the top-level document.

![initial navigation
flow](/docs/client_hints/images/initial_navigation_flow.png)

When initializing the `NavigationRequest` in `//content` layer, we look up the
existing Client Hints storage to add them to the HTTP request header(the
decision to add a specific hint also considers other settings like Permissions
Policy). For User-Agent Client Hints, one special case is User-Agent overrides.
If User-Agent has been overridden and the overridden User-Agent metadata is also
not empty, the User-Agent Client Hints will be added to the requests header.
Otherwise, no User-Agent Client Hints will be added. For details, check the
current implementation of `UpdateNavigationRequestClientUaHeadersImpl`.

#### After Receiving Response

Once the site's response is available, `CriticalClientHintsThrottle` (a
`URLLoaderThrottle` that is registered on top-level navigation requests)
monitors whether the corresponding request needs to be restarted based on the
values in the `Critical-CH` header and the existing Client Hints storage. It’s
run on every response in the navigation it’s assigned to. The restart is
implemented as an internal redirect. It replaces the original response and is
sent to the navigation stack to proceed as normal.

![navigation response
flow](/docs/client_hints/images/navigation_response_flow.png)

In `URLLoaderThrottle`, responses are handled separately for normal(a.k.a.
“final”) response and redirect response. When a redirect response occurs, the
//net stack is notified of a redirect, not a final response. This notification
is sent via a `URLLoaderClient` mojo pipe through either `OnReceivedResponse` or
`OnReceivedRedirect`. The `ThrottlingURLLoader` (a wrapper around the
`URLLoader`) loops through each throttle and calls
`BeforeWillProcessResponse`/`WillProcessResponse` or
`BeforeWillRedirectRequest`/`WillRedirectRequest`. This means responses handled
by `OnReceivedRedirect` are not also processed by `OnReceivedResponse`. This is
the reason why we need to implement both `BeforeWillProcessResponse` and
`BeforeWillRedirectRequest` in the `CriticalClientHintsThrottle`.

In the following sections, we will discuss detailed workflow on handling normal
and redirect responses with and without `Critical-CH`.

##### Normal 200

For a normal 200 response without restarting the request, we parse the
`Accept-CH header` and persist the Client Hints preferences
(`ParseAndPersistAcceptCHForNavigation`) if it’s top-level frame during
navigation commit(`​​NavigationRequest::CommitNavigation`). We also update
`enabled_client_hints` commit parameter to allow subresource requests to inherit
the top-level frame’s Client Hints storage.

##### Redirect 301/302

For a redirect response like 301, 302 without a restart, we parse the
`Accept-CH` header from the redirect response and persist the Client Hints
preferences when checking redirect completion
(`NavigationRequest::OnRedirectChecksComplete`). This means the subsequent
request header to the redirect page will include the Client Hints which
specified in the original response `Accept-CH` header. Once the response of the
redirect page is available, the remaining workflow is similar to a normal 200
response:

*   Parsing and persisting the Client Hints storage again using the redirect
    page response header.
*   Updating the `enabled_client_hints` commit params.

The following example explains how the redirect request works:

```
GET /foo HTTP/1.1
Host: example.com

HTTP/1.1 301 Moved Permanently
Accept-Ch: Sec-CH-UA-Platform-Version,Sec-CH-UA-Bitness
Location: /bar

GET /bar
Sec-CH-UA-Platform-Version: "6.1.25"
Sec-CH-UA-Bitness: "64"
Host: example.com

HTTP/1.1 200 OK
Accept-Ch: Sec-CH-UA-Full-Version-List
```

The first request to `example.com/foo` won’t include any Client Hints since
clients don’t know what to send before receiving any response. Once the redirect
response arrives, it will update the Client Hints storage to include
`Sec-CH-UA-Platform-Version` and `Sec-CH-UA-Bitness`, and the request to the new
redirect location `/bar` will include these hints. However, if the final
redirect location 200 response has a different Client Hints preference (e.g.,
`Sec-CH-UA-Full-Version-List`), it will update the Client Hints storage as
`Sec-CH-UA-Full-Version-List` when committing the navigation. The next request
to `example.com` will then send `Sec-CH-UA-Full-Version-List` int the request
headers. Therefore, we recommend that sites send a consistent Client Hints
preference for the same origin to avoid frequently changing Client Hints
storage.

##### Normal 200 with Critical-CH

As mentioned earlier, the `Critical-CH` restart mechanism was implemented as an
internal redirect. In this case, the response goes through
`CriticalClientHintsThrottle::BeforeWillProcessResponse`. Once there are missing
critical hints on the request header, we parse and persist the Client Hints
preferences based on the latest response and then trigger an internal 307
redirect to restart the request. As the internal redirect response won’t contain
the original response header (this means no `Accept-CH` header and `Critical-CH`
header on the internal redirect response), we won’t update the Client Hints
preferences storage when checking navigation redirect request completion
(`NavigationRequest::OnRedirectChecksComplete`). However, we parse and update
the Client Hints storage for the top-level frame when the final redirect
response arrives and the navigation is committed.

##### Redirect 301/302 with Critical-CH

In this scenario, a site redirects a request to a different location, and the
responses include both `Accept-CH` header and `Critical-CH` header. The redirect
response goes through `CriticalClientHintsThrottle::BeforeWillRedirectRequest`,
where we check if any critical Client Hints are missing. Then, we parse and
update the Client Hints storage on `MaybeRestartWithHints` accordingly. If a
restart is needed, an internal redirect 307 will be sent to replace the original
response. As mentioned earlier, we won’t update Client Hints preference storage
for the internal redirect 307. The restarted request will include the missing
critical Client Hints in its headers and then proceed as normal redirect
requests, without further restarts.

**Notes:** To avoid endless redirect loops, we only restart once per origin.

The following example explains in details:

```
GET /foo HTTP/1.1
Host: example.com

HTTP/1.1 301 Moved Permanently
Accept-CH: Sec-CH-UA-Platform-Version,Sec-CH-UA-Bitness
Critical-CH: Sec-CH-UA-Platform-Version
Location: /bar

Internal Redirect
HTTP/1.1 307 Internal Redirect

GET /foo HTTP/1.1
Host: example.com
Sec-CH-UA-Platform-Version: "6.1.25"
Sec-CH-UA-Bitness: "64"

HTTP/1.1 301 Moved Permanently
Accept-CH: Sec-CH-UA-Platform-Version,Sec-CH-UA-Bitness
Critical-CH: Sec-CH-UA-Platform-Version
Location: /bar

GET /bar
Sec-CH-UA-Platform-Version: "6.1.25"
Sec-CH-UA-Bitness: "64"
Host: example.com

HTTP/1.1 200 OK
Accept-CH: Sec-CH-UA-Platform-Version,Sec-CH-UA-Bitness
Critical-CH: Sec-CH-UA-Platform-Version
```

The first request to `/foo` doesn’t contains any Client Hints header. The
browser receives the 301 redirect response with critical hint
`Sec-CH-UA-Platform-Version`. As the critical hint was missing from the original
request, the request to `/foo` will be restarted via an internal 307 redirect.
The restarted request to `/foo` will include Client Hints headers
`Sec-CH-UA-Platform-Version` and `Sec-CH-UA-Bitness` and then follow the
redirect new direction `/bar` as described in the [Redirect 301/302
section](#redirect-301302).

### Subresource Requests

#### General Client Hints

This section will cover the implementation details on adding Client Hints to
subresource requests. Since we only parse and store preferences for the
top-level frame, these preferences are passed to the document via the
`enabled_client_hints` property of `CommitNavigationParams` when the navigation
commits. It carries the commit params to
`RenderFrameHostImpl::CommitNavigation`, which calls
`NavigationClient::CommitNavigation`. The data is then transferred into a Blink
format `WebNavigationParams` when calling `RenderFrameImpl::CommitNavigation`.
Finally, the parameters are passed down to `FrameLoader::CommitNavigation`, and
the Client Hint preferences are saved to `client_hints_preferences_` when the
document starts loading(`DocumentLoader::StartLoadingInternal`). This allows
`FrameFetchContext` to add the appropriate Client Hint headers when populating
resource request.

![subresource Client
Hints](/docs/client_hints/images/subresource_client_hints.png)

#### User-Agent Client Hints

To populate User-Agent Client Hints on the subresource requests, one additional
information is getting the User-Agent metadata. A frame fetch request will get
the User-Agent metadata from `UserAgentMetadata()` in the `LocalFrameClient`
interface. Implementation is in `LocalFrameClientImpl::UserAgentMetadata()`.

![ua Client Hints](/docs/client_hints/images/ua_client_hints.png)

The logic then branches based on whether the User-Agent has been overridden.

If the User-Agent has **not** been overridden, it calls the
`Platform::UserAgentMetadata()` API. The `RenderBlinkPlatformImpl`
implementation overrides `UserAgentMetadata` and calls
`RenderThreadImpl::GetUserAgentMetadata()`. It returns `user_agent_metadata_`
which is updated by `RenderProcessHostImpl::Init()` via Mojo.
`RenderProcessHostImpl` will call the platform's content browser client to get
their own User-Agent metadata. Usually, different platforms call
`embedder_support::GetUserAgentMetadata` in `//components` layer to generate the
consistent User-Agent Client Hints metadata across all platforms. Exceptions
include Android WebView and legacy Headless, which overrides the brand lists,
and the Shell client, which provides its own User-Agent metadata for testing.

If the User-Agent has been overridden, it calls
`WebLocalFrameClient::UserAgentMetadataOverride` API to get the User-Agent
metadata. `RenderFrameImpl`, which implements the `WebLocalFrameClient`
interface, overrides the `UserAgentMetadataOverride` method to get the
User-Agent metadata from render preferences via
`blink::WebView::GetRendererPreferences`. `Blink::WebView` (implemented as
`WebViewImpl`) gets the render preferences via Mojo. To update the render
preferences with the overridden User-Agent metadata, `RenderViewHostImpl`
broadcasts the `renderer_preferences_` to all renders via Mojo once a platform
has set the User-Agent metadata overrides.

Before returning the final User-Agent metadata, we had a probe for
`InspectorEmulationAgent` to apply User-Agent metadata overrides from devtools
if it has.

### \<meta\> tag

As described in the [Subresource Requests section](#subresource-requests),
Client hints specified in a `<meta>` tag is one of the sources that
`FrameFetchContext` users when adding Client Hints to the subresource requests.

When `HTMLDocumentParser` processes a Client Hints `<meta>` tag, it updates the
local frame’s `client_hints_preferences_` based on permission policy via
`ClientHintsPreferences::UpdateFromMetaCH`. For security reasons, we only
support `<meta>` tags in the raw HTML text. Client Hint preferences will not be
updated if the tag is injected via JavaScript.

### JavaScript Interface

User-Agent Client Hints can be accessed through the JavaScript interface:
`navigator.NavigatorUAData` and `NavigatorUAData: getHighEntropyValues()`.
`getHighEntropyValues()` is an asynchronous API, it returns a Promise that
resolves with a dictionary object containing the User-Agent high entropy Client
Hints values. As `getHighEntropyValues()` could reveal more information, its
asynchronous design allows the browser time to perform checks, such as
requesting user permission.

When users call JS interface to get User-Agent Client Hints, the key step is
still getting the User-Agent metadata. `NavigatorBase::GetUserAgentMetadata()`
implements the `NavigatorUA::GetUserAgentMetadata()` interface and sets the
User-Agent Client Hints data in `NavigatorUAData`. The JS call gets the
User-Agent metadata from `UserAgentMetadata()` in the `LocalFrameClient`
interface. The remaining logics are the same as subresource requests getting the
User-Agent metadata.

![js ua Client Hints](/docs/client_hints/images/js_ua_client_hints.png)

When web developers call `getHighEntropyValues()`, it reads data from
`NavigatorUAData`, triggers a promise resolver, and returns the values
asynchronously.

### ACCEPT_CH Frame

The ACCEPT_CH frame is one of mechanisms defined in the Client Hints Reliability
protocol [RFC
draft](https://datatracker.ietf.org/doc/draft-victortan-httpbis-chr-accept-ch-frame/).
We take advantage of
[ALPS](https://datatracker.ietf.org/doc/html/draft-vvv-tls-alps-01)
(Application-Layer Protocol Settings) to deliver the ACCEPT_CH frame over the
TLS handshake before the first application protocol round trip. For Client
Hints, the ACCEPT_CH HTTP/2 and HTTP/3 frame is added to the TLS handshake. It's
a connection-level optimization to avoid the performance hit of a retry in most
cases. The browser can see the connection-level settings and react appropriately
before sending the request to the server. It’s not an alternative mechanism to
replace the `Accept-CH` header. The goal of `ACCEPT_CH` frame is to provide a
reliable way to make Client Hints available in requests header avoiding an extra
network round-trip in the common cases.

#### Passing ACCEPT_CH frame to URLRequest

Once the server sends the `ACCEPT_CH` frame during TLS handshake, we can access
that data from `HttpStream::GetAcceptChViaAlps()`.

*   For HTTP/2 frames, `AlpsDecoder` in `//net/spdy` is responsible for decoding
    the `ACCEPT_CH` frame. `SpdySession` saves the `ACCEPT_CH` frame entries
    (where an entry is a Client Hints preference and origin pair) to a hash map.
*   For HTTP/3 frames, `OnAcceptChFrameReceivedViaAlps` in
    `QuicChromiumClientSession` will analyze the `ACCEPT_CH` frame and save the
    entries to a hash map. The QUIC stack is responsible for decoding the
    `ACCEPT_CH` frame.
*   A `ConnectedCallback` is used to carry the `accept_ch_frame` raw string from
    `HttpStream` to `URLRequest` as a property of `TransportInfo` struct.

![accept_ch frame url
request](/docs/client_hints/images/accept_ch_frame_url_request.png)

See here for detailed documents on [how to request an
HttpSteam](https://chromium.googlesource.com/chromium/src/+/refs/heads/master/net/docs/life-of-a-url-request.md#check-the-cache_request-an-httpstream).

#### AcceptCHFrameObserver

To send Client Hints messages from network service to the browser process, we
added a Mojo interface `AcceptCHFrameObserver` which is owned by a single
`URLLoader`. The observer checks whether an `ACCEPT_CH` frame received over
HTTP/2 or HTTP/3 affects the associated navigation request.
`OnAcceptCHFrameReceived` informs the observer an `ACCEPT_CH` frame was received
for a given URL. The observer MUST check if the Client Hints in the frame need
to be added to the request. If so, the request should drop the associated
`URLLoader`, terminate the Mojo pipe, and issue a new request with the updated
headers.

![accept_ch frame
observer](/docs/client_hints/images/accept_ch_frame_observer.png)

#### OnAcceptCHFrameReceived

`OnAcceptCHFrameReceived` is used to inform `AcceptCHFrameObserver` that an
`ACCEPT_CH` frame was received for the given origin. Once the `accept_ch_frame`
is received, we filter all enabled Client Hints and check whether any Client
Hints are missed from the frame tree node. If so, we set those Client Hints as
additional Client Hints and store them in memory on the connection, but not
persistent in prefs storage. Then, we apply those Client Hints to the
`modified_headers` and clean the Client Hints stored in the memory for the given
origin. Finally, we check whether those Client Hints are already in the
`resource_request_->headers`. If not, we restart the request and reset the
`blink::ThrottlingURLLoader`.

#### A Simple Request

![accept_ch frame simple
request](/docs/client_hints/images/accept_ch_frame_simple_request.png)

The following are the major steps for how the `ACCEPT_CH` frame Client Hints
added to the request header when visiting `https://a.com`:

1.  `NavigationRequest` initialize the request and call
    `NavigationURLLoaderImpl` to start it.
1.  Initially, the `ThrottlingURLLoader(url_loader_)` is null and
    `NavigationURLLoaderImpl` creates one when it calls `MaybeStartLoader`.
1.  When constructing `ThrottlingURLLoader`, it calls
    `ThrottlingURLLoader::Start()` to start the request before returning to
    `NavigationURLLoaderImpl`.
1.  Once `url_loader_` is created, it calls `URLLoaderFactory` to create an
    instance of `URLLoader` (to perform a single request to a URL).
    `NetworkService` can now send the `ACCEPT_CH` frame message to the
    browser/render process once the message pipe is created for the `URLLoader`.
1.  `URLLoader`'s `onConnected` callback checks if there are Client Hints
    (retrieved from `TransportInfo` in `HttpStream`) that have not yet been
    added to the request header. In this initial request, only default hints are
    present. It notifies the `AcceptCHFrameObserver` (via the Mojo interface),
    which then calls `OnAcceptCHFrameReceived` to add the `ACCEPT_CH` frame
    hints and returns `net::ERR_IO_PENDING`.
1.  Once the `ACCEPT_CH` frame is received, `OnAcceptCHFrameReceived` adds the
    expected Client Hints to the requests, stores the Client Hints in memory and
    cleans them before restarting the request. It also reset the `url_loader_`.
1.  Then, `NavigationURLLoaderImpl` creates a new instance of `url_loader_` for
    restarting.
1.  Once the new `url_loader_` is created, it calls `URLLoaderFactory` to create
    the `URLLoader`, and `onConnected` is triggered again.
1.  Now, `onConnected` knows that the request header already included the hint
    values from the `ACCEPT_CH` frame.
1.  Since all Client Hints are already added to the request header, finally
    returns `net::OK`.

#### A Restart Request (Internal Redirect)

![accept_ch frame restart
request](/docs/client_hints/images/accept_ch_frame_restart_request.png)

Here are the major steps when visiting `https//a.com` with a rewrite rule to a
new location: `https://a.com?foo=1&bar=2` :

1.  When visiting `https//a.com`, `NavigationRequest` initializes the request
    and calls `NavigationURLLoaderImpl` to start the request.
1.  For the initial request, `ThrottlingURLLoader(url_loader_)` is null and
    `NavigationURLLoaderImpl` creates one when it calls `MaybeStartLoader`.
1.  A `URLLoaderThrottle` applies a configured rule in `WillStartRequest`,
    updating the URL from `https://a.com` to `https://a.com?foo=1&bar=2`.
1.  `ThrottlingURLLoader` sends an internal redirect request (307) because the
    `throttle_will_start_redirect_url_` (Set when a throttle changed the URL in
    `WillStartRequest`) is not null.
1.  `NavigationRequest` adds the Client Hints to the new request header after
    reading the Client Hints cache(both persistent and in-memory). At this
    point, no specific hints are cached, the new request header only includes
    the default enabled Client Hints. Any hints from the previous request are
    removed to prevent cross-origin leaks.
1.  Then, `NavigationURLLoaderImpl`follows the redirect chains to start the new
    request to `https://a.com?foo=1&bar=2`.
1.  Since `url_loader_` was already created, it follows the redirect chains to
    request as a normal URL.
1.  `url_loader_` calls `URLLoaderFactory` to create a new `URLLoader` (to
    perform a single request to a URL). `NetworkService` now can send the
    `ACCEPT_CH` frame message to the browser/render process once the message
    pipe is created in `URLLoader`.
1.  `URLLoader`'s `onConnected` checks whether the request header already
    includes all the Client Hints in the `ACCEPT_CH` frame. As the request
    header only added the default Client Hints, it will call
    `OnAcceptCHFrameReceived` and returns `net::ERR_IO_PENDING`.
1.  Once the `ACCEPT_CH` frame is received, `OnAcceptCHFrameReceived` adds the
    expected Client Hints to the requests, stores the Client Hints in memory and
    cleans them before restarting the request. It also reset the `url_loader_`.
1.  Once the request restarts, the `url_loader_` doesn't send another internal
    redirect because no rewrite URL applies again.
1.  `url_loader_` calls `URLLoaderFactory` to create another instance of
    `URLLoader` to make the request, and `onConnected` was triggered again.
1.  `onConnected` checks again if the request header includes all the Client
    Hints in the `ACCEPT_CH` frame.
1.  Since add all the `ACCEPT_CH` frame Client Hints in the previous call of
    `OnAcceptCHFrameReceived`, we returns `net::OK`. Request internal redirect
    form `https://a.com` to `https://a.com?foo=1&bar=2` completed.

**Notes:** In a redirect (step 5), the new request is initially populated with
only the default hints. The `ACCEPT_CH` frame hints are only added later (step
10) because they are stored temporarily in memory and cleared immediately after
use within `OnAcceptCHFrameReceived`. One of the reasons to clean the additional
client hints immediately is to avoid sending client hints to origins that don’t
request them, especially for cross origins.
