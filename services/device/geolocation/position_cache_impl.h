// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_POSITION_CACHE_IMPL_H_
#define SERVICES_DEVICE_GEOLOCATION_POSITION_CACHE_IMPL_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
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
  using Hash = base::string16;

  class CacheEntry {
   public:
    CacheEntry(const Hash& hash,
               const mojom::Geoposition& position,
               std::unique_ptr<base::OneShotTimer> eviction_timer);
    ~CacheEntry();
    CacheEntry(CacheEntry&&);
    CacheEntry& operator=(CacheEntry&&);

    inline bool operator==(const Hash& hash) const { return hash_ == hash; }
    const mojom::Geoposition* position() const { return &position_; }

   private:
    Hash hash_;
    mojom::Geoposition position_;
    std::unique_ptr<base::OneShotTimer> eviction_timer_;
    DISALLOW_COPY_AND_ASSIGN(CacheEntry);
  };

  static Hash MakeKey(const WifiData& wifi_data);
  void EvictEntry(const Hash& hash);

  const base::TickClock* clock_;
  std::vector<CacheEntry> data_;
  mojom::Geoposition last_used_position_;
  DISALLOW_COPY_AND_ASSIGN(PositionCacheImpl);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_POSITION_CACHE_IMPL_H_
