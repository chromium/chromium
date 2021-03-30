# Cookies

*** aside
_"In the beginning ~~the Universe was~~ cookies were created. This has
made a lot of people very angry and has been widely regarded as a bad move."_

_"Sometimes me think, what is friend? And then me say: a friend is someone to
share last cookie with."_
***

This directory is concerned with the management of cookies, as specified by
[RFC 6265bis](https://tools.ietf.org/html/draft-ietf-httpbis-rfc6265bis).
Cookies are implemented mostly in this directory, but also elsewhere, as
described [below](#Cookie-implementation-classes).

*** promo
* Those who wish to work with the implementation of cookies may refer to
  [Life of a cookie](#Life-of-a-cookie) and
  [Cookie implementation classes](#Cookie-implementation-classes).

* Those who wish to make use of cookies elsewhere in Chromium may refer to
  [Main interfaces for finding, setting, deleting, and observing cookies](#Main-interfaces-for-finding_setting_deleting_and-observing-cookies).
***

[TOC]

## Life of a cookie

This section describes the lifecycle of a typical/simple cookie on most
platforms, and serves as an overview of some important classes involved in
managing cookies.

This only covers cookie accesses via
[HTTP requests](https://developer.mozilla.org/en-US/docs/Web/HTTP/Cookies).
Other APIs for accessing cookies include JavaScript
([`document.cookie`](https://developer.mozilla.org/en-US/docs/Web/API/Document/cookie)
and [CookieStore API](https://wicg.github.io/cookie-store/)) or Chrome
extensions
([`chrome.cookies`](https://developer.chrome.com/extensions/cookies)).

### Cookie is received and parsed

*** note
**Summary:**

1. An HTTP response containing a `Set-Cookie` header is received.
2. The `Set-Cookie` header is processed by `URLRequestHttpJob`.
3. The header contents are parsed into a `CanonicalCookie` and passed to the
   `CookieStore` for storage.
***

A cookie starts as a `Set-Cookie` header sent in the server's response to an
HTTP request:

```
HTTP/1.1 200 OK
Date: ...
Server: ...
...
Set-Cookie: chocolate_chip=tasty; Secure; SameSite=Lax; Max-Age=3600
```

The response passes through the `HttpNetworkTransaction` and
`HttpCache::Transaction` to the `URLRequestHttpJob`. (See
[Life of a `URLRequest`](/net/docs/life-of-a-url-request.md#send-request-and-read-the-response-headers)
for more details.) The `URLRequestHttpJob` then reads any `Set-Cookie` headers
in the response (there may be multiple) and processes each `Set-Cookie` header
by calling into `//net/cookies` classes for parsing and storing:

First, the cookie, which has been provided as a string, is parsed into a
`ParsedCookie`. This struct simply records all the token-value pairs present in
the `Set-Cookie` header and keeps track of which cookie attribute each
corresponds to. The first token-value pair is always treated as the cookie's
name and value.

The `ParsedCookie` is then converted into a `CanonicalCookie`. This is the main
data type representing cookies. Any cookie consumer that does not deal directly
with HTTP headers operates on `CanonicalCookie`s. A `CanonicalCookie` has some
additional guarantees of validity over a `ParsedCookie`, such as valid
expiration times, valid domain and path attributes, etc. Once a
`CanonicalCookie` is created, you will almost never see a `ParsedCookie` used
for anything else.

If a valid `CanonicalCookie` could not be created (due to some illegal syntax,
inconsistent attribute values, or other circumstances preventing parsing), then
we stop here, and `URLRequestHttpJob` moves on to the next `Set-Cookie` header.

The `NetworkDelegate` also gets a chance to block the setting of the cookie,
based on the user's third-party cookie blocking settings. If it is blocked,
`URLRequestHttpJob` likewise moves on to the next `Set-Cookie` header.

If this did result in a valid and not-blocked `CanonicalCookie`, it is then
passed to the `CookieStore` to be stored.

### Cookie is stored

*** note
**Summary:**

1. The `CookieStore` receives the `CanonicalCookie` and validates some
   additional criteria before updating its in-memory cache of cookies.
2. The `CookieStore` may also update its on-disk backing store via the
   `CookieMonster::PersistentCookieStore` interface.
3. The result of the cookie storage attempt is reported back to the
   `URLRequestHttpJob`.
***

The `CookieStore` lives in the `URLRequestContext` and its main implementation,
used for most platforms, is `CookieMonster`. (The rest of this section assumes
that we are using a `CookieMonster`.) It exposes an asynchronous interface for
storing and retrieving cookies.

When `CookieMonster` receives a `CanonicalCookie` to be set, it queues a task to
validate and set the cookie. Most of the time this runs immediately, but it may
be delayed if the network service has just started up, and the contents of the
`PersistentCookieStore` are still being loaded from disk. It checks some
criteria against the cookie's source URL and a `CookieOptions` object (which
contains some other parameters describing the "context" in which the cookie is
being set, such as whether it's being accessed in a same-site or cross-site
context).

If everything checks out, the `CookieMonster` proceeds with setting the cookie.
If an equivalent cookie is present in the store, then it may be deleted.
Equivalent is defined as sharing a name, domain, and path, based on the
invariant specified by the RFC that no two such cookies may exist at a given
time. `CookieMonster` stores its `CanonicalCookie`s in a multimap keyed on the
registrable domain (eTLD+1) of the cookie's domain attribute.

The cookie may also be persisted to disk by a
`CookieMonster::PersistentCookieStore` depending on factors like whether the
cookie is a persistent cookie (has an expiration date), whether session cookies
should also be persisted (e.g. if the browser is set to restore the previous
browsing session), and whether the profile should have persistent storage (e.g.
yes for normal profiles, but not for Incognito profiles). The
`SQLitePersistentCookieStore` is the main implementation of
`CookieMonster::PersistentCookieStore`. It stores cookies in a SQLite database
on disk, at a filepath specified by the user's profile.

After the cookie has been stored (or rejected), the `CookieMonster` calls back
to the `URLRequestHttpJob` with the outcome of the storage attempt. The
`URLRequestHttpJob` stashes away the outcomes and stores them in the
`URLRequest` after all `Set-Cookie` headers in the response are processed, so
that interested parties (e.g.  DevTools) can subsequently be notified of cookie
activity.

### Cookie is retrieved and sent

*** note
**Summary:**

1. A network request reaches the net stack and generates a `URLRequestHttpJob`,
   which queries the `CookieStore` for cookies to include with the request.
2. The `CookieStore` evaluates each of its candidate cookies for whether it can
   be included in the request, and reports back to the `URLRequestHttpJob` with
   included and excluded cookies.
3. `URLRequestHttpJob` serializes the included cookies into a `Cookie` request
   header and attaches it to the outgoing request.
***

Some time later, a (credentialed) request to the same eTLD+1 triggers a cookie
access in order to retrieve the relevant cookies to include in the outgoing
`Cookie` request header. The request makes its way to the net stack and causes a
`URLRequestHttpJob` to be created. (See
[Life of a `URLRequest`](/net/docs/life-of-a-url-request.md#check-the-cache_request-an-httpstream)
for more details.) Upon being started, the `URLRequestHttpJob` asks the
`CookieStore` to retrieve the correct cookies for the URL being requested.

The `CookieMonster` queues a task to retrieve the cookies. Most of the time this
runs immediately, except if the contents of the `PersistentCookieStore` are
still being loaded from disk. The `CookieMonster` examines each of the cookies
it has stored for that URL's registrable domain and decides whether it should be
included or excluded for that request based on the requested URL and the
`CookieOptions`, by computing a `CookieInclusionStatus`. Criteria for inclusion
are described in
[RFC 6265bis](https://tools.ietf.org/html/draft-ietf-httpbis-rfc6265bis#section-5.5)
and include: the URL matching the cookie's `Domain` and `Path` attributes, the
URL being secure if the cookie has the `Secure` attribute, the request context
(i.e. `CookieOptions`) being same-site if the cookie is subject to `SameSite`
enforcement, etc. If any of the requirements are not met, a
`CookieInclusionStatus::ExclusionReason` is recorded.

After the exclusion reasons have been tallied up for each cookie, the cookies
without any exclusion reasons are deemed suitable for inclusion, and are
returned to the `URLRequestHttpJob`. The excluded cookies are also returned,
along with the `CookieInclusionStatus` describing why each cookie was excluded.

The included cookies are serialized into a `Cookie` header string (if the
`NetworkDelegate` is ok with it, based on the user's third-party cookie blocking
settings). The `URLRequestHttpJob` attaches this `Cookie` header to the outgoing
request headers:

```
GET /me/want/cookie/omnomnomnom HTTP/1.1
Host: ...
User-Agent: ...
Cookie: chocolate_chip=tasty
...
```

The included cookies, excluded cookies, and their corresponding
`CookieInclusionStatus`es are also stored in the `URLRequest` to notify
consumers of cookie activity notifications.

## Cookie implementation classes

This section lists classes involved in cookie management and access.

The core classes are highlighted.

### In this directory (//net/cookies)

*** note
* **[`CanonicalCookie`](/net/cookies/canonical_cookie.h)**

    The main data type representing cookies. Basically everything that's not
    directly dealing with HTTP headers or their equivalents operates on these.

    These are generally obtained via `CanonicalCookie::Create()`, which parses a
    string (a `Set-Cookie` header) into an intermediate `ParsedCookie`, whose
    fields it canonicalizes/validates and then copies into a `CanonicalCookie`.
***

*** note
* **[`CookieStore`](/net/cookies/cookie_store.h)**

    The main interface for a given platform's cookie handling. Provides
    asynchronous methods for setting and retrieving cookies.

    Its implementations are responsible for keeping track of all the cookies,
    finding cookies relevant for given HTTP requests, saving cookies received in
    HTTP responses, etc., and need to know quite a bit about cookie semantics.
***

*** note
* **[`CookieMonster`](/net/cookies/cookie_monster.h)**

    The implementation of `CookieStore` used on most platforms.

    It stores all cookies in a multimap keyed on the eTLD+1 of the cookie's
    domain. Also manages storage limits by evicting cookies when per-eTLD+1 or
    global cookie counts are exceeded.

    It can optionally take an implementation of
    `CookieMonster::PersistentCookieStore` to load and store cookies
    persisently.
***

* [`CookieOptions`](/net/cookies/cookie_options.h)

    Contains parameters for a given attempt to access cookies via
    `CookieStore`, such as whether the access is for an HTTP request (as opposed
    to a JavaScript API), and the same-site or cross-site context of the request
    (relevant to enforcement of the `SameSite` cookie attribute).

* [`SiteForCookies`](/net/cookies/site_for_cookies.h)

    Represents which origins should be considered "same-site" for a given
    context (e.g. frame). This is used to compute the same-site or cross-site
    context of a cookie access attempt (which is then conveyed to the
    `CookieStore` via a `CookieOptions`).

    It is generally the eTLD+1 and scheme of the top-level frame. It may also be
    empty, in which case it represents a context that is cross-site to
    everything (e.g. a nested iframe whose ancestor frames don't all belong to
    the same site).

* [`CookieInclusionStatus`](/net/cookies/cookie_inclusion_status.h)

    Records the outcome of a given attempt to access a cookie. Various reasons
    for cookie exclusion are recorded
    (`CookieInclusionStatus::ExclusionReason`), as well as informational
    statuses (`CookieInclusionStatus::WarningReason`) typically used to emit
    warnings in DevTools.

    May be used as a member of a `CookieAccessResult`, which includes even more
    metadata about the outcome of a cookie access attempt.

* [`CookieAccessDelegate`](/net/cookies/cookie_access_delegate.h)

    Interface for a `CookieStore` to query information from its embedder that
    may modify its decisions on cookie inclusion/exclusion. Its main
    implementation allows `CookieMonster` to access data from the network
    service layer (e.g. `CookieManager`).

* [`CookieChangeDispatcher`](/net/cookies/cookie_monster_change_dispatcher.h)

    Interface for subscribing to changes in the contents of the `CookieStore`.
    The main implementation is `CookieMonsterChangeDispatcher`.

### Elsewhere in //net

*** note
* **[`SQLitePersistentCookieStore`](/net/extras/sqlite/sqlite_persistent_cookie_store.h)**

    Implementation of `CookieMonster::PersistentCookieStore` used on most
    platforms. Uses a SQLite database to load and store cookies, potentially
    using an optional delegate to encrypt and decrypt their at-rest versions.
    This class is refcounted.

    `CookieMonster` loads cookies from here on startup. All other operations
    attempting to access cookies in the process of being loaded are blocked
    until loading of those cookies completes. Thus, it fast-tracks loading of
    cookies for an eTLD+1 with pending requests, to decrease latency for
    cookie access operations made soon after browser startup (by decreasing the
    number of cookies whose loading is blocking requests).
***

*** note
* **[`URLRequestHttpJob`](/net/url_request/url_request_http_job.h)**

    A `URLRequestJob` implementation that handles HTTP requests; most
    relevantly, the `Cookie` and `Set-Cookie` HTTP headers. It drives the
    process for storing cookies and retrieving cookies for HTTP requests.

    Also logs cookie events to the NetLog for each request.
***

* [`URLRequest`](/net/url_request/url_request.h)

    Mostly relevant for its two members, `maybe_sent_cookies_` and
    `maybe_stored_cookies_`, which are vectors in which `URLRequestHttpJob`
    stashes the cookies it considered sending/storing and their
    `CookieInclusionStatus`es. These then get mojo'ed over to the browser
    process to notify observers of cookie activity.

### In //services/network

*** note
* **[`CookieManager`](/services/network/cookie_manager.h)**

    The network service API to cookies. Basically exports a `CookieStore` via
    mojo IPC.

    Owned by the `NetworkContext`.
***

*** note
* **[`RestrictedCookieManager`](/services/network/restricted_cookie_manager.h)**

    Mojo interface for accessing cookies for a specific origin. This can be
    handed out to untrusted (i.e. renderer) processes, as inputs are assumed to
    be unsafe.

    It is primarily used for accessing cookies via JavaScript.
    It provides a synchronous interface for
    [`document.cookie`](https://developer.mozilla.org/en-US/docs/Web/API/Document/cookie),
    as well as an asynchronous one for the [CookieStore API](https://wicg.github.io/cookie-store/).
***

* [`CookieSettings`](/services/network/cookie_settings.h)

    Keeps track of the content settings (per-profile
    [permissions](/components/permissions/README.md) for types of
    content that a given origin is allowed to use) for cookies, such as the
    user's third-party cookie blocking settings, origins/domains with
    third-party cookie blocking exceptions or "legacy" access settings.

    It is not to be confused with `content_settings::CookieSettings`, which
    manages the browser's version of the cookie content settings, of which
    `network::ContentSettings` is approximately a copy/mirror. The
    `ProfileNetworkContextService` populates its contents upon `NetworkContext`
    construction from the browser-side content settings, and also updates it
    whenever the browser-side content settings change.

* [`SessionCleanupCookieStore`](/services/network/session_cleanup_cookie_store.h)

    Implements `CookieMonster::PersistentCookieStore`, by wrapping a
    `SQLitePersistentCookieStore`. Keeps an in-memory map of cookies per eTLD+1
    to allow deletion of cookies for sites whose cookie content setting is
    "session-only" from the persistent store when the session ends.


### Elsewhere

* [`CookieAccessObserver`](/services/network/public/mojom/cookie_access_observer.mojom)
  and [`WebContentsObserver`](/content/public/browser/web_contents_observer.h)

    `CookieAccessObserver` is a mojo interface used to observe attempts to
    access (read or write) cookies. It is implemented by `NavigationHandle` and
    `RenderFrameHost`.

    The cookie accesses are attributable to a committed document that called
    `document.cookie` or made a network request (if notified through
    `RenderFrameHost`), or a not-yet-committed navigation that resulted in a
    network request (if notified through `NavigationHandle`).

    The `CookieAccessObserver`s forward the notifications to `WebContents`,
    which then notifies its `WebContentsObserver`s. One such
    `WebContentsObserver` that cares about this information is
    `PageSpecificContentSettings`, which displays information about allowed and
    blocked cookies in UI surfaces (see next item).

* [`CookiesTreeModel`](/chrome/browser/browsing_data/cookies_tree_model.h)

    Stores cookie information for use in settings UI (the Page Info Bubble and
    various `chrome://settings` pages). Populated with info from
    `PageSpecificContentSettings`.

* [`CookieJar`](/third_party/blink/renderer/core/loader/cookie_jar.h)

    Implements the `document.cookie` API in the renderer by requesting a
    `RestrictedCookieManager` from the browser.

* [`CookieStore`](/third_party/blink/renderer/modules/cookie_store/cookie_store.h)

    Implements the JavaScript
    [CookieStore API](https://wicg.github.io/cookie-store/) in the renderer by
    requesting a `RestrictedCookieManager` from the browser. (Not to be confused
    with `net::CookieStore`.)

* [`CookiesAPI`](/chrome/browser/extensions/api/cookies/cookies_api.h)

    Implements the
    [`chrome.cookies`](https://developer.chrome.com/extensions/cookies) API for
    Chrome extensions. Gives extensions with the proper permissions essentially
    unfettered access to the `CookieStore`.

### Platform-specific

* [`CookieStoreIOS` and `CookieStoreIOSPersistent`](/ios/net/cookies)

    iOS-specific `CookieStore` implementations, mainly relying on the iOS native
    cookie implementation (`NSHTTPCookie`).

* [`android_webview::CookieManager`](/android_webview/browser/cookie_manager.h)

    Manages cookies for Android WebView. It typically wraps a
    `network::mojom::CookieManager`, but it can also be used before a
    `NetworkContext` even exists, thanks to Android WebView's
    [cookie API](https://developer.android.com/reference/kotlin/android/webkit/CookieManager),
    which means it is sometimes initialized with a bare `net::CookieStore`.

    Also notable for allowing cookies for `file://` scheme URLs (normally they
    are only allowed for HTTP and websocket schemes and `chrome-extension://`),
    though this is non-default and deprecated.

## Main interfaces for finding, setting, deleting, and observing cookies

This section summarizes interfaces for interacting with cookies from various
parts of the codebase.

### From //net or //services/network

*** note
Use [`net::CookieStore`](/net/cookies/cookie_store.h) to save and retrieve
[`CanonicalCookie`](/net/cookies/canonical_cookie.h)s.
***

* `CanonicalCookie`s are the main data type representing cookies. Get one using
  `CanonicalCookie::Create()`.

* The `CookieStore` can be accessed via its owning `URLRequestContext`, which
  can be accessed through `NetworkContext`.

* To access cookies, you need a `CookieOptions`. The main things in this object
  are the `HttpOnly` access permission and the `SameSite` context. The latter
  can be obtained from [`cookie_util`](/net/cookies/cookie_util.h) functions.

* Retrieve cookies using `GetCookieListWithOptionsAsync()`.

* Save cookies using `SetCanonicalCookieAsync()`.

*** note
Use `CookieStore` to selectively delete cookies.
***

* `DeleteCanonicalCookieAsync()` deletes a single cookie.

* `DeleteAllCreatedInTimeRangeAsync()` deletes cookies created in a time range.

* `DeleteAllMatchingInfoAsync()` deletes cookies that match a filter.

*** note
Use the [`CookieChangeDispatcher`](/net/cookies/cookie_change_dispatcher.h)
interface to subscribe to changes.
***

* Use `AddCallbackForCookie()` to observe changes to cookies with a given name
  that would be sent with a request to a specific URL.

* Use `AddCallbackForUrl()` to observe changes to all cookies that would be sent
  with a request to a specific URL.

* Use `AddCallbackForAllChanges()` to observe changes to all cookies in the
  `CookieStore`.

### From the browser process

*** note
Use [`CookieManager`](/services/network/cookie_manager.h)
(which basically exports a `net::CookieStore` interface via mojo) to save
and retrieve [`CanonicalCookie`](/net/cookies/canonical_cookie.h)s.
***

* The profile's `CookieManager` can be accessed from the browser process through
  `StoragePartition::GetCookieManagerForBrowserProcess()`.

* You can also get get a `CookieManager` pipe from the `NetworkContext` using
  `GetCookieManager()`.

* Retrieve cookies using `CookieManager::GetCookieList()`.

* Save cookies using `CookieManager::SetCanonicalCookie()`.

*** note
Use `CookieManager` to selectively delete cookies.
***

* If you have a copy of the `CanonicalCookie` to delete (e.g. a cookie
  previously fetched from the store), use
  `CookieManager::DeleteCanonicalCookie()`.

* To delete cookies with certain characteristics, construct a
  [`CookieDeletionFilter`](/services/network/public/mojom/cookie_manager.mojom)
  and use `CookieManager::DeleteCookies()`.

*** note
Use `CookieManager`'s change listener interface to subscribe to changes (this
parallels the `net::CookieChangeDispatcher` interface).
***

* Add a `CookieChangeListener` registration for a URL (and optionally a cookie
  name) via `AddCookieChangeListener()`

* Add a `CookieChangeListener` registration for all cookies via
  `AddGlobalChangeListener()`.

### From untrusted (e.g. renderer) processes

*** note
Use a
[`network::mojom::RestrictedCookieManager`](/services/network/public/mojom/restricted_cookie_manager.mojom)
interface to access cookies for a particular origin.
***

* Request a `RestrictedCookieManager` interface from the browser. This creates a
  `RestrictedCookieManager` bound to a `RenderFrameHost`, which can only access
  cookies on behalf of the corresponding origin.

* Cookies can be read and written asynchronously (`GetAllForUrl()`,
  `SetCanonicalCookie()`) or synchronously (`SetCookieFromString()`,
  `GetCookiesString()`).

* [Compromised renderers](/docs/security/compromised-renderers.md#Cookies)
  shouldn't be able to access cookies of another site, or `HttpOnly` cookies
  (even from the same site).
