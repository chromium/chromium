// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_CLIENT_SESSION_CACHE_H_
#define NET_SSL_SSL_CLIENT_SESSION_CACHE_H_

#include <stddef.h>
#include <time.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_provider.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/base/privacy_mode.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace base {
class Clock;
namespace trace_event {
class ProcessMemoryDump;
}
}

namespace net {

class NET_EXPORT SSLClientSessionCache {
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
    base::Optional<IPAddress> dest_ip_addr;
    NetworkIsolationKey network_isolation_key;
    PrivacyMode privacy_mode = PRIVACY_MODE_DISABLED;
  };

  explicit SSLClientSessionCache(const Config& config);
  ~SSLClientSessionCache();

  // Returns true if |entry| is expired as of |now|.
  static bool IsExpired(SSL_SESSION* session, time_t now);

  size_t size() const;

  // Returns the session associated with |cache_key| and moves it to the front
  // of the MRU list. Returns nullptr if there is none.
  bssl::UniquePtr<SSL_SESSION> Lookup(const Key& cache_key);

  // Inserts |session| into the cache at |cache_key|. If there is an existing
  // one, it is released. Every |expiration_check_count| calls, the cache is
  // checked for stale entries.
  void Insert(const Key& cache_key, bssl::UniquePtr<SSL_SESSION> session);

  // Removes all entries associated with |server|.
  void FlushForServer(const HostPortPair& server);

  // Removes all entries from the cache.
  void Flush();

  void SetClockForTesting(base::Clock* clock);

  // Dumps memory allocation stats. |pmd| is the ProcessMemoryDump of the
  // browser process.
  void DumpMemoryStats(base::trace_event::ProcessMemoryDump* pmd,
                       const std::string& parent_absolute_name) const;

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
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  base::Clock* clock_;
  Config config_;
  base::MRUCache<Key, Entry> cache_;
  size_t lookups_since_flush_;
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientSessionCache);
};

}  // namespace net

#endif  // NET_SSL_SSL_CLIENT_SESSION_CACHE_H_
