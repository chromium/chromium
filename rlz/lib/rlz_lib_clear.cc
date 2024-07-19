// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "rlz/lib/rlz_lib_clear.h"

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

void ClearProductState(Product product, const AccessPoint* access_points) {
  ScopedRlzValueStoreLock lock;
  RlzValueStore* store = lock.GetStore();
  if (!store || !store->HasAccess(RlzValueStore::kWriteAccess))
    return;

  // Delete all product specific state.
  VERIFY(ClearAllProductEvents(product));
  VERIFY(store->ClearPingTime(product));

  // Delete all RLZ's for access points being uninstalled.
  if (access_points) {
    for (int i = 0; access_points[i] != NO_ACCESS_POINT; i++) {
      VERIFY(store->ClearAccessPointRlz(access_points[i]));
    }
  }

  store->CollectGarbage();
}

}  // namespace rlz_lib
