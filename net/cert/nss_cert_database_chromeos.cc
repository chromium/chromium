// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/nss_cert_database_chromeos.h"

#include <cert.h>
#include <certdb.h>
#include <pk11pub.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/threading/scoped_blocking_call.h"

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

ScopedCERTCertificateList NSSCertDatabaseChromeOS::ListCertsSync() {
  LogUserCertificates("ListCertsSync");
  return ListCertsImpl(profile_filter_);
}

void NSSCertDatabaseChromeOS::ListCerts(
    const NSSCertDatabase::ListCertsCallback& callback) {
  LogUserCertificates("ListCerts");
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::Bind(&NSSCertDatabaseChromeOS::ListCertsImpl, profile_filter_),
      callback);
}

crypto::ScopedPK11Slot NSSCertDatabaseChromeOS::GetSystemSlot() const {
  if (system_slot_)
    return crypto::ScopedPK11Slot(PK11_ReferenceSlot(system_slot_.get()));
  return crypto::ScopedPK11Slot();
}

void NSSCertDatabaseChromeOS::LogUserCertificates(
    const std::string& log_reason) const {
  // Unit tests may not have a TaskScheduler instance.
  if (!base::TaskScheduler::GetInstance())
    return;

  crypto::ScopedPK11Slot system_slot(GetSystemSlot());

  base::PostTaskWithTraits(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&NSSCertDatabaseChromeOS::LogUserCertificatesImpl,
                     log_reason, base::Passed(&system_slot)));
}

void NSSCertDatabaseChromeOS::ListModules(
    std::vector<crypto::ScopedPK11Slot>* modules,
    bool need_rw) const {
  NSSCertDatabase::ListModules(modules, need_rw);

  size_t pre_size = modules->size();
  const NSSProfileFilterChromeOS& profile_filter = profile_filter_;
  base::EraseIf(*modules, [&profile_filter](crypto::ScopedPK11Slot& module) {
    return !profile_filter.IsModuleAllowed(module.get());
  });
  DVLOG(1) << "filtered " << pre_size - modules->size() << " of " << pre_size
           << " modules";
}

ScopedCERTCertificateList NSSCertDatabaseChromeOS::ListCertsImpl(
    const NSSProfileFilterChromeOS& profile_filter) {
  // This method may acquire the NSS lock or reenter this code via extension
  // hooks (such as smart card UI). To ensure threads are not starved or
  // deadlocked, the base::ScopedBlockingCall below increments the thread pool
  // capacity if this method takes too much time to run.
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);

  ScopedCERTCertificateList certs(
      NSSCertDatabase::ListCertsImpl(crypto::ScopedPK11Slot()));

  size_t pre_size = certs.size();
  base::EraseIf(certs, [&profile_filter](ScopedCERTCertificate& cert) {
    return !profile_filter.IsCertAllowed(cert.get());
  });
  DVLOG(1) << "filtered " << pre_size - certs.size() << " of " << pre_size
           << " certs";
  return certs;
}

// static
void NSSCertDatabaseChromeOS::LogUserCertificatesImpl(
    const std::string& log_reason,
    crypto::ScopedPK11Slot system_slot) {
  // See ListCertsImpl for details on why we use |MAY_BLOCK|.
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);

  bool system_slot_present = system_slot != nullptr;
  VLOG(0) << "UserCertLogging: Invoked with log_reason=" << log_reason
          << ", system_slot_present=" << system_slot_present;

  CERTCertList* cert_list = PK11_ListCerts(PK11CertListUnique, nullptr);
  CERTCertListNode* node;
  for (node = CERT_LIST_HEAD(cert_list); !CERT_LIST_END(node, cert_list);
       node = CERT_LIST_NEXT(node)) {
    CERTCertificate* cert = node->cert;

    // Skip if the certificate is not a user certificate, or if this can't be
    // determined.
    CERTCertTrust nss_trust;
    SECStatus rv = CERT_GetCertTrust(cert, &nss_trust);
    if (rv != SECSuccess) {
      LOG(ERROR) << "CERT_GetCertTrust failed with error " << PORT_GetError();
      continue;
    }
    unsigned int all_flags = nss_trust.sslFlags | nss_trust.emailFlags |
                             nss_trust.objectSigningFlags;
    // This logic is from |mozilla_security_manager::GetCertType|.
    bool is_user_cert = cert->nickname && (all_flags & CERTDB_USER);
    if (!is_user_cert)
      continue;

    // Get names from the certificate. Only log issuer CommonName for now.
    std::string issuer_name = GetCertIssuerCommonName(cert);

    // Get information about the slot the certificate is on.
    PK11SlotInfo* slot = cert->slot;
    bool is_hardware_backed = slot && PK11_IsHW(slot);
    bool is_on_system_slot = slot == system_slot.get();
    int cert_slot_id = static_cast<int>(PK11_GetSlotID(slot));

    // Get information about the corresponding private key: the slot id it is on
    // and the pkcs11 object ID.
    int key_slot_id = -1;
    std::string key_pkcs11_id;
    SECKEYPrivateKey* private_key =
        PK11_FindKeyByAnyCert(cert, nullptr /* wincx */);
    if (private_key) {
      key_slot_id = static_cast<int>(PK11_GetSlotID(private_key->pkcs11Slot));
      // Get the CKA_ID attribute for a key.
      SECItem* sec_item = PK11_GetLowLevelKeyIDForPrivateKey(private_key);
      if (sec_item) {
        key_pkcs11_id = base::HexEncode(sec_item->data, sec_item->len);
        SECITEM_FreeItem(sec_item, PR_TRUE);
      }
      SECKEY_DestroyPrivateKey(private_key);
    }

    VLOG(0) << "UserCertLogging: Cert with issuer=" << issuer_name
            << ", cert_slot_id=" << cert_slot_id
            << ", is_hw_backed=" << is_hardware_backed
            << ", is_on_system_slot=" << is_on_system_slot
            << ", key_slot_id=" << key_slot_id
            << ", key_pkcs11_id=" << key_pkcs11_id;
  }
  CERT_DestroyCertList(cert_list);
}

}  // namespace net
