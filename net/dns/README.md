# Chrome Host Resolution

Implementation and support of host resolution for the Chrome network stack.
Includes client implementations of host resolution protocols (DNS and mDNS),
host resolution caching, support for dealing with system host resolution
(including reading HOSTS files and tracking system network settings related to
host resolution), and various related utilities.

*** promo
Note: "DNS" in this directory (including the directory name) is often used as
shorthand for all host resolution, not just that using the Domain Name System.
This document attempts to use "DNS" only to refer to the actual Domain Name
System, except when referring to strings or paths that contain other usage of
"DNS".
***

[TOC]

## Usage

### From outside the network service

Most interaction with host resolution should be through the [network service](/services/network/README.md)
[`network::HostResolver`](/services/network/public/mojom/host_resolver.mojom),
retrieved from [`network::NetworkContext`](/services/network/public/mojom/network_context.mojom)
using `network::NetworkContext::CreateHostResolver()`.

Host resolution is requested using `network::HostResolver::ResolveHost()`. There
is also a shortcut using `network::NetworkContext::ResolveHost()` when a
separate passable object is not needed.

Some general utilities are also available in [`/net/dns/public/`](/net/dns/public/)
that are intended for use by any code, inside or outside the network service.
Otherwise, code outside the network service should never interact directly with
the code in [`/net/dns/`](/net/dns/).

### From inside the network service

Inside the network service or inside the Chrome networking stack, host
resolution goes through [`net::HostResolver`](/net/dns/host_resolver.h),
retrieved from [`net::URLRequestContext`](/net/url_request/url_request_context.h).

### Stand-alone tools

Tests and stand-alone tools not part of the browser may interact with host
resolution directly by creating their own HostResolvers using
`net::HostResolver::CreateStandaloneResolver()`.

## Test support

### MockHostResolver

[`net::MockHostResolver`](/net/dns/mock_host_resolver.h)

Tests with the ability to inject and replace the used `net::HostResolver` should
replace it with a `net::MockHostResolver`, allowing rule-based results.
`net::MockCachingHostResolver` is the same except it includes basic support for
the caching functionality normally done by prod `net::HostResolver`s.

Some tests may also find `net::HangingHostResolver` useful. It will begin host
resolution requests, but never complete them until cancellation.

### TestHostResolver

[`content::TestHostResolver`](/content/public/test/test_host_resolver.h)

Used by most browser tests (via [`content::BrowserTestBase`](/content/public/test/browser_test_base.h)),
`content::TestHostResolver` installs itself on creation globally into all host
resolvers in the process. By default, only allows resolution of the local host
and returns `net::ERR_NAME_NOT_RESOLVED` for other hostnames. Allows setting rules
for other results using a [net::RuleBasedHostResolverProc](/net/dns/mock_host_resolver.h).

*** note
**Warning:** `content::TestHostResolver` only replaces host address resolution
to the system and then artificially uses such system resolution for many
requests that would normally be handled differently (e.g. using the built-in DNS
client). This means a significant amount of normal prod host resolution logic
will be bypassed in tests using `content::TestHostResolver`.
***

### Request remapping

Most prod logic for creating HostResolvers will check if any global remappings
have been requested. In the browser, this is requested using the
["host-resolver-rules"](/services/network/public/cpp/network_switches.h)
commandline flag.

See [`net::HostMappingRules`](/net/base/host_mapping_rules.h) for details on the
format of the rules string. Allows remapping any request hostname to another
hostname, an IP address, or a NOTFOUND error.

## Implementation

### HostResolver

[`net::HostResolver`](/net/dns/host_resolver.h)

The main interface for requesting host resolution within the network stack or
network service. In prod, generally owned, and retrieved as-needed from
[`net::URLRequestContext`](/net/url_request/url_request_context.h)s. Created
using `net::HostResolver::CreateResolver()` or
`net::HostResolver::CreateStandaloneResolver()`.

Various implementations are used in prod.

#### ContextHostResolver

[`net::ContextHostResolver`](/net/dns/context_host_resolver.h)

