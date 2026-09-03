// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RLZ_WIN_LIB_RLZ_VALUE_STORE_REGISTRY_H_
#define RLZ_WIN_LIB_RLZ_VALUE_STORE_REGISTRY_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "rlz/lib/rlz_value_store.h"

namespace rlz_lib {

// Implements RlzValueStore by storing values in the windows registry.
class RlzValueStoreRegistry : public RlzValueStore {
 public:
  RlzValueStoreRegistry(const RlzValueStoreRegistry&) = delete;
  RlzValueStoreRegistry& operator=(const RlzValueStoreRegistry&) = delete;

  static std::wstring GetWideLibKeyName();

  bool HasAccess(AccessType type) override;

  bool WritePingTime(Product product, int64_t time) override;
  std::optional<int64_t> ReadPingTime(Product product) override;
  bool ClearPingTime(Product product) override;

  bool WriteAccessPointRlz(AccessPoint access_point,
                           std::string_view new_rlz) override;
  bool ReadAccessPointRlz(AccessPoint access_point,
                          char* rlz,
                          size_t rlz_size) override;
  bool ClearAccessPointRlz(AccessPoint access_point) override;
  bool UpdateExistingAccessPointRlz(std::string_view brand) override;

  bool AddProductEvent(Product product, std::string_view event_rlz) override;
  std::vector<std::string> ReadProductEvents(Product product) override;
  bool ClearProductEvent(Product product, std::string_view event_rlz) override;
  bool ClearAllProductEvents(Product product) override;

  bool AddStatefulEvent(Product product, std::string_view event_rlz) override;
  bool IsStatefulEvent(Product product, std::string_view event_rlz) override;
  bool ClearAllStatefulEvents(Product product) override;

  void CollectGarbage() override;

 private:
  RlzValueStoreRegistry() {}
  friend class ScopedRlzValueStoreLock;
};

}  // namespace rlz_lib

#endif  // RLZ_WIN_LIB_RLZ_VALUE_STORE_REGISTRY_H_

