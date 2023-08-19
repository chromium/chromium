// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/position_cache_impl.h"

#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/geolocation/wifi_data.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {

// static
const size_t PositionCacheImpl::kMaximumSize = 10;
// static
const base::TimeDelta PositionCacheImpl::kMaximumLifetime = base::Days(1);

PositionCacheImpl::CacheEntry::CacheEntry(
    const Hash& hash,
    mojom::GeopositionPtr position,
    std::unique_ptr<base::OneShotTimer> eviction_timer)
    : hash_(hash),
      position_(std::move(position)),
      eviction_timer_(std::move(eviction_timer)) {}
PositionCacheImpl::CacheEntry::~CacheEntry() = default;
PositionCacheImpl::CacheEntry::CacheEntry(CacheEntry&&) = default;
PositionCacheImpl::CacheEntry& PositionCacheImpl::CacheEntry::operator=(
    CacheEntry&&) = default;

// static
PositionCacheImpl::Hash PositionCacheImpl::MakeKey(const WifiData& wifi_data) {
  // Currently we use only WiFi data and base the key only on the MAC addresses.
  std::string key;
  const size_t kCharsPerMacAddress = 6 * 3 + 1;  // e.g. "11:22:33:44:55:66|"
  key.reserve(wifi_data.access_point_data.size() * kCharsPerMacAddress);
  const std::string separator("|");
  for (const auto& access_point_data : wifi_data.access_point_data) {
    key += separator;
    key += access_point_data.mac_address;
    key += separator;
  }
  return key;
}

PositionCacheImpl::PositionCacheImpl(const base::TickClock* clock)
    : clock_(clock) {
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

PositionCacheImpl::~PositionCacheImpl() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void PositionCacheImpl::CachePosition(const WifiData& wifi_data,
                                      const mojom::Geoposition& position) {
  const Hash key = MakeKey(wifi_data);

  // If the cache is full, remove the oldest entry.
  if (data_.size() == kMaximumSize) {
    data_.erase(data_.begin());
  }

  DCHECK_LT(data_.size(), kMaximumSize);

  auto eviction_timer = std::make_unique<base::OneShotTimer>(clock_);
  // Ensure that the entry we're adding will be evicted after kMaximumLifetime.
  // base::Unretained safe because the timer is indirectly owned by |this|.
  eviction_timer->Start(FROM_HERE, kMaximumLifetime,
                        base::BindOnce(&PositionCacheImpl::EvictEntry,
                                       base::Unretained(this), key));

  data_.emplace_back(key, position.Clone(), std::move(eviction_timer));
}

const mojom::Geoposition* PositionCacheImpl::FindPosition(
    const WifiData& wifi_data) {
  const Hash key = MakeKey(wifi_data);
  auto it = base::ranges::find(data_, key);
  if (it == data_.end()) {
    ++miss_count_;
    last_miss_ = base::Time::Now();
    return nullptr;
  }
  ++hit_count_;
  last_hit_ = base::Time::Now();
  return it->position();
}

size_t PositionCacheImpl::GetPositionCacheSize() const {
  return data_.size();
}

const mojom::GeopositionResult* PositionCacheImpl::GetLastUsedNetworkPosition()
    const {
  GEOLOCATION_LOG(DEBUG) << "Get last used network position";
  return last_used_result_.get();
}

void PositionCacheImpl::SetLastUsedNetworkPosition(
    const mojom::GeopositionResult& result) {
  last_used_result_ = result.Clone();
}

void PositionCacheImpl::FillDiagnostics(
    mojom::PositionCacheDiagnostics& diagnostics) {
  diagnostics.cache_size = data_.size();
  if (last_hit_) {
    diagnostics.last_hit = *last_hit_;
  }
  if (last_miss_) {
    diagnostics.last_miss = *last_miss_;
  }
  if (hit_count_ || miss_count_) {
    diagnostics.hit_rate =
        static_cast<double>(hit_count_) / (hit_count_ + miss_count_);
  }
  if (last_used_result_) {
    diagnostics.last_network_result = last_used_result_.Clone();
  }
}

void PositionCacheImpl::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType) {
  GEOLOCATION_LOG(DEBUG) << "Network changed";
  // OnNetworkChanged is called " when a change occurs to the host
  // computer's hardware or software that affects the route network packets
  // take to any network server.". This means that whatever position we had
  // stored for a wired connection (empty WifiData) could have become stale.
  EvictEntry(MakeKey(WifiData()));
  last_used_result_.reset();
}

void PositionCacheImpl::EvictEntry(const Hash& hash) {
  data_.erase(std::remove(data_.begin(), data_.end(), hash), data_.end());
}

}  // namespace device