The main prod implementation of `net::HostResolver`. Expected to be owned 1:1 by
a single `net::URLRequestContext`, the `net::ContextHostResolver` owns or keeps
references to per-URLRequestContext properties used for host resolution,
including an owned [`net::HostCache`](/net/dns/host_cache.h).

On resolution, calls into an underlying `net::HostResolverManager` with the per-
context properties.

On destruction, silently cancels all host resolution requests made through the
`net::ContextHostResolver` instance. This prevents the underlying
`net::HostResolverManager` from continuing to reference the per-context
properties that may be destroyed on destruction of the `net::URLRequestContext`
or `net::ContextHostResolver`.

#### MappedHostResolver

[`net::MappedHostResolver`](/net/dns/mapped_host_resolver.h)

A wrapping implementation around another `net::HostResolver`. Maintains request
remapping rules to remap request hostnames to other hostnames or IP addresses.

Used to implement the ["host-resolver-rules"](/services/network/public/cpp/network_switches.h)
commandline flag.

#### StaleHostResolver

[`cronet::StaleHostResolver`](/components/cronet/stale_host_resolver.h)

A wrapping implementation around another `net::HostResolver`. Returns stale
(expired or invalidated by network changes) data from the `net::HostCache` when
non-stale results take longer than a configurable timeout. Reduces host
resolution latency at the expense of accuracy.

Only used and created by [Cronet](/components/cronet/README.md).

### HostResolverManager

[`net::HostResolverManager`](/net/dns/host_resolver_manager.h)

Scheduler and controller of host resolution requests. Contains the logic for
immediate host resolution from fast local sources (e.g. querying
`net::HostCache`s, IP address literals, etc). Throttles, schedules, and merges
asynchronous jobs for resolution from slower network sources. 

On destruction, silently cancels all in-progress host resolution requests.

In prod, a single shared `net::HostResolverManager` is generally used for the
entire browser. The shared manager is owned and configured by the
[`network::NetworkService`](/services/network/network_service.h).

#### Request

`net::HostResolverManager::RequestImpl`

Implementation of [`net::HostResolver::ResolveHostRequest`](/net/dns/host_resolver.h)
and overall representation of a single request for resolution from a
`net::HostResolverManager`. The `RequestImpl` object itself primarily acts only
as a container of parameters and results for the request, leaving the actual
logic to the `net::HostResolverManager` itself.

Data collected at this layer:

* "Net.DNS.Request.TotalTime" (recommended for experiments)
* "Net.DNS.Request.TotalTimeAsync"

#### Job

`net::HostResolverManager::Job`

Representation of an asynchronous job for resolution from slower network
sources. Contains the logic to determine and query the appropriate source for
host resolution results with retry and fallback support to other sources. On
completion adds results to relevant `net::HostCache`s and invokes request
callbacks.

Multiple requests can be merged into a single Job if compatible. This includes
merging newly-started Jobs with already-running Jobs.

`net::HostResolverManager` schedules and throttles running
`net::HostResolverManager::Job`s using a [`net::PrioritizedDispatcher`](/net/base/prioritized_dispatcher.h).
The throttling is important to avoid overworking network sources, especially
poorly designed home routers that may crash on only a small number of concurrent
DNS resolves.

Data collected at this layer:

* "Net.DNS.ResolveSuccessTime"
* "Net.DNS.ResolveFailureTime"
* "Net.DNS.ResolveCategory"
* "Net.DNS.ResolveError.Fast"
* "Net.DNS.ResolveError.Slow"

### Host resolution sources

Various sources are used to query host resolution. The sources to be used by a
`net::HostResolverManager::Job` are determined in advance of running the Job by
`net::HostResolverManager::CreateTaskSequence()`, which outputs a list of
`net::HostResolverManager::TaskType` specifying the order of sources to attempt.
By default, this will use internal logic to decide the source to use and will
often allow fallback to additional sources.

The sources chosen by default are also affected by the Secure DNS mode, by
default determined from
[`net::DnsConfig::secure_dns_mode`](/net/dns/dns_config.h) but overridable for
individual requests using
`net::HostResolver::ResolveHostParameters::secure_dns_mode_override`.

