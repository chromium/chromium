// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RLZ_WIN_LIB_RLZ_VALUE_STORE_REGISTRY_H_
#define RLZ_WIN_LIB_RLZ_VALUE_STORE_REGISTRY_H_

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "rlz/lib/rlz_value_store.h"

namespace rlz_lib {

// Implements RlzValueStore by storing values in the windows registry.
class RlzValueStoreRegistry : public RlzValueStore {
 public:
  static std::wstring GetWideLibKeyName();

  bool HasAccess(AccessType type) override;

  bool WritePingTime(Product product, int64_t time) override;
  bool ReadPingTime(Product product, int64_t* time) override;
  bool ClearPingTime(Product product) override;

  bool WriteAccessPointRlz(AccessPoint access_point,
                                   const char* new_rlz) override;
  bool ReadAccessPointRlz(AccessPoint access_point,
                                  char* rlz,
                                  size_t rlz_size) override;
  bool ClearAccessPointRlz(AccessPoint access_point) override;
  bool UpdateExistingAccessPointRlz(const std::string& brand) override;

  bool AddProductEvent(Product product, const char* event_rlz) override;
  bool ReadProductEvents(Product product,
                                 std::vector<std::string>* events) override;
  bool ClearProductEvent(Product product,
                                 const char* event_rlz) override;
  bool ClearAllProductEvents(Product product) override;

  bool AddStatefulEvent(Product product,
                                const char* event_rlz) override;
  bool IsStatefulEvent(Product product,
                               const char* event_rlz) override;
  bool ClearAllStatefulEvents(Product product) override;

  void CollectGarbage() override;

 private:
  RlzValueStoreRegistry() {}
  DISALLOW_COPY_AND_ASSIGN(RlzValueStoreRegistry);
  friend class ScopedRlzValueStoreLock;
};

}  // namespace rlz_lib

#endif  // RLZ_WIN_LIB_RLZ_VALUE_STORE_REGISTRY_H_

