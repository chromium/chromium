// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_CLIENT_SESSION_CACHE_H_
#define NET_SSL_SSL_CLIENT_SESSION_CACHE_H_

#include <stddef.h>
#include <time.h>

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/containers/lru_cache.h"
#include "base/functional/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/raw_ptr.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/session_usage.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace base {
class Clock;
}

namespace net {

class NET_EXPORT SSLClientSessionCache : public base::MemoryPressureListener {
 public:
  struct Config {
    // The maximum number of entries in the cache.
    size_t max_entries = 1024;
    // The number of calls to Lookup before a new check for expired sessions.
    size_t expiration_check_count = 256;
  };

  struct NET_EXPORT Key {
    Key();
    Key(const Key& other);
    Key(Key&& other);
    ~Key();
    Key& operator=(const Key& other);
    Key& operator=(Key&& other);

    bool operator==(const Key& other) const;
    bool operator<(const Key& other) const;

    HostPortPair server;
    std::optional<IPAddress> dest_ip_addr;
    NetworkAnonymizationKey network_anonymization_key;
    PrivacyMode privacy_mode = PRIVACY_MODE_DISABLED;
    SessionUsage session_usage = SessionUsage::kDestination;
    ProxyChain proxy_chain = ProxyChain::Direct();
    size_t proxy_chain_index = 0;
  };

  explicit SSLClientSessionCache(const Config& config);

  SSLClientSessionCache(const SSLClientSessionCache&) = delete;
  SSLClientSessionCache& operator=(const SSLClientSessionCache&) = delete;

  ~SSLClientSessionCache() override;

  // Returns true if |entry| is expired as of |now|.
  static bool IsExpired(SSL_SESSION* session, time_t now);

  size_t size() const;
  size_t max_size() const;

  // Returns the current generation number. When a new socket is initialized,
  // the `generation_number()` at the time should be recorded and used when
  // inserting the session into the cache, which will ensure that sessions
  // created with stale configs don't get cached.
  uint64_t generation_number() const { return generation_number_; }

  // Returns the session associated with |cache_key| and moves it to the front
  // of the MRU list. Returns nullptr if there is none.
  bssl::UniquePtr<SSL_SESSION> Lookup(const Key& cache_key);

  // Inserts |session| into the cache at |cache_key|. If there is an existing
  // one, it is released. Every |expiration_check_count| calls, the cache is
  // checked for stale entries. If |generation_number| does not match the
  // current |generation_number_|, the insert will be ignored.
  void Insert(uint64_t generation_number,
              const Key& cache_key,
              bssl::UniquePtr<SSL_SESSION> session);

  // Clears early data support for all current sessions associated with
  // |cache_key|. This may be used after a 0-RTT reject to avoid unnecessarily
  // offering 0-RTT data on retries. See https://crbug.com/1066623.
  void ClearEarlyData(const Key& cache_key);

  // Removes all entries associated with items in |servers|.
  void FlushForServers(const base::flat_set<HostPortPair>& servers);

  // Removes all entries from the cache and updates the generation number.
  // Future insertions will be ignored unless they match the new generation.
  void Flush();

  void SetClockForTesting(base::Clock* clock);

 private:
  struct Entry {
    Entry();
    Entry(Entry&&);
    ~Entry();

    // Adds a new session onto this entry, dropping the oldest one if two are
    // already stored.
    void Push(bssl::UniquePtr<SSL_SESSION> session);

    // Retrieves the latest session from the entry, removing it if its
    // single-use.
    bssl::UniquePtr<SSL_SESSION> Pop();

    // Removes any expired sessions, returning true if this entry can be
    // deleted.
    bool ExpireSessions(time_t now);

    bssl::UniquePtr<SSL_SESSION> sessions[2];
  };

  // Removes all expired sessions from the cache.
  void FlushExpiredSessions();

  // Clear cache on low memory notifications callback.
  void OnMemoryPressure(
      base::MemoryPressureLevel memory_pressure_level) override;

  raw_ptr<base::Clock> clock_;
  uint64_t generation_number_ = 0;
  Config config_;
  base::LRUCache<Key, Entry> cache_;
  size_t lookups_since_flush_ = 0;
  std::unique_ptr<base::AsyncMemoryPressureListenerRegistration>
      memory_pressure_listener_registration_;
};

}  // namespace net

#endif  // NET_SSL_SSL_CLIENT_SESSION_CACHE_H_
