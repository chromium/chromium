// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ephemeral_port_randomizer_mac.h"

#include <sys/sysctl.h>

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "net/base/features.h"

namespace net {

namespace {

constexpr uint16_t kDefaultEphemeralPortStart = 49152;
constexpr uint16_t kDefaultEphemeralPortEnd = 65535;

bool ReadSysctlInt(const char* name, int* value) {
  size_t value_len = sizeof(*value);
  if (sysctlbyname(name, value, &value_len, nullptr, 0) != 0) {
    return false;
  }
  return value_len == sizeof(*value);
}

uint16_t DefaultRandInt(uint16_t first, uint16_t last) {
  return static_cast<uint16_t>(base::RandIntInclusive(first, last));
}

}  // namespace

EphemeralPortRandomizer::EphemeralPortRandomizer()
    : EphemeralPortRandomizer(
          ReadPortRangeFromSysctl(),
          base::Seconds(features::kTcpPortRandomizationReuseDelaySec.Get()),
          base::BindRepeating(&DefaultRandInt)) {}

EphemeralPortRandomizer::EphemeralPortRandomizer(
    PortRange port_range,
    base::TimeDelta reuse_delay,
    RandIntCallback rand_int_callback)
    : port_range_(port_range),
      reuse_delay_(reuse_delay),
      rand_int_callback_(std::move(rand_int_callback)) {
  CHECK(rand_int_callback_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

// static
std::unique_ptr<EphemeralPortRandomizer>
EphemeralPortRandomizer::CreateForTesting(PortRange port_range,
                                          base::TimeDelta reuse_delay,
                                          RandIntCallback rand_int_callback) {
  return base::WrapUnique(new EphemeralPortRandomizer(
      port_range, reuse_delay, std::move(rand_int_callback)));
}

EphemeralPortRandomizer::~EphemeralPortRandomizer() = default;

std::optional<uint16_t> EphemeralPortRandomizer::PickPort(
    const IPEndPoint& peer_address) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const PortRange& range = port_range_;
  if (range.first == 0 || range.last == 0 || range.first > range.last) {
    return std::nullopt;
  }

  uint32_t range_size = static_cast<uint32_t>(range.last - range.first) + 1;
  uint16_t start_port = rand_int_callback_.Run(range.first, range.last);
  base::TimeTicks now = base::TimeTicks::Now();

  // Look up the peer entry once. If there are no recently-used ports for this
  // peer, we can return immediately without looping. Record the port as used
  // immediately so subsequent callers won't pick the same port.
  auto [peer_it, inserted] = recent_ports_by_peer_.try_emplace(
      peer_address, PortLRUCache::NO_AUTO_EVICT);
  if (inserted) {
    peer_it->second.Put(start_port, now);
    return start_port;
  }

  auto& port_cache = peer_it->second;
  CleanupExpired(port_cache, now);

  if (port_cache.empty()) {
    port_cache.Put(start_port, now);
    return start_port;
  }

  for (uint32_t attempt = 0; attempt < range_size; ++attempt) {
    uint32_t offset =
        (static_cast<uint32_t>(start_port - range.first) + attempt) %
        range_size;
    uint16_t candidate = static_cast<uint16_t>(range.first + offset);
    // Use Peek() to avoid reordering the LRU cache during lookup.
    auto port_it = port_cache.Peek(candidate);
    if (port_it == port_cache.end() ||
        (now - port_it->second) >= reuse_delay_) {
      port_cache.Put(candidate, now);
      return candidate;
    }
  }

  base::UmaHistogramBoolean("Net.EphemeralPortRandomizer.PickPortExhausted",
                            true);
  return std::nullopt;
}

void EphemeralPortRandomizer::RecordPortUse(const IPEndPoint& peer_address,
                                            uint16_t port) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::TimeTicks now = base::TimeTicks::Now();

  auto [peer_it, inserted] = recent_ports_by_peer_.try_emplace(
      peer_address, PortLRUCache::NO_AUTO_EVICT);
  auto& port_cache = peer_it->second;
  if (!inserted) {
    CleanupExpired(port_cache, now);
  }
  port_cache.Put(port, now);

  MaybeStartCleanupTimer();
}

// static
EphemeralPortRandomizer::PortRange
EphemeralPortRandomizer::ReadPortRangeFromSysctl() {
  int first = 0;
  int last = 0;
  if (!ReadSysctlInt("net.inet.ip.portrange.first", &first) ||
      !ReadSysctlInt("net.inet.ip.portrange.last", &last) || first <= 0 ||
      last <= 0 || first > last || last > 65535) {
    return {kDefaultEphemeralPortStart, kDefaultEphemeralPortEnd};
  }
  return {static_cast<uint16_t>(first), static_cast<uint16_t>(last)};
}

void EphemeralPortRandomizer::CleanupExpired(PortLRUCache& port_cache,
                                             base::TimeTicks now) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Iterate from the back (least recently used / oldest) and erase expired
  // entries. Stop at the first non-expired entry since older entries are
  // further back.
  while (!port_cache.empty()) {
    auto oldest = port_cache.rbegin();
    if ((now - oldest->second) >= reuse_delay_) {
      port_cache.Erase(oldest);
    } else {
      break;
    }
  }
}

void EphemeralPortRandomizer::MaybeStartCleanupTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!cleanup_timer_.IsRunning() && !recent_ports_by_peer_.empty()) {
    // This use of base::Unretained is safe because `cleanup_timer_` is owned
    // by `this`.
    cleanup_timer_.Start(
        FROM_HERE, reuse_delay_,
        base::BindOnce(&EphemeralPortRandomizer::OnCleanupTimer,
                       base::Unretained(this)));
  }
}

void EphemeralPortRandomizer::OnCleanupTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::TimeTicks now = base::TimeTicks::Now();
  absl::erase_if(recent_ports_by_peer_, [&](auto& entry) {
    CleanupExpired(entry.second, now);
    return entry.second.empty();
  });

  // Restart the timer if there are still entries to clean up later.
  MaybeStartCleanupTimer();
}

// static
EphemeralPortRandomizer& EphemeralPortRandomizer::GetInstance() {
  static base::NoDestructor<EphemeralPortRandomizer> instance;
  return *instance;
}

}  // namespace net
