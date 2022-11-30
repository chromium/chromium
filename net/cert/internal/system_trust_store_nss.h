// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_SYSTEM_TRUST_STORE_NSS_H_
#define NET_CERT_INTERNAL_SYSTEM_TRUST_STORE_NSS_H_

#include "crypto/scoped_nss_types.h"
#include "net/base/net_export.h"
#include "net/cert/internal/system_trust_store.h"

namespace net {

// Create a SystemTrustStore that will accept trust for:
// (*) built-in certificates
// (*) certificates stored on the |user_slot_restriction|, if non-null.
NET_EXPORT std::unique_ptr<SystemTrustStore>
CreateSslSystemTrustStoreNSSWithUserSlotRestriction(
    crypto::ScopedPK11Slot user_slot_restriction);

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
// Create a SystemTrustStore that will accept trust for:
// (*) Chrome Root Store certificates
// (*) certificates stored on the |user_slot_restriction|, if non-null.
NET_EXPORT std::unique_ptr<SystemTrustStore>
CreateSslSystemTrustStoreChromeRootWithUserSlotRestriction(
    std::unique_ptr<TrustStoreChrome> chrome_root,
    crypto::ScopedPK11Slot user_slot_restriction);
#endif

}  // namespace net

#endif  // NET_CERT_INTERNAL_SYSTEM_TRUST_STORE_NSS_H_
