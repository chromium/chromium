// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_FAKE_POSITION_CACHE_H_
#define SERVICES_DEVICE_GEOLOCATION_FAKE_POSITION_CACHE_H_

#include <utility>
#include <vector>

#include "services/device/geolocation/position_cache.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {

class FakePositionCache : public PositionCache {
 public:
  FakePositionCache();

  FakePositionCache(const FakePositionCache&) = delete;
  FakePositionCache& operator=(const FakePositionCache&) = delete;

  ~FakePositionCache() override;

  void CachePosition(const WifiData& wifi_data,
                     const mojom::Geoposition& position) override;
  const mojom::Geoposition* FindPosition(const WifiData& wifi_data) override;
  size_t GetPositionCacheSize() const override;
  const mojom::GeopositionResult* GetLastUsedNetworkPosition() const override;
  void SetLastUsedNetworkPosition(
      const mojom::GeopositionResult& position) override;

 private:
  std::vector<std::pair<WifiData, mojom::GeopositionPtr>> data;
  mojom::GeopositionResultPtr last_used_result;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_FAKE_POSITION_CACHE_H_
