// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_POSITION_CACHE_IMPL_H_
#define SERVICES_DEVICE_GEOLOCATION_POSITION_CACHE_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/network_change_notifier.h"
#include "services/device/geolocation/position_cache.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace base {
class TickClock;
}  // namespace base

namespace device {

class PositionCacheImpl
    : public PositionCache,
      public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  // The maximum size of the cache of positions.
  static const size_t kMaximumSize;
  // The maximum time an entry can reside in cache before forced eviction.
  // This is to ensure the user's location cannot be tracked arbitrarily far
  // back in history.
  static const base::TimeDelta kMaximumLifetime;

  // |clock| is used to measure time left until kMaximumLifetime.
  explicit PositionCacheImpl(const base::TickClock* clock);

  PositionCacheImpl(const PositionCacheImpl&) = delete;
  PositionCacheImpl& operator=(const PositionCacheImpl&) = delete;

  ~PositionCacheImpl() override;

  void CachePosition(const WifiData& wifi_data,
                     const mojom::Geoposition& position) override;

  const mojom::Geoposition* FindPosition(
      const WifiData& wifi_data) const override;

  size_t GetPositionCacheSize() const override;

  const mojom::Geoposition& GetLastUsedNetworkPosition() const override;
  void SetLastUsedNetworkPosition(const mojom::Geoposition& position) override;

  // net::NetworkChangeNotifier::NetworkChangeObserver
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

 private:
  // In order to avoid O(N) comparisons while searching for the right WifiData,
  // we hash the contents of those objects and use the hashes as cache keys.
  using Hash = std::u16string;

  class CacheEntry {
   public:
    CacheEntry(const Hash& hash,
               const mojom::Geoposition& position,
               std::unique_ptr<base::OneShotTimer> eviction_timer);

    CacheEntry(const CacheEntry&) = delete;
    CacheEntry& operator=(const CacheEntry&) = delete;

    ~CacheEntry();
    CacheEntry(CacheEntry&&);
    CacheEntry& operator=(CacheEntry&&);

    inline bool operator==(const Hash& hash) const { return hash_ == hash; }
    const mojom::Geoposition* position() const { return &position_; }

   private:
    Hash hash_;
    mojom::Geoposition position_;
    std::unique_ptr<base::OneShotTimer> eviction_timer_;
  };

  static Hash MakeKey(const WifiData& wifi_data);
  void EvictEntry(const Hash& hash);

  raw_ptr<const base::TickClock> clock_;
  std::vector<CacheEntry> data_;
  mojom::Geoposition last_used_position_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_POSITION_CACHE_IMPL_H_
