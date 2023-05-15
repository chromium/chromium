// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_NSS_CERT_DATABASE_CHROMEOS_H_
#define NET_CERT_NSS_CERT_DATABASE_CHROMEOS_H_

#include "base/functional/callback.h"
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

  NSSCertDatabaseChromeOS(const NSSCertDatabaseChromeOS&) = delete;
  NSSCertDatabaseChromeOS& operator=(const NSSCertDatabaseChromeOS&) = delete;

  ~NSSCertDatabaseChromeOS() override;

  // |system_slot| is the system TPM slot, which is only enabled for certain
  // users.
  void SetSystemSlot(crypto::ScopedPK11Slot system_slot);

  // NSSCertDatabase implementation.
  void ListCerts(NSSCertDatabase::ListCertsCallback callback) override;

  // Uses NSSCertDatabase implementation and adds additional Chrome OS specific
  // certificate information.
  void ListCertsInfo(ListCertsInfoCallback callback,
                     NSSRootsHandling nss_roots_handling) override;

  crypto::ScopedPK11Slot GetSystemSlot() const override;

  void ListModules(std::vector<crypto::ScopedPK11Slot>* modules,
                   bool need_rw) const override;
  bool SetCertTrust(CERTCertificate* cert,
                    CertType type,
                    TrustBits trust_bits) override;

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

  // Certificate information listing implementation used by |ListCertsInfo|.
  // The certificate list normally returned by
  // NSSCertDatabase::ListCertsInfoImpl is additionally filtered by
  // |profile_filter|. Also additional Chrome OS specific information is added.
  // Static so it may safely be used on the worker thread.
  static CertInfoList ListCertsInfoImpl(
      const NSSProfileFilterChromeOS& profile_filter,
      crypto::ScopedPK11Slot system_slot,
      bool add_certs_info,
      NSSRootsHandling nss_roots_handling);

  NSSProfileFilterChromeOS profile_filter_;
  crypto::ScopedPK11Slot system_slot_;
};

}  // namespace net

#endif  // NET_CERT_NSS_CERT_DATABASE_CHROMEOS_H_
