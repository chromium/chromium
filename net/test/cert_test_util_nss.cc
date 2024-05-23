// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/test/cert_test_util.h"

#include <certdb.h>
#include <pk11pub.h>
#include <secmod.h>
#include <secmodt.h>
#include <string.h>

#include <memory>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "crypto/nss_key_util.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/cert_type.h"
#include "net/cert/x509_util_nss.h"

namespace net {

namespace {

// IsKnownRoot returns true if the given certificate is one that we believe
// is a standard (as opposed to user-installed) root.
bool IsKnownRoot(CERTCertificate* root) {
  if (!root || !root->slot) {
    return false;
  }

  // Historically, the set of root certs was determined based on whether or
  // not it was part of nssckbi.[so,dll], the read-only PKCS#11 module that
  // exported the certs with trust settings. However, some distributions,
  // notably those in the Red Hat family, replace nssckbi with a redirect to
  // their own store, such as from p11-kit, which can support more robust
  // trust settings, like per-system trust, admin-defined, and user-defined
  // trust.
  //
  // As a given certificate may exist in multiple modules and slots, scan
  // through all of the available modules, all of the (connected) slots on
  // those modules, and check to see if it has the CKA_NSS_MOZILLA_CA_POLICY
  // attribute set. This attribute indicates it's from the upstream Mozilla
  // trust store, and these distributions preserve the attribute as a flag.
  crypto::AutoSECMODListReadLock lock_id;
  for (const SECMODModuleList* item = SECMOD_GetDefaultModuleList();
       item != nullptr; item = item->next) {
    for (int i = 0; i < item->module->slotCount; ++i) {
      PK11SlotInfo* slot = item->module->slots[i];
      if (PK11_IsPresent(slot) && PK11_HasRootCerts(slot)) {
        CK_OBJECT_HANDLE handle = PK11_FindCertInSlot(slot, root, nullptr);
        if (handle != CK_INVALID_HANDLE &&
            PK11_HasAttributeSet(slot, handle, CKA_NSS_MOZILLA_CA_POLICY,
                                 PR_FALSE) == CK_TRUE) {
          return true;
        }
      }
    }
  }

  return false;
}

// Returns true if the provided slot looks like it contains built-in root.
bool IsNssBuiltInRootSlot(PK11SlotInfo* slot) {
  if (!PK11_IsPresent(slot) || !PK11_HasRootCerts(slot)) {
    return false;
  }
  crypto::ScopedCERTCertList cert_list(PK11_ListCertsInSlot(slot));
  if (!cert_list) {
    return false;
  }
  bool built_in_cert_found = false;
  for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
       !CERT_LIST_END(node, cert_list); node = CERT_LIST_NEXT(node)) {
    if (IsKnownRoot(node->cert)) {
      built_in_cert_found = true;
      break;
    }
  }
  return built_in_cert_found;
}

// Returns the slot which holds the built-in root certificates.
crypto::ScopedPK11Slot GetNssBuiltInRootCertsSlot() {
  crypto::AutoSECMODListReadLock auto_lock;
  SECMODModuleList* head = SECMOD_GetDefaultModuleList();
  for (SECMODModuleList* item = head; item != nullptr; item = item->next) {
    int slot_count = item->module->loaded ? item->module->slotCount : 0;
    for (int i = 0; i < slot_count; i++) {
      PK11SlotInfo* slot = item->module->slots[i];
      if (IsNssBuiltInRootSlot(slot)) {
        return crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot));
      }
    }
  }
  return crypto::ScopedPK11Slot();
}

}  // namespace

bool ImportSensitiveKeyFromFile(const base::FilePath& dir,
                                std::string_view key_filename,
                                PK11SlotInfo* slot) {
  base::FilePath key_path = dir.AppendASCII(key_filename);
  std::string key_pkcs8;
  bool success = base::ReadFileToString(key_path, &key_pkcs8);
  if (!success) {
    LOG(ERROR) << "Failed to read file " << key_path.value();
    return false;
  }

  crypto::ScopedSECKEYPrivateKey private_key(
      crypto::ImportNSSKeyFromPrivateKeyInfo(slot,
                                             base::as_byte_span(key_pkcs8),
                                             /*permanent=*/true));
  LOG_IF(ERROR, !private_key)
      << "Could not create key from file " << key_path.value();
  return !!private_key;
}

