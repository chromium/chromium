# Threat Model And Defenses Against Compromised Renderers

Given the complexity of the browser, our threat model must use a "defense
in depth" approach to limit the damage that occurs if an attacker
finds a way around the Same Origin Policy or other security logic in the
renderer process.
For example, the combination of Chrome's sandbox, IPC security checks, and Site
Isolation limit what an untrustworthy renderer process can do.  They
protect Chrome users against attackers, even when such attackers are able to
bypass security logic in the renderer process.
For other arguments for the "defense in depth" approach and why our
threat model covers compromised renderers, please see
[the Site Isolation motivation](https://www.chromium.org/Home/chromium-security/site-isolation#TOC-Motivation).

In a compromised renderer, an attacker is able to execute
arbitrary native (i.e. non-JavaScript) code within the renderer
process's sandbox.  A compromised renderer can forge
malicious IPC messages, impersonate a Chrome Extension content script,
or use other techniques to trick more privileged parts of the browser.

The document below gives an overview of features that Chrome attempts to
protect against attacks from a compromised renderer.  Newly discovered
holes in this protection would be considered security bugs and possibly
eligible for the
[Chrome Vulnerability Rewards Program](https://www.google.com/about/appsecurity/chrome-rewards/).

[TOC]


## Site Isolation foundations

Most of the other protections listed in this document implicitly assume that
attacker-controlled execution contexts (e.g. HTML documents or service workers)
are hosted in a separate renderer process from other, victim contexts.
This separation is called
[Site Isolation](https://www.chromium.org/Home/chromium-security/site-isolation)
and allows the privileged browser
process to restrict what origins a renderer process is authorized to read or
control.

The privilege restriction can be implemented in various ways - see the
"protection techniques" listed in other sections in this document.
One example is validating in the browser process whether an incoming IPC can
legitimately claim authority over a given origin (e.g. by checking via
`CanAccessDataForOrigin` if the process lock matches).
Another example is making sure that capabilities handed over to renderer
processes are origin-bound (e.g. by setting `request_initiator_origin_lock`
on a `URLLoaderFactory` given to renderer processes).
Yet another example is making security decisions based on trustworthy knowledge,
calculated within the privileged browser process (e.g. using
`RenderFrameHost::GetLastCommittedOrigin()`).

Compromised renderers shouldn’t be able to commit an execution context
(e.g. commit a navigation to a HTML document, or create a service worker)
in a renderer process hosting other, cross-site execution contexts.
On desktop platforms all sites (site = scheme plus eTLD+1) should be isolated
from each other.
On Android, sites where the user entered a password should be isolated
from each other and from other sites.

**Known gaps in protection**:
- No form of Site Isolation is active in Android WebView.
  See also https://crbug.com/769449.
- Frames with `<iframe sandbox>` attribute are not isolated
  from their non-opaque precursor origin.
  See also https://crbug.com/510122.
- `file:` frames may share a process with other `file:` frames.
  See also https://crbug.com/780770.


## Cross-Origin HTTP resources

Compromised renderers shouldn't be able to read the contents (header + body) of
a cross-site HTTP response, unless it is a valid subresource needed for
compatibility (e.g., JavaScript, images, etc), or is successfully allowed via
CORS.

Protection techniques:
- Enforcing
  [Cross-Origin Read Blocking
  (CORB)](https://www.chromium.org/Home/chromium-security/corb-for-developers)
  in the NetworkService process
  (i.e. before the HTTP response is handed out to the renderer process).
- Only allowing the privileged browser process to create
  `network::mojom::URLLoaderFactory` objects that handle HTTP requests.
  This lets the browser process carefully control security-sensitive
  `network::mojom::URLLoaderFactoryParams` of such factories (such as
  `request_initiator_origin_lock`, `is_orb_enabled`, `disable_web_security` or
  `isolation_info`).

**Known gaps in protection**:
- Content types for which CORB does not apply
  (e.g. `image/png`, `application/octet-stream`) are not protected by
  default.  We recommend that HTTP servers protect such resources by
  either serving a `Cross-Origin-Resource-Policy: same-origin` response header
  or validating the `Sec-Fetch-Site` request header.


## Contents of cross-site frames

Compromised renderers shouldn't be able to read the contents of cross-site
frames.  Examples:
- Text or pixels of cross-site frames.
- Full URL (e.g. URL path or query) of cross-site frames.
  Note that the origin of other frames
  needs to be exposed via `window.origin` for legacy reasons.

Protection techniques:
- Compositing tab contents (both for display and for printing)
  outside the renderer processes.
- Isolating PDF plugins.
- Being careful what URLs are exposed in console messages.

**Known gaps in protection**:
- Mixed content console messages may disclose cross-site URLs
  (see also https://crbug.com/726178).


## Cookies

Compromised renderers shouldn’t be able to read or write
any cookies of another site,
or `httpOnly` cookies even from the same site.

Protection techniques:
- Renderer processes are only given `network::mojom::RestrictedCookieManager`
  for origins within their site
  (see `StoragePartitionImpl::CreateRestrictedCookieManager`).
- Mojo serialization does not send any cookies from HTTP headers to the renderer
  process (see
  `ParamTraits<scoped_refptr<net::HttpResponseHeaders>>::Write`).


## Passwords

Compromised renderers shouldn’t be able to read or write passwords of
other sites.

Protection techniques:
- Using `CanAccessDataForOrigin` to verify IPCs sent by a renderer process
  (e.g. `//components/password_manager/content/browser/bad_message.cc`)
- Using trustworthy, browser-side knowledge
  to determine which credentials to read or write
  (e.g. `content::RenderFrameHost::GetLastCommittedURL` in
  `password_manager::CredentialManagerImpl::GetOrigin`).


## Security-sensitive UI/chrome elements (e.g. Omnibox)

Compromised renderers shouldn’t be able to influence/spoof
security-sensitive UI elements.

Examples:
- Omnibox
    - URL (e.g. renderer process locked to foo.com shouldn’t
      be able to trick the Omnibox into displaying bar.com)
    - Secure / not secure chip (e.g. a renderer process locked to a HTTP
      site shouldn’t be able to trick the Omnibox into displaying a
      HTTPS-associated lock)
    - Content settings (e.g. a renderer process that has been granted
      microphone access shouldn’t be able to suppress the mic/camera
      icon in the Omnibox)
- Dialogs and prompts (for example a permissions dialog asking to allow
  a site to show notifications)
    - Origin in dialogs (e.g. a renderer process locked to foo.com
      shouldn’t be able to trick the Omnibox into displaying a bar.com
      URL in permission dialogs)

Protection techniques:
- `RenderFrameHostImpl::CanCommitOriginAndUrl` verifies that the renderer
  process is able to commit what it claims, and kills the process otherwise.
- Work-in-progress: calculating the origin in the browser process,
  before a navigation commits (https://crbug.com/888079).


## Permissions

Compromised renderers shouldn’t be able to gain permissions without user
consent.

Examples: microphone access permission, geolocation permission, etc.

Protection techniques:
- Requesting permissions based on browser-side knowledge of frame's origin
  (e.g. see `GeolocationServiceImplContext::RequestPermission`).


## Web storage

Compromised renderers shouldn’t be able to read from or write into
storage of another site.

Examples of protected storage technologies:
- localStorage
- sessionStorage
- indexedDB
- blob storage
- webSQL

Protection techniques:
- Using `CanAccessDataForOrigin` to verify IPCs sent by a renderer process
  (e.g. see `StoragePartitionImpl::OpenLocalStorage`).
- Binding Mojo interfaces to a single origin obtained from browser-side
  information in `RenderFrameHost::GetLastCommittedOrigin()`
  (e.g. see `RenderFrameHostImpl::CreateIDBFactory`).


## Messaging

Compromised renderers shouldn’t be able to:
- Spoof the `MessageEvent.origin` seen by a recipient of a `postMessage`.
- Bypass enforcement of the `targetOrigin` argument of `postMessage`.
- Send or receive `BroadcastChannel` messages for another origin.
- Spoof the `MessageSender.url`, nor `MessageSender.origin`, nor
  `MessageSender.id` (i.e. an extension id which can differ from the origin when
  the message is sent from a content script), as seen by a recipient of a
  `chrome.runtime.sendMessage`.
  See also [MessageSender documentation](https://developers.chrome.com/extensions/runtime#type-MessageSender) and [content script security guidance](https://groups.google.com/a/chromium.org/forum/#!topic/chromium-extensions/0ei-UCHNm34).
- Spoof the id of a Chrome extension initiating
  [native messaging](https://developer.chrome.com/docs/apps/nativeMessaging/)
  communication.

Protection techniques:
- Using `CanAccessDataForOrigin` to verify IPCs sent by a renderer process
  (e.g. in `RenderFrameProxyHost::OnRouteMessageEvent` or
  `BroadcastChannelProvider::ConnectToChannel`).
- Using `ContentScriptTracker` to check if IPCs from a given renderer process
  can legitimately claim to act on behalf content scripts of a given extension.


## JavaScript code cache

Compromised renderers shouldn't be able to poison the JavaScript code cache
used by scripts executed in cross-site execution contexts.

Protection techniques:
- Using trustworthy, browser-side origin lock while writing to and fetching from
  the code cache by using `ChildProcessSecurityPolicyImpl::GetOriginLock` in
  `GetSecondaryKeyForCodeCache` in
  `//content/browser/renderer_host/code_cache_host_impl.cc`


## Cross-Origin-Resource-Policy response header

A compromised renderer shouldn’t be able to bypass
[Cross-Origin-Resource-Policy (CORP)](https://developer.mozilla.org/en-US/docs/Web/HTTP/Cross-Origin_Resource_Policy_%28CORP%29),
which prevents or allows responses from being requested cross-origin, more
explicitly than CORB.

Protection techniques:
- Enforcing Cross-Origin-Resource-Policy in the NetworkService process
  (i.e. before the HTTP response is handed out to the renderer process).
- Preventing spoofing of `network::ResourceRequest::request_initiator`
  by comparing against `request_initiator_origin_lock` in
  `network::CorsURLLoaderFactory::IsValidRequest`.


## Frame-ancestors CSP and X-Frame-Options response headers

A compromised renderer shouldn’t be able to bypass `X-Frame-Options`
or `frame-ancestors` CSP.

For example, if example.com/page.html sends a `X-Frame-Options: deny` header,
then it should never commit in a subframe, even if some renderers have
been compromised.

Protection techniques:
- `X-Frame-Options: deny` is enforced in the browser process
  via `content::AncestorThrottle`, an implementation of
  `content::NavigationThrottle`.
- `frame-ancestors` is enforced in a renderer process, but
  this process is considered trustworthy in this scenario
  (because it hosts the frame that is requesting protection).
  See also https://crbug.com/759184 which tracks
  moving this enforcement into the browser process.


## HTTP request headers

Compromised renderers shouldn’t be able to control security sensitive HTTP
request headers like `Host`, `Origin`, or `Sec-Fetch-Site`.

Protection techniques:
- Using `AreRequestHeadersSafe` to reject `Host` and other headers that
  should only be generated internally within the NetworkService.
- Preventing spoofing of `network::ResourceRequest::request_initiator`
  by comparing against `request_initiator_origin_lock` in
  `network::CorsURLLoaderFactory::IsValidRequest`.


## (WIP) SameSite cookies

Compromised renderers shouldn’t be able to send a cross-site HTTP request with
SameSite cookies.

**Work-in-progress / not protected today**.

TODO(morlovich): Add details.  I assume that this requires trustworthy
|request_initiator| (similar to the `Origin` header), but probably more
than that.

See also https://crbug.com/927967.


## (WIP) User gestures / activations.

Compromised renderers shouldn't be able to spoof user gestures to perform
actions requiring them:

- A compromised renderer should not be able to forge a gesture that affects
  the trusted browser UI.  For example, a compromised renderer should not be
  able to interact with the Omnibox or the WebBluetooth chooser.

- A compromised renderer should not be able to forge a gesture that grants
  extra capabilities to a web origin.   For example, a compromised renderer
  should not be able to open an unlimited number of popup
  windows by forging user gestures.
  **Work-in-progress / not protected today** - see https://crbug.com/848778.


## Web Accessible Resources of Chrome Extensions

Compromised non-extension renderers shouldn’t be able to access
non-web-accessible-resources of a Chrome Extension.

Protection techniques:
- Navigations: Enforcement in the browser process
  via `extensions::ExtensionNavigationThrottle`, an implementation of
  `content::NavigationThrottle`.  This relies on non-spoofability
  of `content::NavigationHandle::GetInitiatorOrigin`.
- Subresources: Enforcement in the browser process via
  `ExtensionURLLoaderFactory::CreateLoaderAndStart`.  This relies
  on process boundaries and therefore doesn't rely on non-spoofability
  of `network::ResourceRequest::request_initiator`.


## Non-Web resources

Compromised *web* renderer processes shouldn’t be able to access
*local* resources (e.g. `file://...` or `chrome://settings`).

Protection techniques:
- TODO(lukasza, nasko): need to research


## Android-specific protection gaps

Due to resource constraints, on Android platforms only some sites get a
dedicated renderer process, isolated from other sites.
(Current heuristic is to isolate the sites where the user has entered a password
in the past.)
This means that some sites are hosted in a renderer process that is
*not* locked to any particular site.  If an attacker compromises
an unlocked renderer process, they may try to abuse protection gaps listed
below.

**Known gaps in protection**:
- When `CanAccessDataForOrigin` runs on the IO thread, it cannot protect
  isolated sites against being accessed from an unlocked renderer process.
  Some web storage protections depend on `CanAccessDataForOrigin` calls
  on the IO thread.
  See also https://crbug.com/764958.


## Renderer processes hosting DevTools frontend

If an attacker could take control over the DevTools frontend then the attacker
would gain access to all the cookies, storage, etc. of any origin within the
page and would be able to execute arbitrary scripts in any frame of the page.
This means that treating the DevTools renderer as untrustworthy wouldn't in
practice offer additional protection for the same-origin-policy.

Because of the above:

- Chrome ensures that the DevTools frontend is always hosted in a renderer
  process separate from renderers hosting web origins.
- Chrome assumes that the DevTools frontend is always trustworthy
  (i.e. never compromised, or under direct control of an attacker).
  For example, when the DevTools process asks to initiate a HTTP request on
  behalf of https://example.com, the browser process trusts the DevTools
  renderer to claim authority to initiate requests of behalf of this origin
  (e.g. attach SameSite cookies, send appropriate Sec-Fetch-Site request header,
  etc.).
