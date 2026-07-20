// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/lib/rlz_lib_clear.h"

#include "base/containers/span.h"
#include "rlz/lib/assert.h"
#include "rlz/lib/rlz_value_store.h"

namespace rlz_lib {

bool ClearAllProductEvents(Product product) {
  ScopedRlzValueStoreLock lock;
  RlzValueStore* store = lock.GetStore();
  if (!store || !store->HasAccess(RlzValueStore::kWriteAccess))
    return false;

  bool result;
  result = store->ClearAllProductEvents(product);
  result &= store->ClearAllStatefulEvents(product);
  return result;
}

void ClearProductState(Product product,
                       base::span<const AccessPoint> access_points) {
  ScopedRlzValueStoreLock lock;
  RlzValueStore* store = lock.GetStore();
  if (!store || !store->HasAccess(RlzValueStore::kWriteAccess))
    return;

  // Delete all product specific state.
  VERIFY(ClearAllProductEvents(product));
  VERIFY(store->ClearPingTime(product));

  // Delete all RLZ's for access points being uninstalled.
  for (AccessPoint point : access_points) {
    VERIFY(store->ClearAccessPointRlz(point));
  }

  store->CollectGarbage();
}

}  // namespace rlz_lib
