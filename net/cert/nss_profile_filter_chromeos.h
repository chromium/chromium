// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_NSS_PROFILE_FILTER_CHROMEOS_H_
#define NET_CERT_NSS_PROFILE_FILTER_CHROMEOS_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/net_export.h"
#include "net/cert/scoped_nss_types.h"

namespace net {

// On ChromeOS each user has separate NSS databases, which are loaded
// simultaneously when multiple users are logged in at the same time. NSS
// doesn't have built-in support to partition databases into separate groups, so
// NSSProfileFilterChromeOS can be used to check if a given slot or certificate
// should be used for a given user.
//
// Objects of this class are thread-safe except for the Init function, which if
// called must not be called while other threads could access the object.
class NET_EXPORT NSSProfileFilterChromeOS {
 public:
  // Create a filter. Until Init is called (or if Init is called with NULL
  // slot handles), the filter will allow only certs/slots from the read-only
  // slots and the root CA module.
  NSSProfileFilterChromeOS();
  NSSProfileFilterChromeOS(const NSSProfileFilterChromeOS& other);
  ~NSSProfileFilterChromeOS();

  NSSProfileFilterChromeOS& operator=(const NSSProfileFilterChromeOS& other);

  // Initialize the filter with the slot handles to allow. This method is not
  // thread-safe.
  void Init(crypto::ScopedPK11Slot public_slot,
            crypto::ScopedPK11Slot private_slot,
            crypto::ScopedPK11Slot system_slot);

  bool IsModuleAllowed(PK11SlotInfo* slot) const;
  bool IsCertAllowed(CERTCertificate* cert) const;

 private:
  crypto::ScopedPK11Slot public_slot_;
  crypto::ScopedPK11Slot private_slot_;
  crypto::ScopedPK11Slot system_slot_;
};

}  // namespace net

#endif  // NET_CERT_NSS_PROFILE_FILTER_CHROMEOS_H_
