# Caching UDP connect() Results in AddressSorterPosix

## Status

**Shipping on Android**

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

[`AddressSorterPosix`](../dns/address_sorter_posix.h) delegates connect result
caching to a [`ConnectPredictor`](../dns/connect_predictor.h) interface,
implemented by
[`ConnectPredictorLRUCache`](../dns/connect_predictor_lru_cache.h) using a
three-level `base::HashingLRUCache` hierarchy:

- **Cache Hierarchy & Keys**:
  1. **Network Level**: Keyed by
     [`handles::NetworkHandle`](../base/network_handle.h) (`target_network`),
     mapping to a per-network `NetworkAnonymizationKey` LRU cache.
  2. **Partition Level**: Keyed by
     [`NetworkAnonymizationKey`](../base/network_anonymization_key.h), mapping
     to a `ConnectPredictorLRUCache::Partition`. Including
     `NetworkAnonymizationKey` and `target_network` isolates routing state
     across networks and prevents cross-origin timing attacks where a malicious
     top-level site uses the cache speedup as a browser history oracle.
  3. **Prediction Level**: Each `Partition` maintains an LRU cache keyed by
     masked destination [`IPAddress`](../base/ip_address.h) and mapping to
     `ConnectPredictor::ConnectResult` (`rv` and `source_address`).
- **Subnet Masking**: To maximize hit rate, the destination IP is zero-masked to
  a `/24` prefix for IPv4 (and IPv4-mapped IPv6) and to a `/64` prefix for IPv6.
  Standard operating system routing tables operate at a subnet level; therefore,
  IPs within the same subnet share the same route, outgoing interface, and
  selected local source IP.
- **Cache Capacity**: Controlled by feature parameters on
  `net::features::kAddressSorterConnectCache`:
  - `kAddressSorterConnectCacheMaxNetworks`: default `2` networks.
  - `kAddressSorterConnectCacheMaxNaksPerNetwork`: default `16` partitions per
    network.
  - `kAddressSorterConnectCacheMaxPredictionsPerPartition`: default `1024`
    predictions per partition.
- **Thread Safety**: Access in `AddressSorterPosix` is guarded by
  `THREAD_CHECKER(thread_checker_)`.

### 2. Lookup & Insertion Flow

`AddressSorter::Sort()` accepts `const NetworkAnonymizationKey&
anonymization_key` and `handles::NetworkHandle target_network`, which are used
to look up a `base::WeakPtr<ConnectPredictor::Partition>` via
`ConnectPredictor::GetPartition()` at the start of `Sort()`.

- **Cache Hit**: When `Partition::Predict()` returns a cached `ConnectResult`,
  `SortContext` bypasses socket creation and `ConnectAsync()` entirely,
  populating the route reachability and source address from the cached result.
- **Cache Miss**: Allocates a socket and initiates `ConnectAsync()`.
  - If `ConnectAsync()` completes synchronously, the result is immediately
    recorded via `Partition::RecordResult()` inline. This allows duplicate
    subnets within the same `Sort()` invocation to hit the cache.
  - If it returns `net::ERR_IO_PENDING`, the result is recorded in the
    asynchronous completion callback (provided the `Partition` `WeakPtr` is
    still valid).
- **No Coalescing**: Concurrent `Sort()` requests for the same subnet are not
  coalesced. The minor overhead of occasional redundant local `connect()` calls
  is preferred over the high complexity of managing pending callback queues.

### 3. Invalidation

To prevent serving stale routing decisions after a network change (e.g.,
switching from Wi-Fi to Cellular, or connecting to a VPN):

- [`ConnectPredictorLRUCache`](../dns/connect_predictor_lru_cache.h) implements
  [`NetworkChangeObserver`](../base/network_change_notifier.h) and clears the
  entire cache (`cache_.Clear()`) on `OnNetworkChanged()`.
- Because `SortContext` holds a `base::WeakPtr<ConnectPredictor::Partition>`,
  clearing the cache (or evicting a partition) safely invalidates outstanding
  partition references held by any in-flight asynchronous `Sort()` calls.

### 4. Configuration

The cache is guarded by the `net::features::kAddressSorterConnectCache` feature
flag (enabled by default on Android), along with the capacity parameters
`kAddressSorterConnectCacheMaxNetworks`,
`kAddressSorterConnectCacheMaxNaksPerNetwork`, and
`kAddressSorterConnectCacheMaxPredictionsPerPartition`. When the feature is
disabled, `ConnectPredictor::Create()` returns `nullptr` and
`AddressSorterPosix` falls back to the un-cached behavior.

## Testing

### Parameterized Unit Tests

Existing sorting tests in
[`net/dns/address_sorter_posix_unittest.cc`](../dns/address_sorter_posix_unittest.cc)
(`AddressSorterPosixSyncOrAsyncTest`) are parameterized over both synchronous /
asynchronous socket connect modes and caching enabled / disabled.

### Cache Unit Tests

In [`net/dns/address_sorter_posix_unittest.cc`](../dns/address_sorter_posix_unittest.cc)
(`AddressSorterPosixCacheTest`) and
[`net/dns/connect_predictor_lru_cache_unittest.cc`](../dns/connect_predictor_lru_cache_unittest.cc)
(`ConnectPredictorLRUCacheTest`), we verify:

1. **Cache Hit**: Second sort for the same IP bypasses socket creation.
2. **Invalidation**: `OnNetworkChanged()` empties the cache and safely handles
   in-flight asynchronous sorts.
3. **Partitioning & Eviction**: Unique `NetworkAnonymizationKey`s and
   `handles::NetworkHandle`s partition cached entries, with LRU eviction at each
   of the three cache levels.
4. **Subnet Masking**: IPs in the same `/24` (IPv4 and IPv4-mapped IPv6) or
   `/64` (IPv6) subnet hit the same cache entry, while differing subnets do not.
