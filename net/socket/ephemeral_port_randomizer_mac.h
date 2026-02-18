// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_EPHEMERAL_PORT_RANDOMIZER_MAC_H_
#define NET_SOCKET_EPHEMERAL_PORT_RANDOMIZER_MAC_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/containers/lru_cache.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace net {

// Randomizes ephemeral source ports for TCP connections on macOS to work
// around the lack of OS-level port randomization. Must be used on a single
// thread (the network service IO thread).
class NET_EXPORT_PRIVATE EphemeralPortRandomizer {
 public:
  // Inclusive range [first, last] of ephemeral ports.
  struct PortRange {
    uint16_t first = 0;
    uint16_t last = 0;
  };

  // Returns a random integer in the inclusive range [first, last].
  using RandIntCallback =
      base::RepeatingCallback<uint16_t(uint16_t first, uint16_t last)>;

  // Returns the singleton instance.
  static EphemeralPortRandomizer& GetInstance();

  // Creates an instance for testing with custom parameters. In tests, use
  // base::test::TaskEnvironment with TimeSource::MOCK_TIME to control time.
  static std::unique_ptr<EphemeralPortRandomizer> CreateForTesting(
      PortRange port_range,
      base::TimeDelta reuse_delay,
      RandIntCallback rand_int_callback);

  EphemeralPortRandomizer(const EphemeralPortRandomizer&) = delete;
  EphemeralPortRandomizer& operator=(const EphemeralPortRandomizer&) = delete;

  ~EphemeralPortRandomizer();

  std::optional<uint16_t> PickPort(const IPEndPoint& peer_address);
  void RecordPortUse(const IPEndPoint& peer_address, uint16_t port);

 private:
  friend class base::NoDestructor<EphemeralPortRandomizer>;

  // Uses the OS ephemeral port range from sysctl (net.inet.ip.portrange),
  // falling back to 49152-65535 if the sysctl read fails.
  EphemeralPortRandomizer();

  // Used by CreateForTesting().
  EphemeralPortRandomizer(PortRange port_range,
                          base::TimeDelta reuse_delay,
                          RandIntCallback rand_int_callback);

  using PortLRUCache = base::HashingLRUCache<uint16_t, base::TimeTicks>;

  static PortRange ReadPortRangeFromSysctl();
  void CleanupExpired(PortLRUCache& port_cache, base::TimeTicks now);
  void MaybeStartCleanupTimer();
  void OnCleanupTimer();

  const PortRange port_range_;
  const base::TimeDelta reuse_delay_;
  const RandIntCallback rand_int_callback_;

  absl::flat_hash_map<IPEndPoint, PortLRUCache> recent_ports_by_peer_;
  base::OneShotTimer cleanup_timer_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace net

#endif  // NET_SOCKET_EPHEMERAL_PORT_RANDOMIZER_MAC_H_
