// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/nss_cert_database_chromeos.h"

#include <cert.h>
#include <pk11pub.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "net/cert/nss_cert_database.h"

namespace net {

NSSCertDatabaseChromeOS::NSSCertDatabaseChromeOS(
    crypto::ScopedPK11Slot public_slot,
    crypto::ScopedPK11Slot private_slot)
    : NSSCertDatabase(std::move(public_slot), std::move(private_slot)) {
  // By default, don't use a system slot. Only if explicitly set by
  // SetSystemSlot, the system slot will be used.
  profile_filter_.Init(GetPublicSlot(),
                       GetPrivateSlot(),
                       crypto::ScopedPK11Slot() /* no system slot */);
}

NSSCertDatabaseChromeOS::~NSSCertDatabaseChromeOS() = default;

void NSSCertDatabaseChromeOS::SetSystemSlot(
    crypto::ScopedPK11Slot system_slot) {
  system_slot_ = std::move(system_slot);
  profile_filter_.Init(GetPublicSlot(), GetPrivateSlot(), GetSystemSlot());
}

void NSSCertDatabaseChromeOS::ListCerts(
    NSSCertDatabase::ListCertsCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&NSSCertDatabaseChromeOS::ListCertsImpl, profile_filter_),
      std::move(callback));
}

void NSSCertDatabaseChromeOS::ListCertsInfo(
    ListCertsInfoCallback callback,
    NSSRootsHandling nss_roots_handling) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&NSSCertDatabaseChromeOS::ListCertsInfoImpl,
                     profile_filter_, /*slot=*/GetSystemSlot(),
                     /*add_certs_info=*/true, nss_roots_handling),
      std::move(callback));
}

crypto::ScopedPK11Slot NSSCertDatabaseChromeOS::GetSystemSlot() const {
  if (system_slot_)
    return crypto::ScopedPK11Slot(PK11_ReferenceSlot(system_slot_.get()));
  return crypto::ScopedPK11Slot();
}

void NSSCertDatabaseChromeOS::ListModules(
    std::vector<crypto::ScopedPK11Slot>* modules,
    bool need_rw) const {
  NSSCertDatabase::ListModules(modules, need_rw);

  const NSSProfileFilterChromeOS& profile_filter = profile_filter_;
  std::erase_if(*modules, [&profile_filter](crypto::ScopedPK11Slot& module) {
    return !profile_filter.IsModuleAllowed(module.get());
  });
}

bool NSSCertDatabaseChromeOS::SetCertTrust(CERTCertificate* cert,
                                           CertType type,
                                           TrustBits trust_bits) {
  crypto::ScopedPK11Slot public_slot = GetPublicSlot();

  // Ensure that the certificate exists on the public slot so NSS puts the trust
  // settings there (https://crbug.com/1132030).
  if (public_slot == GetSystemSlot()) {
    // Never attempt to store trust setting on the system slot.
    return false;
  }

  if (!IsCertificateOnSlot(cert, public_slot.get())) {
    // Copy the certificate to the public slot.
    SECStatus srv =
        PK11_ImportCert(public_slot.get(), cert, CK_INVALID_HANDLE,
                        cert->nickname, PR_FALSE /* includeTrust (unused) */);
    if (srv != SECSuccess) {
      LOG(ERROR) << "Failed to import certificate onto public slot.";
      return false;
    }
  }
  return NSSCertDatabase::SetCertTrust(cert, type, trust_bits);
}

// static
ScopedCERTCertificateList NSSCertDatabaseChromeOS::ListCertsImpl(
    const NSSProfileFilterChromeOS& profile_filter) {
  CertInfoList certs_info =
      ListCertsInfoImpl(profile_filter, crypto::ScopedPK11Slot(),
                        /*add_certs_info=*/false, NSSRootsHandling::kInclude);

  return ExtractCertificates(std::move(certs_info));
}

// static
NSSCertDatabase::CertInfoList NSSCertDatabaseChromeOS::ListCertsInfoImpl(
    const NSSProfileFilterChromeOS& profile_filter,
    crypto::ScopedPK11Slot system_slot,
    bool add_certs_info,
    NSSRootsHandling nss_roots_handling) {
  // This method may acquire the NSS lock or reenter this code via extension
  // hooks (such as smart card UI). To ensure threads are not starved or
  // deadlocked, the base::ScopedBlockingCall below increments the thread pool
  // capacity if this method takes too much time to run.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  CertInfoList certs_info(NSSCertDatabase::ListCertsInfoImpl(
      crypto::ScopedPK11Slot(), add_certs_info, nss_roots_handling));

  // Filter certificate information according to user profile.
  std::erase_if(certs_info, [&profile_filter](CertInfo& cert_info) {
    return !profile_filter.IsCertAllowed(cert_info.cert.get());
  });

  if (add_certs_info) {
    // Add Chrome OS specific information.
    for (auto& cert_info : certs_info) {
      cert_info.device_wide =
          IsCertificateOnSlot(cert_info.cert.get(), system_slot.get());
      cert_info.hardware_backed = IsHardwareBacked(cert_info.cert.get());
    }
  }

  return certs_info;
}

}  // namespace net
