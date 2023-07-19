// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_POSITION_CACHE_H_
#define SERVICES_DEVICE_GEOLOCATION_POSITION_CACHE_H_

#include <cstddef>

namespace device {
namespace mojom {
class Geoposition;
class GeopositionResult;
class PositionCacheDiagnostics;
}  // namespace mojom

struct WifiData;

// Cache of recently resolved locations, keyed by the set of unique WiFi APs
// used in the network query.
class PositionCache {
 public:
  virtual ~PositionCache() = default;

  // Caches the current position response for the current set of cell ID and
  // WiFi data. In the case of the cache exceeding an implementation-defined
  // maximum size this will evict old entries in FIFO orderer of being added.
  virtual void CachePosition(const WifiData& wifi_data,
                             const mojom::Geoposition& position) = 0;

  // Searches for a cached position response for the current set of data.
  // Returns nullptr if the position is not in the cache, or the cached
  // position if available. Ownership remains with the cache. Do not store
  // the pointer, treat it as an iterator into the cache's internals.
  virtual const mojom::Geoposition* FindPosition(const WifiData& wifi_data) = 0;

  // Returns the number of cached position responses stored in the cache.
  virtual size_t GetPositionCacheSize() const = 0;

  // Returns most recently used position, or `nullptr` if
  // SetLastUsedNetworkPosition wasn't called yet.
  virtual const mojom::GeopositionResult* GetLastUsedNetworkPosition()
      const = 0;

  // Stores the most recently used position.
  virtual void SetLastUsedNetworkPosition(
      const mojom::GeopositionResult& result) = 0;

  virtual void FillDiagnostics(mojom::PositionCacheDiagnostics& diagnostics) {}
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_POSITION_CACHE_H_
