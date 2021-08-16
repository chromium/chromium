/*
 * NSS utility functions
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "net/third_party/mozilla_win/cert/win_util.h"

#include "base/win/wincrypt_shim.h"
#include "crypto/scoped_capi_types.h"

namespace net {

void GatherEnterpriseCertsForLocation(HCERTSTORE cert_store,
                                      DWORD location,
                                      LPCWSTR store_name) {
  if (!(location == CERT_SYSTEM_STORE_LOCAL_MACHINE ||
        location == CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY ||
        location == CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE ||
        location == CERT_SYSTEM_STORE_CURRENT_USER ||
        location == CERT_SYSTEM_STORE_CURRENT_USER_GROUP_POLICY)) {
    return;
  }

  DWORD flags =
      location | CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG;

  crypto::ScopedHCERTSTORE enterprise_root_store(CertOpenStore(
      CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, NULL, flags, store_name));
  if (!enterprise_root_store.get()) {
    return;
  }
  // Priority of the opened cert store in the collection does not matter, so set
  // everything to priority 0.
  CertAddStoreToCollection(cert_store, enterprise_root_store.get(),
                           /*dwUpdateFlags=*/0, /*dwPriority=*/0);
}

}  // namespace net
