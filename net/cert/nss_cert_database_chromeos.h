// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_NSS_CERT_DATABASE_CHROMEOS_H_
#define NET_CERT_NSS_CERT_DATABASE_CHROMEOS_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/net_export.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/nss_profile_filter_chromeos.h"

namespace net {

class NET_EXPORT NSSCertDatabaseChromeOS : public NSSCertDatabase {
 public:
  NSSCertDatabaseChromeOS(crypto::ScopedPK11Slot public_slot,
                          crypto::ScopedPK11Slot private_slot);
  ~NSSCertDatabaseChromeOS() override;

  // |system_slot| is the system TPM slot, which is only enabled for certain
  // users.
  void SetSystemSlot(crypto::ScopedPK11Slot system_slot);

  // NSSCertDatabase implementation.
  void ListCerts(NSSCertDatabase::ListCertsCallback callback) override;
  void ListModules(std::vector<crypto::ScopedPK11Slot>* modules,
                   bool need_rw) const override;
  crypto::ScopedPK11Slot GetSystemSlot() const override;

  // TODO(mattm): handle trust setting, deletion, etc correctly when certs exist
  // in multiple slots.
  // TODO(mattm): handle trust setting correctly for certs in read-only slots.

 private:
  // Certificate listing implementation used by |ListCerts|.
  // The certificate list normally returned by NSSCertDatabase::ListCertsImpl
  // is additionally filtered by |profile_filter|.
  // Static so it may safely be used on the worker thread.
  static ScopedCERTCertificateList ListCertsImpl(
      const NSSProfileFilterChromeOS& profile_filter);

  NSSProfileFilterChromeOS profile_filter_;
  crypto::ScopedPK11Slot system_slot_;

  DISALLOW_COPY_AND_ASSIGN(NSSCertDatabaseChromeOS);
};

}  // namespace net

#endif  // NET_CERT_NSS_CERT_DATABASE_CHROMEOS_H_
