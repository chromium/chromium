# Caching UDP connect() Results in AddressSorterPosix

## Status

**Active Design & Implementation**

## Objective

Reduce DNS resolution and connection establishment latency by caching UDP
`connect()` results in
[`net::AddressSorterPosix`](../dns/address_sorter_posix.h).

On Android, UDP `connect()` calls incur a median latency of 817 microseconds and
a mean of 2.6 milliseconds, blocking the network IO thread. Since
`AddressSorterPosix` invokes `connect()` to determine route reachability and
source IP selection for candidate destination IPs, caching these results
significantly improves browser resolution latency.

## Background

When resolving hostnames with multiple IP addresses,
[`AddressSorter`](../dns/address_sorter.h) performs destination address sorting
(RFC 3484 / RFC 6724). On POSIX platforms,
[`AddressSorterPosix::Sort()`](../dns/address_sorter_posix.cc) evaluates
destinations by:

1. Creating a temporary
   [`DatagramClientSocket`](../socket/datagram_client_socket.h).
2. Calling `ConnectAsync()` to the destination (using port 80 if port 0 is
   provided).
3. Querying `GetLocalAddress()` to discover the OS-selected source
   [`IPAddress`](../base/ip_address.h).
4. Sorting candidate destinations based on matching policy tables.

This process causes high socket churn and redundant `connect()` system calls
during page loads.

## Design

### 1. Cache Data Structures & Capacity

We introduce a bounded LRU cache inside
[`AddressSorterPosix`](../dns/address_sorter_posix.h):

- **Cache Key**: `std::pair<IPAddress, NetworkAnonymizationKey>`.
  - **Subnet Masking**: To maximize hit rate, the destination IP is zero-masked
    to a `/24` prefix for IPv4 (and IPv4-mapped IPv6) and to a `/64` prefix for
    IPv6. Standard operating system routing tables operate at a subnet level;
    therefore, IPs within the same subnet share the same route, outgoing
    interface, and selected local source IP.
  - **State Partitioning**: Including
    [`NetworkAnonymizationKey`](../base/network_anonymization_key.h) prevents
    cross-origin timing attacks where a malicious top-level site uses the cache
    speedup as a browser history oracle.
- **Cache Capacity**: A limit of `4096` entries (approximately 400 KB memory
  footprint) provides an excellent hit rate for multi-tab sessions.
- **Thread Safety**: Access is guarded by `THREAD_CHECKER(thread_checker_)`.

### 2. Lookup & Insertion Flow

`AddressSorter::Sort()` is updated to accept
`const NetworkAnonymizationKey& anonymization_key`.

- **Cache Hit**: Bypasses socket creation and `ConnectAsync()` entirely,
  populating the route reachability and source address from the cache.
- **Cache Miss**: Allocates a socket and initiates `ConnectAsync()`.
  - If `ConnectAsync()` completes synchronously, the result is immediately
    cached inline. This allows duplicate subnets within the same `Sort()`
    invocation to hit the cache.
  - If it returns `net::ERR_IO_PENDING`, the result is cached in the
    asynchronous completion callback.
- **No Coalescing**: Concurrent `Sort()` requests for the same subnet are not
  coalesced. The minor overhead of occasional redundant local `connect()` calls
  is preferred over the high complexity of managing pending callback queues.

### 3. Invalidation

To prevent serving stale routing decisions after a network change (e.g.,
switching from Wi-Fi to Cellular, or connecting to a VPN):

- `AddressSorterPosix` implements both
  [`IPAddressObserver`](../base/network_change_notifier.h) and
  [`NetworkChangeObserver`](../base/network_change_notifier.h).
- The connect cache is fully cleared (`connect_cache_.Clear()`) on both
  `OnIPAddressChanged()` and `OnNetworkChanged()`.

### 4. Configuration

The cache is guarded by the `net::features::kAddressSorterConnectCache` feature
flag (disabled by default). When disabled, `AddressSorterPosix` falls back to
the original un-cached behavior.

## Testing

### Parameterized Unit Tests

All existing tests in
[`net/dns/address_sorter_posix_unittest.cc`](../dns/address_sorter_posix_unittest.cc)
are parameterized to run twice: with caching enabled and with caching disabled.

### New Cache Tests

We verify:

1. **Cache Hit**: Second sort for the same IP bypasses socket creation.
2. **Invalidation**: `OnIPAddressChanged()` and `OnNetworkChanged()` empty the
   cache.
3. **Partitioning**: Unique `NetworkAnonymizationKey`s partition cached entries.
4. **Subnet Masking**: IPs in the same `/24` or `/64` subnet successfully hit
   the same cache entry, while differing subnets do not.