bool ImportClientCertToSlot(CERTCertificate* nss_cert, PK11SlotInfo* slot) {
  std::string nickname =
      x509_util::GetDefaultUniqueNickname(nss_cert, USER_CERT, slot);
  SECStatus rv = PK11_ImportCert(slot, nss_cert, CK_INVALID_HANDLE,
                                 nickname.c_str(), PR_FALSE);
  if (rv != SECSuccess) {
    LOG(ERROR) << "Could not import cert";
    return false;
  }
  return true;
}

ScopedCERTCertificate ImportClientCertToSlot(
    const scoped_refptr<X509Certificate>& cert,
    PK11SlotInfo* slot) {
  ScopedCERTCertificate nss_cert =
      x509_util::CreateCERTCertificateFromX509Certificate(cert.get());
  if (!nss_cert)
    return nullptr;

  if (!ImportClientCertToSlot(nss_cert.get(), slot))
    return nullptr;

  return nss_cert;
}

scoped_refptr<X509Certificate> ImportClientCertAndKeyFromFile(
    const base::FilePath& dir,
    std::string_view cert_filename,
    std::string_view key_filename,
    PK11SlotInfo* slot,
    ScopedCERTCertificate* nss_cert) {
  if (!ImportSensitiveKeyFromFile(dir, key_filename, slot)) {
    LOG(ERROR) << "Could not import private key from file " << key_filename;
    return nullptr;
  }

  scoped_refptr<X509Certificate> cert(ImportCertFromFile(dir, cert_filename));

  if (!cert.get()) {
    LOG(ERROR) << "Failed to parse cert from file " << cert_filename;
    return nullptr;
  }

  *nss_cert = ImportClientCertToSlot(cert, slot);
  if (!*nss_cert)
    return nullptr;

  // |cert| continues to point to the original X509Certificate before the
  // import to |slot|. However this should not make a difference as NSS handles
  // state globally.
  return cert;
}

scoped_refptr<X509Certificate> ImportClientCertAndKeyFromFile(
    const base::FilePath& dir,
    std::string_view cert_filename,
    std::string_view key_filename,
    PK11SlotInfo* slot) {
  ScopedCERTCertificate nss_cert;
  return ImportClientCertAndKeyFromFile(dir, cert_filename, key_filename, slot,
                                        &nss_cert);
}

ScopedCERTCertificate ImportCERTCertificateFromFile(
    const base::FilePath& certs_dir,
    std::string_view cert_file) {
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir, cert_file);
  if (!cert)
    return nullptr;
  return x509_util::CreateCERTCertificateFromX509Certificate(cert.get());
}

ScopedCERTCertificateList CreateCERTCertificateListFromFile(
    const base::FilePath& certs_dir,
    std::string_view cert_file,
    int format) {
  CertificateList certs =
      CreateCertificateListFromFile(certs_dir, cert_file, format);
  ScopedCERTCertificateList nss_certs;
  for (const auto& cert : certs) {
    ScopedCERTCertificate nss_cert =
        x509_util::CreateCERTCertificateFromX509Certificate(cert.get());
    if (!nss_cert)
      return {};
    nss_certs.push_back(std::move(nss_cert));
  }
  return nss_certs;
}

ScopedCERTCertificate GetAnNssBuiltinSslTrustedRoot() {
  crypto::ScopedPK11Slot root_certs_slot = GetNssBuiltInRootCertsSlot();
  if (!root_certs_slot) {
    return nullptr;
  }

  scoped_refptr<X509Certificate> ssl_trusted_root;

  crypto::ScopedCERTCertList cert_list(
      PK11_ListCertsInSlot(root_certs_slot.get()));
  if (!cert_list) {
    return nullptr;
  }
  for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
       !CERT_LIST_END(node, cert_list); node = CERT_LIST_NEXT(node)) {
    CERTCertTrust trust;
    if (CERT_GetCertTrust(node->cert, &trust) != SECSuccess) {
      continue;
    }
    int trust_flags = SEC_GET_TRUST_FLAGS(&trust, trustSSL);
    if ((trust_flags & CERTDB_TRUSTED_CA) == CERTDB_TRUSTED_CA) {
      return x509_util::DupCERTCertificate(node->cert);
    }
  }

  return nullptr;
}

}  // namespace net