Specific sources for a request can be
specified using `net::HostResolver::ResolveHostParameters::source` and
[`net::HostResolverSource`](/net/dns/host_resolver_source.h).

The Job will then use \*Task objects that implement the behavior specific to the
particular resolution sources.

#### SYSTEM

`net::HostResolverSource::SYSTEM`
`net::HostResolverManager::TaskType::SYSTEM`

Implemented by: `net::HostResolverSystemTask`

Usually called the "system resolver" or sometimes the "proc resolver" (because
it was historically always implemented using net::HostResolverProc). Results
are queried from the system or OS using the `getaddrinfo()` OS API call. This
source is only capable of address (A and AAAA) resolves but will also query for
canonname info if the request includes the `HOST_RESOLVER_CANONNAME` flag. The
system will query from its own internal cache, HOSTS files, DNS, and sometimes
mDNS, depending on the capabilities of the system.

When host resolution requests do not specify a source, the system resolver will
always be used for **address resolves** when **any** of the following are true:

* Requests with the `HOST_RESOLVER_CANONNAME` flag
* For hostnames ending in ".local"
* When the Secure DNS mode is `net::SecureDnsMode::OFF` and
  `net::HostResolverSource::DNS` is not enabled via
  `net::HostResolverManager::SetInsecureDnsClientEnabled(true)`
* When a system DNS configuration could not be determined

Secure DNS requests cannot be made using the system resolver.

`net::HostResolverSystemTask`'s behavior can be overridden by an asynchronous
global override (e.g. in case resolution needs to be brokered out of the current
process for sandboxing reasons). Otherwise, it posts a blocking task to a
[`base::ThreadPool`](/base/task/thread_pool.h) to make blocking resolution
requests in-process.
On a timeout, additional attempts are made, but previous attempts are not
cancelled as there is no cancellation mechanism for `getaddrinfo()`. The first
attempt to complete is used and any other attempt completions are ignored.

In prod, the blocking task runner always calls `SystemHostResolverCall()`, which
makes the actual call to `getaddrinfo()` using the
[`net::AddressInfo`](/net/dns/address_info.h) helper. In tests, the blocking
task runner may use a test implementation of
[`net::HostResolverProc`](/net/dns/host_resolver_proc.h), which itself can be
chained.

Data collected specifically for this source:

* "Net.DNS.SystemTask.SuccessTime"
* "Net.DNS.SystemTask.FailureTime"

#### DNS

`net::HostResolverSource::DNS`
`net::HostResolverManager::TaskType::DNS`
`net::HostResolverManager::TaskType::SECURE_DNS`

Implemented by: `net::HostResolverManager::DnsTask`

Usually called the "built-in resolver" or the "async resolver". Results are
queried from DNS using [`net::DnsClient`](/net/dns/dns_client.h), a Chrome
network stack implementation of a DNS "stub resolver" or "DNS client".

When host resolution requests do not specify a source, the built-in resolver
will be used when **all** of the following are true:

* DnsClient is enabled for insecure requests enabled via
  `net::HostResolverManager::SetInsecureDnsClientEnabled(true)` or
  the Secure DNS mode is not `net::SecureDnsMode::OFF`.
* The system DNS configuration could be determined successfully
* The request hostname does not end in ".local"
* The request is not an address query with the `HOST_RESOLVER_CANONNAME` flag

The `net::HostResolverManager::DnsTask` will create and run a
[`net::DnsTransaction`](/net/dns/dns_transaction.h) for each DNS name/type pair
to be queried. The task will then process successful results from the returned
[`net::DnsResponse`](/net/dns/dns_response.h).

When a request requires both A and AAAA results, they are handled via two
separate `net::DnsTransaction`s and the `net::HostResolverManager::DnsTask` will
request a second slots from the `net::PrioritizedDispatcher` used by
`net::HostResolverManager`. The A transaction is started immediately on starting
the `net::HostResolverManager::DnsTask`, and the AAAA transaction is started
once a second dispatcher slot can be obtained.

