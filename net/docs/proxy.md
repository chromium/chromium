# Proxy support in Chrome

This document establishes basic proxy terminology and describes Chrome-specific
proxy behaviors.

[TOC]

## Proxy server identifiers

A proxy server is an intermediary used for network requests. A proxy server can
be described by its address, along with the proxy scheme that should be used to
communicate with it.

This can be written as a string using either the "PAC format" or the "URI
format".

The PAC format is how one names a proxy server in [Proxy
auto-config](https://en.wikipedia.org/wiki/Proxy_auto-config) scripts. For
example:
* `PROXY foo:2138`
* `SOCKS5 foo:1080`
* `DIRECT`

The "URI format" instead encodes the information as a URL. For example:
* `foo:2138`
* `http://foo:2138`
* `socks5://foo:1080`
* `direct://`

The port number is optional in both formats. When omitted, a per-scheme default
is used.

See the [Proxy server schemes](#Proxy-server-schemes) section for details on
what schemes Chrome supports, and how to write them in the PAC and URI formats.

Most UI surfaces in Chrome (including command lines and policy) expect URI
formatted proxy server identifiers. However outside of Chrome, proxy servers
are generally identified less precisely by just an address -- the proxy
scheme is assumed based on context.

In Windows' proxy settings there are host and port fields for the
"HTTP", "Secure", "FTP", and "SOCKS" proxy. With the exception of "SOCKS",
those are all identifiers for insecure HTTP proxy servers (proxy scheme is
assumed as HTTP).

## Proxy resolution

Proxying in Chrome is done at the URL level.

When the browser is asked to fetch a URL, it needs to decide which IP endpoint
to send the request to. This can be either a proxy server, or the target host.

This is called proxy resolution. The input to proxy resolution is a URL, and
the output is an ordered list of [proxy server
identifiers](#Proxy-server-identifiers).

What proxies to use can be described using either:

* [Manual proxy settings](#Manual-proxy-settings) - proxy resolution is defined
  using a declarative set of rules. These rules are expressed as a mapping from
  URL scheme to proxy server identifier(s), and a list of proxy bypass rules for
  when to go DIRECT instead of using the mapped proxy.

* PAC script - proxy resolution is defined using a JavaScript program, that is
  invoked whenever fetching a URL to get the list of proxy server identifiers
  to use.

* Auto-detect - the WPAD protocol is used to probe the network (using DHCP/DNS)
  and possibly discover the URL of a PAC script.

## Proxy server schemes

When using an explicit proxy in the browser, multiple layers of the network
request are impacted, depending on the scheme that is used. Some implications
of the proxy scheme are:

* Is communication to the proxy done over a secure channel?
* Is name resolution (ex: DNS) done client side, or proxy side?
* What authentication schemes to the proxy server are supported?
* What network traffic can be sent through the proxy?

Chrome supports these proxy server schemes:

* [DIRECT](#DIRECT-proxy-scheme)
* [HTTP](#HTTP-proxy-scheme)
* [HTTPS](#HTTPS-proxy-scheme)
* [SOCKSv4](#SOCKSv4-proxy-scheme)
* [SOCKSv5](#SOCKSv5-proxy-scheme)
* [QUIC](#QUIC-proxy-scheme)

### DIRECT proxy scheme

* Default port: N/A (neither host nor port are applicable)
* Example identifier (PAC): `DIRECT`
* Example identifier (URI): `direct://`

This is a pseudo proxy scheme that indicates instead of using a proxy we are
sending the request directly to the target server.

It is imprecise to call this a "proxy server", but it is a convenient abstraction.

### HTTP proxy scheme

* Default port: 80
* Example identifier (PAC): `PROXY proxy:8080`, `proxy` (non-standard; don't use)
* Example identifiers (URI): `http://proxy:8080`, `proxy:8080` (can omit scheme)

Generally when one refers to a "proxy server" or "web proxy", they are talking
about an HTTP proxy.

When using an HTTP proxy in Chrome, name resolution is always deferred to the
proxy. HTTP proxies can proxy `http://`, `https://`, `ws://` and `wss://` URLs.
(Chrome's FTP support is deprecated, and HTTP proxies cannot proxy `ftp://` anymore)

Communication to HTTP proxy servers is insecure, meaning proxied `http://`
requests are sent in the clear. When proxying `https://` requests through an
HTTP proxy, the TLS exchange is forwarded through the proxy using the `CONNECT`
method, so end-to-end encryption is not broken. However when establishing the
tunnel, the hostname of the target URL is sent to the proxy server in the
clear.

HTTP proxies in Chrome support the same HTTP authentiation schemes as for
target servers: Basic, Digest, Negotiate, NTLM.

### HTTPS proxy scheme

* Default port: 443
* Example identifier (PAC): `HTTPS proxy:8080`
* Example identifier (URI): `https://proxy:8080`

This works like an [HTTP proxy](#HTTP-proxy-scheme), except the
communication to the proxy server is protected by TLS, and may negotiate
HTTP/2 (but not QUIC).

Because the connection to the proxy server is secure, https:// requests
sent through the proxy are not sent in the clear as with an HTTP proxy.
Similarly, since CONNECT requests are sent over a protected channel, the
hostnames for proxied https:// URLs is also not revealed.

In addition to the usual HTTP authentication methods, HTTPS proxies also
support client certificates.

HTTPS proxies using HTTP/2 can offer better performance in Chrome than a
regular HTTP proxy due to higher connection limits (HTTP/1.1 proxies in Chrome
are limited to 32 simultaneous connections across all domains).

Chrome, Firefox, and Opera support HTTPS proxies; however, most older HTTP
stacks do not.

Specifying an HTTPS proxy is generally not possible through system proxy
settings. Instead, one must use either a PAC script or a Chrome proxy setting
(command line, extension, or policy).

See the dev.chromium.org document on [secure web
proxies](http://dev.chromium.org/developers/design-documents/secure-web-proxy)
for tips on how to run and test against an HTTPS proxy.

### SOCKSv4 proxy scheme

* Default port: 1080
* Example identifiers (PAC): `SOCKS4 proxy:8080`, `SOCKS proxy:8080`
* Example identifier (URI): `socks4://proxy:8080`

SOCKSv4 is a simple transport layer proxy that wraps a TCP socket. Its use
is transparent to the rest of the protocol stack; after an initial
handshake when connecting the TCP socket (to the proxy), the rest of the
loading stack is unchanged.

No proxy authentication methods are supported for SOCKSv4.

When using a SOCKSv4 proxy, name resolution for target hosts is always done
client side, and moreover must resolve to an IPv4 address (SOCKSv4 encodes
target address as 4 octets, so IPv6 targets are not possible).

There are extensions to SOCKSv4 that allow for proxy side name resolution, and
IPv6, namely SOCKSv4a. However Chrome does not allow configuring, or falling
back to v4a.

A better alternative is to just use the newer version of the protocol, SOCKSv5
(which is still 20+ years old).

### SOCKSv5 proxy scheme

* Default port: 1080
* Example identifier (PAC): `SOCKS5 proxy:8080`
* Example identifiers (URI): `socks://proxy:8080`, `socks5://proxy:8080`

[SOCKSv5](https://tools.ietf.org/html/rfc1928) is a transport layer proxy that
wraps a TCP socket, and allows for name resolution to be deferred to the proxy.

In Chrome when a proxy's scheme is set to SOCKSv5, name resolution is always
done proxy side (even though the protocol allows for client side as well). In
Firefox client side vs proxy side name resolution can be configured with
`network.proxy.socks_remote_dns`; Chrome has no equivalent option and will
always use proxy side resolution.

No authentication methods are supported for SOCKSv5 in Chrome (although some do
exist for the protocol).

A handy way to create a SOCKSv5 proxy is with `ssh -D`, which can be used to
tunnel web traffic to a remote host over SSH.

In Chrome SOCKSv5 is only used to proxy TCP-based URL requests. It cannot be
used to relay UDP traffic.

### QUIC proxy scheme

* Default (UDP) port: 443
* Example identifier (PAC): `QUIC proxy:8080`
* Example identifier (URI): `quic://proxy:8080`

A QUIC proxy uses QUIC (UDP) as the underlying transport, but otherwise
behaves as an HTTP proxy. It has similar properties to an [HTTPS
proxy](#HTTPS-proxy-scheme), in that the connection to the proxy server
is secure, and connection limits are less restrictive.

Support for QUIC proxies in Chrome is currently experimental and not
ready for production use. In particular, sending https:// and wss://
URLs through a QUIC proxy is [disabled by
default](https://bugs.chromium.org/p/chromium/issues/detail?id=969859).

Another caveat is that QUIC does not currently support
client certificates since it does not use a TLS
handshake. This may change in future versions.

## Manual proxy settings

The simplest way to configure proxy resolution is by providing a static list of
rules comprised of:

1. A mapping of URL schemes to [proxy server identifiers](#Proxy-server-identifiers).
2. A list of [proxy bypass rules](#Proxy-bypass-rules)

We refer to this mode of configuration as "manual proxy settings".

Manual proxy settings can succinctly describe setups like:

* Use proxy `http://foo:8080` for all requests
* Use proxy `http://foo:8080` for all requests except those to a `google.com`
  subdomain.
* Use proxy `http://foo:8080` for all `https://` requests, and proxy
  `socsk5://mysocks:90` for everything else

Although manual proxy settings are a ubiquituous way to configure proxies
across platforms, there is no standard representation or feature set.

Chrome's manual proxy settings most closely resembles that of WinInet. But it
also supports idioms from other platforms -- for instance KDE's notion of
reversing the bypass list, or Gnome's interpretation of bypass patterns as
suffix matches.

When defining manual proxy settings in Chrome, we specify three (possibly
empty) lists of [proxy server identifiers](#Proxy-server-identifiers).

  * proxies for HTTP - A list of proxy server identifiers to use for `http://`
    requests, if non-empty.
  * proxies for HTTPS - A list of proxy server identifiers to use for
    `https://` requests, if non-empty.
  * other proxies - A list of proxy server identifiers to use for everything
    else (whatever isn't matched by the other two lists)

There are a lot of ways to end up with manual proxy settings in Chrome
(discussed in other sections).

The following examples will use the command line method. Launching Chrome with
`--proxy-server=XXX` (and optionally `--proxy-bypass-list=YYY`)

Example: To use proxy `http://foo:8080` for all requests we can launch
Chrome with `--proxy-server="http://foo:8080"`. This translates to:

  * proxies for HTTP - *empty*
  * proxies for HTTPS - *empty*
  * other proxies - `http://foo:8080`

With the above configuration, if the proxy server was unreachable all requests
would fail with `ERR_PROXY_CONNECTION_FAILED`. To address this we could add a
fallback to `DIRECT` by launching using
`--proxy-server="http://foo:8080,direct://"` (note the comma separated list).
This command line means:

  * proxies for HTTP - *empty*
  * proxies for HTTPS - *empty*
  * other proxies - `http://foo:8080`, `direct://`

If instead we wanted to proxy only `http://` URLs through the
HTTPS proxy `https://foo:443`, and have everything else use the SOCKSv5 proxy
`socks5://mysocks:1080` we could launch Chrome with
`--proxy-server="http=https://foo:443;socks=socks5://mysocks:1080"`. This now
expands to:

  * proxies for HTTP - `https://foo:443`
  * proxies for HTTPS - *empty*
  * other proxies - `socks5://mysocks:1080`

The command line above uses WinInet's proxy map format, with some additional
features:

* Instead of naming proxy servers by just a hostname:port, you can use Chrome's
  URI format for proxy server identifiers. In other words, you can prefix the
  proxy scheme so it doesn't default to HTTP.
* The `socks=` mapping is understood more broadly as "other proxies". The
  subsequent proxy list can include proxies of any scheme, however if the
  scheme is omitted it will be understood as SOCKSv4 rather than HTTP.

### Mapping WebSockets URLs to a proxy

[Manual proxy settings](#Manual-proxy-settings) don't have mappings for `ws://`
or `wss://` URLs.

Selecting a proxy for these URL schemes is a bit different from other URL
schemes. The algorithm that Chrome uses is:

* If "other proxies" is non-empty use it
* If "proxies for HTTPS" is non-empty use it
* Otherwise use "proxies for HTTP"

This is per the recommendation in section 4.1.3 of [RFC
6455](https://tools.ietf.org/html/rfc6455).

It is possible to route `ws://` and `wss://` separately using a PAC script.

### Proxy credentials in manual proxy settings

Most platforms' [manual proxy settings](#Manual-proxy-settings) allow
specifying a cleartext username/password for proxy sign in. Chrome does not
implement this, and will not use any credentials embedded in the proxy
settings.

Proxy authentication will instead go through the ordinary flow to find
credentials.

## Proxy bypass rules

In addition to specifying three lists of [proxy server
identifiers](#proxy-server-identifiers), Chrome's [manual proxy
settings](#Manual-proxy-settings) lets you specify a list of "proxy bypass
rules".

This ruleset determines whether a given URL should skip use of a proxy all
together, even when a proxy is otherwise defined for it.

This concept is also known by names like "exception list", "exclusion list" or
"no proxy list".

Proxy bypass rules can be written as an ordered list of strings. Ordering
generally doesn't matter, but may when using subtractive rules.

When manual proxy settings are specified from the command line, the
`--proxy-bypass-list="RULES"` switch can be used, where `RULES` is a semicolon
or comma separated list of bypass rules.

Following are the string constructions for the bypass rules that Chrome
supports. They can be used when defining a Chrome manual proxy settings from
command line flags, extensions, or policy.

When using system proxy settings, one should use the platform's rule format and
not Chrome's.

### Bypass rule: Hostname

```
[ URL_SCHEME "://" ] HOSTNAME_PATTERN [ ":" <port> ]
```

Matches a hostname using a wildcard pattern, and an optional scheme and port
restriction.

Examples:

* `foobar.com` - Matches URL of any scheme and port, whose normalized host is
  `foobar.com`
* `*foobar.com` - Matches URL of any scheme and port, whose normalized host
  ends with `foobar.com` (for instance `blahfoobar.com` and `foo.foobar.com`).
* `*.org:443` - Matches URLs of any scheme, using port 443 and whose top level
  domain is `.org`
* `https://x.*.y.com:99` - Matches https:// URLs on port 99 whose normalized
  hostname matches `x.*.y.com`

### Bypass rule: Subdomain

```
[ URL_SCHEME "://" ] "." HOSTNAME_SUFFIX_PATTERN [ ":" PORT ]
```

Hostname patterns that start with a dot are special cased to mean a subdomain
matches. `.foo.com` is effectively another way of writing `*.foo.com`.

Examples:

* `.google.com` - Matches `calendar.google.com` and `foo.bar.google.com`, but
  not `google.com`.
* `http://.google.com` - Matches only http:// URLs that are a subdomain of `google.com`.

### Bypass rule: IP literal

```
[ SCHEME "://" ] IP_LITERAL [ ":" PORT ]
```

Matches URLs that are IP address literals, and optional scheme and port
restrictions. This is a special case of hostname matching that takes into
account IP literal canonicalization. For example the rules `[0:0:0::1]` and
`[::1]` are equivalent (both represent the same IPv6 address).

Examples:

* `127.0.0.1`
* `http://127.0.0.1`
* `[::1]` - Matches any URL to the IPv6 loopback address.
* `[0:0::1]` - Same as above
* `http://[::1]:99` - Matches any http:// URL to the IPv6 loopback on port 99

### Bypass rule: IPv4 address range

```
IPV4_LITERAL "/" PREFIX_LENGTH_IN_BITS
```

Matches any URL whose hostname is an IPv4 literal, and falls between the given
address range.

Note this [only applies to URLs that are IP
literals](#Meaning-of-IP-address-range-bypass-rules).

Examples:

* `192.168.1.1/16`

### Bypass rule: IPv6 address range

```
IPV6_LITERAL "/" PREFIX_LENGTH_IN_BITS
```

Matches any URL that is an IPv6 literal that falls between the given range.
Note that IPv6 literals must *not* be bracketed.

Note this [only applies to URLs that are IP
literals](#Meaning-of-IP-address-range-bypass-rules).

Examples:

* `fefe:13::abc/33`
* `[fefe::]/40` -- WRONG! IPv6 literals must not be bracketed.

### Bypass rule: Simple hostnames

```
<local>
```

Matches hostnames without a period in them, and that are not IP literals. This
is a naive string search -- meaning that periods appearing *anywhere* count
(including trailing dots!).

This rule corresponds to the "Exclude simple hostnames" checkbox on macOS and
the "Don't use proxy server for local (intranet) addresses" on Windows.

The rule name comes from WinInet, and can easily be confused with the concept
of localhost. However the two concepts are completely orthogonal. In practice
one wouldn't add rules to bypass localhost, as it is [already done
implicitly](#Implicit-bypass-rules).

### Bypass rule: Subtract implicit rules

```
<-loopback>
```

*Subtracts* the [implicit proxy bypass rules](#Implicit-bypass-rules)
(localhost and link local addresses). This is generally only needed for test
setupe. Beware of the security implications to proxying localhost.

Whereas regular bypass rules instruct the browser about URLs that should *not*
use the proxy, this rule has the opposite effect and tells the browser to
instead *use* the proxy.

Ordering may matter when using a subtractive rule, as rules will be evaluated
in a left-to-right order. `<-loopback>;127.0.0.1` has a subtly different effect
than `127.0.0.1;<-loopback>`.

### Meaning of IP address range bypass rules

The IP address range bypass rules in manual proxy settings applies only to URL
literals. This is not what one would intuitively expect.

Example:

Say we have have configured a proxy for all requests, but added a bypass rule
for `192.168.0.0.1/16`. If we now navigate to `http://foo` (which resolves
to `192.168.1.5` in our setup) will the browser connect directly (bypass proxy)
because we have indicated a bypass rule that includes this IP?

It will go through the proxy.

The bypass rule in this case is not applicable, since the browser never
actually does a name resolution for `foo`. Proxy resolution happens before
name resolution, and depending on what proxy scheme is subsequently chosen,
client side name resolution may never be performed.

The usefulness of IP range proxy bypass rules is rather limited, as they only
apply to requests whose URL was explicitly an IP literal.

If proxy decisions need to be made based on the resolved IP address(es) of a
URL's hostname, one must use a PAC script.

## Implicit bypass rules

Requests to certain hosts will not be sent through a proxy, and will instead be
sent directly.

We call these the _implicit bypass rules_. The implicit bypass rules match URLs
whose host portion is either a localhost name or a link-local IP literal.
Essentially it matches:

```
localhost
*.localhost
[::1]
127.0.0.1/8
169.254/16
[FE80::]/10
```

The complete rules are slightly more complicated. For instance on
Windows we will also recognize `loopback`, and there is special casing of
`localhost6` and `localhost6.localdomain6` in Chrome's localhost matching.

This concept of implicit proxy bypass rules is consistent with the
platform-level proxy support on Windows and macOS (albeit with some differences
due to their implementation quirks - see compatibility notes in
`net::ProxyBypassRules::MatchesImplicitRules`)

Why apply implicit proxy bypass rules in the first place? Certainly there are
considerations around ergonomics and user expectation, but the bigger problem
is security. Since the web platform treats `localhost` as a secure origin, the
ability to proxy it grants extra powers. This is [especially
problematic](https://bugs.chromium.org/p/chromium/issues/detail?id=899126) when
proxy settings are externally controllable, as when using PAC scripts.

Historical support in Chrome:

* Prior to M71 there were no implicit proxy bypass rules (except if using
  `--winhttp-proxy-resolver`)
* In M71 Chrome applied implicit proxy bypass rules to PAC scripts
* In M72 Chrome generalized the implicit proxy bypass rules to manually
  configured proxies

### Overriding the implicit bypass rules

If you want traffic to `localhost` to be sent through a proxy despite the
security concerns, it can be done by adding the special proxy bypass rule
`<-loopback>`. This has the effect of _subtracting_ the implicit rules.

For instance, launch Chrome with the command line flag:

```
--proxy-bypass-list="<-loopback>"
```

Note that there currently is no mechanism to disable the implicit proxy bypass
rules when using a PAC script. Proxy bypass lists only apply to manual
settings, so the technique above cannot be used to let PAC scripts decide the
proxy for localhost URLs.

## Evaluating proxy lists (proxy fallback)

Proxy resolution results in a _list_ of [proxy server
identifiers](#Proxy-server-identifiers) to use for a
given request, not just a single proxy server identifier.

For instance, consider this PAC script:

```
function FindProxyForURL(url, host) {
    if (host == "www.example.com") {
        return "PROXY proxy1; HTTPS proxy2; SOCKS5 proxy3";
    }
    return "DIRECT";
}

```

What proxy will Chrome use for connections to `www.example.com`, given that
we have a choice of three separate proxy server identifiers to choose from
{`http://proxy1:80`, `https://proxy2:443`, `socks5://proxy3:1080`}?

Initially, Chrome will try the proxies in order. This means first attempting
the request through `http://proxy1:80`. If that "fails", the request is
next attempted through `https://proxy2:443`. Lastly if that fails, the
request is attempted through `socks5://proxy3:1080`.

This process is referred to as _proxy fallback_. What constitutes a
"failure" is described later.

Proxy fallback is stateful. The actual order of proxy attempts made be Chrome
is influenced by the past responsiveness of proxy servers.

Let's say we request `http://www.example.com/`. Per the PAC script this
resolves to a list of three proxy server identifiers:

{`http://proxy1:80`, `https://proxy2:443`, `socks5://proxy3:1080`}

Chrome will first attempt to issue the request through these proxies in the
left-to-right order.

Let's say that the attempt through `http://proxy1:80` fails, but then the
attempt through `https://proxy2:443` succeeds. Chrome will mark
`http://proxy1:80` as _bad_ for the next 5 minutes. Being marked as _bad_
means that `http://proxy1:80` is de-prioritized with respect to
other proxy server identifiers (including `direct://`) that are not marked as
bad.

That means the next time `http://www.example.com/` is requested, the effective
order for proxies to attempt will be:

{`https://proxy2:443`, `socks5://proxy3:1080`, `http://proxy1:80`}

Conceptually, _bad_ proxies are moved to the end of the list, rather than being
removed from consideration all together.

What constitutes a "failure" when it comes to triggering proxy fallback depends
on the proxy type. Generally speaking, only connection level failures
are deemed eligible for proxy fallback. This includes:

* Failure resolving the proxy server's DNS
* Failure connecting a TCP socket to the proxy server

(There are some caveats for how HTTPS and QUIC proxies count failures for
fallback)

Prior to M67, Chrome would consider failures establishing a
CONNECT tunnel as an error eligible for proxy fallback. This policy [resulted
in problems](https://bugs.chromium.org/p/chromium/issues/detail?id=680837) for
deployments whose HTTP proxies intentionally failed certain https:// requests,
since that necessitates inducing a failure during the CONNECT tunnel
establishment. The problem would occur when a working proxy fallback option
like DIRECT was given, since the failing proxy would then be marked as bad.

Currently there are no options to configure proxy fallback (including disabling
the caching of bad proxies). Future versions of Chrome may [remove caching
of bad proxies](https://bugs.chromium.org/p/chromium/issues/detail?id=936130)
to make fallback predictable.

To investigate issues relating to proxy fallback, one can [collect a NetLog
dump using
chrome://net-export/](https://dev.chromium.org/for-testers/providing-network-details).
These logs can then be loaded with the [NetLog
viewer](https://netlog-viewer.appspot.com/).

There are a few things of interest in the logs:

* The "Proxy" tab will show which proxies (if any) were marked as bad at the
  time the capture ended.
* The "Events" tab notes what the resolved proxy list was, and what the
  re-ordered proxy list was after taking into account bad proxies.
* The "Events" tab notes when a proxy is marked as bad and why (provided the
  event occurred while capturing was enabled).

When debugging issues with bad proxies, it is also useful to reset Chrome's
cache of bad proxies. This can be done by clicking the "Clear bad proxies"
button on
[chrome://net-internals/#proxy](chrome://net-internals/#proxy). Note the UI
will not give feedback that the bad proxies were cleared, however capturing a
new NetLog dump can confirm it was cleared.

## Arguments passed to FindProxyForURL() in PAC scripts

PAC scripts in Chrome are expected to define a JavaScript function
`FindProxyForURL`.

The historical signature for this function is:

```
function FindProxyForURL(url, host) {
  ...
}
```

Scripts can expect to be called with string arguments `url` and `host` such
that:

* `url` is a *sanitized* version of the request's URL
* `host` is the unbracketed host portion of the origin.

Sanitization of the URL means that the path, query, fragment, and identity
portions of the URL are stripped. Effectively `url` will be
limited to a `scheme://host:port/` style URL

Examples of how `FindProxyForURL()` will be called:

```
// Actual URL:   https://www.google.com/Foo
FindProxyForURL('https://www.google.com/', 'www.google.com')

// Actual URL:   https://[dead::beef]/foo?bar
FindProxyForURL('https://[dead::beef]/', 'dead::beef')

// Actual URL:   https://www.example.com:8080#search
FindProxyForURL('https://www.example.com:8080/', 'example.com')

// Actual URL:   https://username:password@www.example.com
FindProxyForURL('https://www.example.com/', 'example.com')
```

Stripping the path and query from the `url` is a departure from the original
Netscape implementation of PAC. It was introduced in Chrome 52 for [security
reasons](https://bugs.chromium.org/p/chromium/issues/detail?id=593759).

There is currently no option to turn off sanitization of URLs passed to PAC
scripts (removed in Chrome 75).

The sanitization of http:// URLs currently has a different policy, and does not
strip query and path portions of the URL. That said, users are advised not to
depend on reading the query/path portion of any URL
type, since future versions of Chrome may [deprecate that
capability](https://bugs.chromium.org/p/chromium/issues/detail?id=882536) in
favor of a consistent policy.

## Resolving client's IP address within a PAC script using myIpAddress()

PAC scripts can invoke `myIpAddress()` to obtain the client's IP address. This
function returns a single IP literal, or `"127.0.0.1"` on failure.

`myIpAddress()` is fundamentally broken for multi-homed hosts.

Consider what happens when a machine has multiple network interfaces, each with
its own IP address. Answering "what is my IP address" depends on what interface
the request is sent out on. Which in turn depends on what the destination IP
is. Which in turn depends on the result of proxy resolution + fallback, which
is what we are currently blocked in!

Chrome's algorithm uses these ordered steps to find an IP address
(short-circuiting when a candidate is found).

1. Select the IP of an interface that can route to public Internet:
    * Probe for route to `8.8.8.8`.
    * Probe for route to `2001:4860:4860::8888`.
2. Select an IP by doing a DNS resolve of the machine's hostname:
    * Select the first IPv4 result if there is one.
    * Select the first IP result if there is one.
3. Select the IP of an interface that can route to private IP space:
    * Probe for route to `10.0.0.0`.
    * Probe for route to `172.16.0.0`.
    * Probe for route to `192.168.0.0`.
    * Probe for route to `FC00::`.

When searching for candidate IP addresses, link-local and loopback addresses
are skipped over. Link-local or loopback address will only be returned as a
last resort when no other IP address was found by following these steps.

This sequence of steps explicitly favors IPv4 over IPv6 results.

*Historical note*: Prior to M72, Chrome's implementation of `myIpAddress()` was
effectively just `getaddrinfo(gethostname)`. This is now step 2 of the heuristic.

### What about pacUseMultihomedDNS?

In Firefox, if you define a global variable named `pacUseMultihomedDNS` in your
PAC script, it causes `myIpAddress()` to report the IP address of the interface
that would (likely) have been used had we connected to it DIRECT.

In particular, it will do a DNS resolution of the target host (the hostname of
the URL that the proxy resolution is being done for), and then
connect a datagram socket to get the source address.

Chrome does not recognize the `pacUseMultihomedDNS` global as having special
meaning. A PAC script is free to define such a global, and it won't have
side-effects. Chrome has no APIs or settings to change `myIpAddress()`'s
algorithm.

## Resolving client's IP address within a PAC script using myIpAddressEx()

Chrome supports the [Microsoft PAC
extension](https://docs.microsoft.com/en-us/windows/desktop/winhttp/myipaddressex)
`myIpAddressEx()`.

This is like `myIpAddress()`, but instead of returning a single IP address, it
can return multiple IP addresses. It returns a string containing a semi-colon
separated list of addresses. On failure it returns an empty string to indicate
no results (whereas `myIpAddress()` returns `127.0.0.1`).

There are some differences with Chrome's implementation:

* In Chrome the function is unconditionally defined, whereas in Internet
  Explorer one must have used the `FindProxyForURLEx` entrypoint.
* Chrome does not enumerate all of the host's network interfaces
* Chrome does not return link-local or loopback addresses (except if no other
  addresses were found).

The algorithm that Chrome uses is nearly identical to that of `myIpAddress()`
described earlier. The main difference is that we don't short-circuit
after finding the first candidate IP, so multiple IPs may be returned.

1. Select all the IPs of interfaces that can route to public Internet:
    * Probe for route to `8.8.8.8`.
    * Probe for route to `2001:4860:4860::8888`.
    * If any IPs were found, return them, and finish.
2. Select an IP by doing a DNS resolve of the machine's hostname:
    * If any IPs were found, return them, and finish.
3. Select the IP of an interface that can route to private IP space:
    * Probe for route to `10.0.0.0`.
    * Probe for route to `172.16.0.0`.
    * Probe for route to `192.168.0.0`.
    * Probe for route to `FC00::`.
    * If any IPs were found, return them, and finish.

Note that short-circuiting happens whenever steps 1-3 find a candidate IP. So
for example if at least one IP address was discovered by checking routes to
public Internet, only those IPs will be returned, and steps 2-3 will not run.

## Android quirks

Proxy resolving via PAC works differently on Android than other desktop Chrome
platforms:

* Android Chrome uses the same Chromium PAC resolver, however does not run it
  out-of-process as on Desktop Chrome. This architectural difference is
  due to the higher process cost on Android, and means Android Chrome is more
  susceptible to malicious PAC scripts. The other consequence is that Android
  Chrome can have distinct regressions from Desktop Chrome as the service setup
  is quite different (and most `browser_tests` are not run on Android either).

* [WebView does not use Chrome's PAC
  resolver](https://bugs.chromium.org/p/chromium/issues/detail?id=989667).
  Instead Android WebView uses the Android system's PAC resolver, which is less
  optimized and uses an old build of V8. When the system is configured to use
  PAC, Android WebView's net code will see the proxy settings as being a
  single HTTP proxy on `localhost`. The system localhost proxy will in turn
  evaluate the PAC script and forward the HTTP request on to the resolved
  proxy. This translation has a number of effects, including what proxy
  schemes are supported, the maximum connection limits, how proxy fallback
  works, and overall performance (the current Android PAC evaluator blocks on
  DNS).

* Android system log messages for `PacProcessor` are not related to Chrome or
  its PAC evaluator. Rather, these are log messages generated by the Android
  system's PAC implementation. This confusion can arise when users add
  `alert()` to debug PAC script logic, and then refer to output in `logcat` to
  try and diagnose a resolving issue in Android Chrome.
