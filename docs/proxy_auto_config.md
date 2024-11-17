# Proxy Auto Config Using WPAD

Most systems support manually configuring a proxy for web access, but this is
cumbersome and kind of technical, so Chrome also supports
[WPAD](http://en.wikipedia.org/wiki/Web_Proxy_Autodiscovery_Protocol) for proxy
configuration (enabled if "automatically detect proxy settings" is enabled on
Windows).

## Problem

Currently, WPAD is pretty slow when we're starting up Chrome - we have to query
the local network for WPAD servers using DNS (and maybe NetBIOS), and we wait
all the way until the resolver timeout before we try sending any HTTP requests
if there's no WPAD server. This is a really crappy user experience, since the
browser's basically unusable for a couple of seconds after startup if
autoconfig is turned on and there's no WPAD server.

## Solution

There are a couple of simplifying assumptions we make:

*   If there is a WPAD server, it is on the same network as us, and hence likely
    to respond to lookups far more quickly than a random internet DNS server
    would.
*   If we get a lookup success for WPAD, there's overwhelmingly likely to be a
    live WPAD server. The WPAD script could also be large (!?) whereas the DNS
    response is necessarily small.

Therefore our proposed solution is that when we're trying to do WPAD resolution,
we fail very fast if the WPAD server doesn't immediately respond to a lookup
(like, 100ms or less). If there's no WPAD server, we'll time the lookup out in
100ms and get ourselves out of the critical path much faster. We won't time out
lookups for explicitly-configured WPAD servers (i.e., custom PAC script URLs) in
this fashion; those will still use the normal DNS timeout.

**This could have bad effects on networks with slow DNS or WPAD servers**, so we
should be careful to allow users to turn this off, and we should keep statistics
as to how often lookups succeed after the timeout.

So here's what our WPAD lookup policy looks like **currently** in practice
(assuming WPAD is enabled throughout):

*   If there's no WPAD server on the network, we try to do a lookup for WPAD,
    time out after two seconds, and disable WPAD. Until this time, no requests
    can proceed.
*   If there's a WPAD server and our lookup for it answers in under two seconds,
    we use that WPAD server (fetch and execute its script) and proceed with
    requests.
*   If there's a WPAD server and our lookup for it answers after two seconds, we
    time out and do not use it (ever) until a network change triggers a WPAD
    reconfiguration.

Here's what the **proposed** lookup policy looks like in practice:

*   If there's no WPAD server on the network, we try to do a lookup for WPAD,
    time out after 100ms, and disable WPAD.
*   If there's a WPAD server and our lookup for it answers in under 100ms or
    it's explicitly configured (via a custom PAC URL), we use that WPAD server.
*   If there's a WPAD server and our lookup for it answers after 100ms, we time
    out and do not use it until a network change.