Each `net::DnsTransaction` internally makes a series of `net::DnsAttempt`s, each
representing an individual DNS request. A single `net::DnsTransaction` can run
many `net::DnsAttempt`s due to retry logic, fallback between multiple configured
DNS servers, and name permutation due to configured search suffixes.

Data collected specifically for this source (more internally to
`net::DnsTransaction` implementation not listed here):

* "Net.DNS.DnsTask.SuccessTime"
* "Net.DNS.InsecureDnsTask.FailureTime"
* "Net.DNS.JobQueueTime.PerTransaction"
* "Net.DNS.JobQueueTime.Failure"
* "Net.DNS.JobQueueTime.Success"

#### MULTICAST_DNS

`net::HostResolverSource::MULTICAST_DNS`
`net::HostResolverManager::TaskType::MDNS`

Implemented by [`net::HostResolverMdnsTask`](/net/dns/host_resolver_mdns_task.h)

Results are queried from mDNS using [`net::MDnsClient`](/net/dns/mdns_client.h).

When host resolution requests do not specify a source, mDNS is only used for
non-address requests when the request hostname ends in ".local".

mDNS requests start with [`net::HostResolverMdnsTask`](/net/dns/host_resolver_mdns_task.h),
which will create and run a [`net::MDnsTransaction`](/net/dns/mdns_client.h) for
each query type needed.

Unlike `net::HostResolverManager::DnsTask`, each `net::HostResolverMdnsTask`
will only ever use a single dispatcher slot, even when both A and AAAA types are
queried concurrently.

`net::MDnsClient` maintains its own cache, separate from the main
[`net::HostCache`](/net/dns/host_cache.h) owned by the
[`net::ContextHostResolver`](/net/dns/context_host_resolver.h). As such, mDNS
results are never cached in the `net::HostCache`.

### IPv6 and connectivity

Some poorly written DNS servers, especially on home routers, are unaware of the
existence of IPv6 and will result in bad performance or even crash when sent
AAAA DNS queries.

To avoid such issues, `net::HostResolverManager` heuristically detects IPv4-only
networks by attempting a UDP connection to `2001:4860:4860::8888` (the IPv6
address for Google Public DNS). If the connection fails, Chrome will convert
host resolution requests for `net::DnsQueryType::UNSPECIFIED` to
`net::DnsQueryType::A`. This generally results in disallowing AAAA requests.

Exceptions when AAAA requests are always allowed despite a failed connectivity
check:

* The host resolution request explicitly requests `net::DnsQueryType::AAAA`
* IP address literal resolution including when a hostname request has been
  rewritten to an IP address literal using `net::MappedHostResolver`
* Results read from HOSTS files where there is no non-loopback IPv4 result. Note
  that this exception only applies when Chrome does the read from HOSTS. When
  Chrome's built-in DNS client is not used, HOSTS is only read by the system
  where Chrome would only request A results to avoid the system making AAAA DNS
  queries.

The heuristic for detecting IPv4-only networks is not perfect. E.g., it fails
and disallows AAAA requests in private (no global internet access including to
Google Public DNS) IPv6-only networks, which could then break most Chrome usage
on the network because, being an IPv6-only network, AAAA results are necessary.

Workarounds to allow Chrome to attempt to load IPv6 endpoints when the
connectivity check fails:

* Starting Chrome with
  `--host-resolver-rules="MAP the.hostname.com [dead::beef]"` where
  `the.hostname.com` is the hostname to allow resolving and `dead::beef` is the
  IPv6 address to resolve it to. `net::MappedHostResolver` acts at a level
  before IPv6 connectivity checks, and if a hostname is remapped to an IP
  literal, connectivity checks do not apply.
* Add entries for the hostnames to resolve to the HOSTS file with just IPv6
  results.  Only works with the built-in DNS client is used.
* Add a network route to `2001:4860:4860::8888`. Doesn't have to actually be
  functional (could just drop requests to it).  As long as Chrome can connect a
  UDP socket to the address, it will pass the heuristic checking
  IPv6-connectivity.
