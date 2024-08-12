// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/nss_cert_database.h"

#include <cert.h>
#include <certdb.h>
#include <certt.h>
#include <dlfcn.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <secmod.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_database.h"
#include "net/cert/internal/trust_store_nss.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "net/third_party/mozilla_security_manager/nsNSSCertificateDB.h"
#include "net/third_party/mozilla_security_manager/nsPKCS12Blob.h"

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "crypto/chaps_support.h"
#endif

// PSM = Mozilla's Personal Security Manager.
namespace psm = mozilla_security_manager;

namespace net {

namespace {

using PK11HasAttributeSetFunction = CK_BBOOL (*)(PK11SlotInfo* slot,
                                                 CK_OBJECT_HANDLE id,
                                                 CK_ATTRIBUTE_TYPE type,
                                                 PRBool haslock);

// TODO(pneubeck): Move this class out of NSSCertDatabase and to the caller of
// the c'tor of NSSCertDatabase, see https://crbug.com/395983 .
// Helper that observes events from the NSSCertDatabase and forwards them to
// the given CertDatabase.
class CertNotificationForwarder : public NSSCertDatabase::Observer {
 public:
  explicit CertNotificationForwarder(CertDatabase* cert_db)
      : cert_db_(cert_db) {}

  CertNotificationForwarder(const CertNotificationForwarder&) = delete;
  CertNotificationForwarder& operator=(const CertNotificationForwarder&) =
      delete;

  ~CertNotificationForwarder() override = default;

  void OnTrustStoreChanged() override {
    cert_db_->NotifyObserversTrustStoreChanged();
  }
  void OnClientCertStoreChanged() override {
    cert_db_->NotifyObserversClientCertStoreChanged();
  }

 private:
  raw_ptr<CertDatabase> cert_db_;
};

// TODO(crbug.com/40890963): once the other IsUntrusted impl is deleted,
// rename this.
bool IsUntrustedUsingTrustStore(const CERTCertificate* cert,
                                bssl::CertificateTrust trust) {
  if (trust.IsDistrusted()) {
    return true;
  }

  // Self-signed certificates that don't have any trust bits set are untrusted.
  // Other certificates that don't have any trust bits set may still be trusted
  // if they chain up to a trust anchor.
  // TODO(mattm): this is weird, but just match the behavior of the existing
  // IsUntrusted function for now.
  if (SECITEM_CompareItem(&cert->derIssuer, &cert->derSubject) == SECEqual) {
    return !trust.IsTrustAnchor();
  }

  return false;
}

}  // namespace

NSSCertDatabase::CertInfo::CertInfo() = default;
NSSCertDatabase::CertInfo::CertInfo(CertInfo&& other) = default;
NSSCertDatabase::CertInfo::~CertInfo() = default;
NSSCertDatabase::CertInfo& NSSCertDatabase::CertInfo::operator=(
    NSSCertDatabase::CertInfo&& other) = default;

NSSCertDatabase::ImportCertFailure::ImportCertFailure(
    ScopedCERTCertificate cert,
    int err)
    : certificate(std::move(cert)), net_error(err) {}

NSSCertDatabase::ImportCertFailure::ImportCertFailure(
    ImportCertFailure&& other) = default;

NSSCertDatabase::ImportCertFailure::~ImportCertFailure() = default;

NSSCertDatabase::NSSCertDatabase(crypto::ScopedPK11Slot public_slot,
                                 crypto::ScopedPK11Slot private_slot)
    : public_slot_(std::move(public_slot)),
      private_slot_(std::move(private_slot)),
      observer_list_(
          base::MakeRefCounted<base::ObserverListThreadSafe<Observer>>()) {
  CHECK(public_slot_);

  CertDatabase* cert_db = CertDatabase::GetInstance();
  cert_notification_forwarder_ =
      std::make_unique<CertNotificationForwarder>(cert_db);
  AddObserver(cert_notification_forwarder_.get());

  psm::EnsurePKCS12Init();
}

NSSCertDatabase::~NSSCertDatabase() = default;

void NSSCertDatabase::ListCerts(ListCertsCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&NSSCertDatabase::ListCertsImpl, crypto::ScopedPK11Slot()),
      std::move(callback));
}

void NSSCertDatabase::ListCertsInSlot(ListCertsCallback callback,
                                      PK11SlotInfo* slot) {
  DCHECK(slot);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&NSSCertDatabase::ListCertsImpl,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot))),
      std::move(callback));
}

void NSSCertDatabase::ListCertsInfo(ListCertsInfoCallback callback,
                                    NSSRootsHandling nss_roots_handling) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&NSSCertDatabase::ListCertsInfoImpl,
                     /*slot=*/nullptr,
                     /*add_certs_info=*/true, nss_roots_handling),
      std::move(callback));
}

#if BUILDFLAG(IS_CHROMEOS)
crypto::ScopedPK11Slot NSSCertDatabase::GetSystemSlot() const {
  return crypto::ScopedPK11Slot();
}

// static
bool NSSCertDatabase::IsCertificateOnSlot(CERTCertificate* cert,
                                          PK11SlotInfo* slot) {
  if (!slot)
    return false;

  return PK11_FindCertInSlot(slot, cert, nullptr) != CK_INVALID_HANDLE;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

crypto::ScopedPK11Slot NSSCertDatabase::GetPublicSlot() const {
  return crypto::ScopedPK11Slot(PK11_ReferenceSlot(public_slot_.get()));
}

crypto::ScopedPK11Slot NSSCertDatabase::GetPrivateSlot() const {
  if (!private_slot_)
    return crypto::ScopedPK11Slot();
  return crypto::ScopedPK11Slot(PK11_ReferenceSlot(private_slot_.get()));
}

void NSSCertDatabase::ListModules(std::vector<crypto::ScopedPK11Slot>* modules,
                                  bool need_rw) const {
  modules->clear();

  // The wincx arg is unused since we don't call PK11_SetIsLoggedInFunc.
  crypto::ScopedPK11SlotList slot_list(
      PK11_GetAllTokens(CKM_INVALID_MECHANISM,
                        need_rw ? PR_TRUE : PR_FALSE,  // needRW
                        PR_TRUE,                       // loadCerts (unused)
                        nullptr));                     // wincx
  if (!slot_list) {
    LOG(ERROR) << "PK11_GetAllTokens failed: " << PORT_GetError();
    return;
  }

  PK11SlotListElement* slot_element = PK11_GetFirstSafe(slot_list.get());
  while (slot_element) {
    modules->push_back(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_element->slot)));
    slot_element = PK11_GetNextSafe(slot_list.get(), slot_element,
                                    PR_FALSE);  // restart
  }
}

bool NSSCertDatabase::SetCertTrust(CERTCertificate* cert,
                                   CertType type,
                                   TrustBits trust_bits) {
  bool success = psm::SetCertTrust(cert, type, trust_bits);
  if (success) {
    NotifyObserversTrustStoreChanged();
  }

  return success;
}

int NSSCertDatabase::ImportFromPKCS12(
    PK11SlotInfo* slot_info,
    const std::string& data,
    const std::u16string& password,
    bool is_extractable,
    ScopedCERTCertificateList* imported_certs) {
  int result =
      psm::nsPKCS12Blob_Import(slot_info, data.data(), data.size(), password,
                               is_extractable, imported_certs);
  if (result == OK) {
    NotifyObserversClientCertStoreChanged();
  }

  return result;
}

// static
int NSSCertDatabase::ExportToPKCS12(const ScopedCERTCertificateList& certs,
                                    const std::u16string& password,
                                    std::string* output) {
  return psm::nsPKCS12Blob_Export(output, certs, password);
}

CERTCertificate* NSSCertDatabase::FindRootInList(
    const ScopedCERTCertificateList& certificates) const {
  DCHECK_GT(certificates.size(), 0U);

  if (certificates.size() == 1)
    return certificates[0].get();

  CERTCertificate* cert0 = certificates[0].get();
  CERTCertificate* cert1 = certificates[1].get();
  CERTCertificate* certn_2 = certificates[certificates.size() - 2].get();
  CERTCertificate* certn_1 = certificates[certificates.size() - 1].get();

  // Using CERT_CompareName is an alternative, except that it is broken until
  // NSS 3.32 (see https://bugzilla.mozilla.org/show_bug.cgi?id=1361197 ).
  if (SECITEM_CompareItem(&cert1->derIssuer, &cert0->derSubject) == SECEqual)
    return cert0;

  if (SECITEM_CompareItem(&certn_2->derIssuer, &certn_1->derSubject) ==
      SECEqual) {
    return certn_1;
  }

  LOG(WARNING) << "certificate list is not a hierarchy";
  return cert0;
}

int NSSCertDatabase::ImportUserCert(const std::string& data) {
  ScopedCERTCertificateList certificates =
      x509_util::CreateCERTCertificateListFromBytes(
          base::as_byte_span(data), net::X509Certificate::FORMAT_AUTO);
  if (certificates.empty())
    return ERR_CERT_INVALID;

  int result = psm::ImportUserCert(certificates[0].get(), GetPublicSlot());

  if (result == OK) {
    NotifyObserversClientCertStoreChanged();
  }

  return result;
}

int NSSCertDatabase::ImportUserCert(CERTCertificate* cert) {
  int result = psm::ImportUserCert(cert, GetPublicSlot());

  if (result == OK) {
    NotifyObserversClientCertStoreChanged();
  }

  return result;
}

bool NSSCertDatabase::ImportCACerts(
    const ScopedCERTCertificateList& certificates,
    TrustBits trust_bits,
    ImportCertFailureList* not_imported) {
  crypto::ScopedPK11Slot slot(GetPublicSlot());
  CERTCertificate* root = FindRootInList(certificates);

  bool success = psm::ImportCACerts(slot.get(), certificates, root, trust_bits,
                                    not_imported);
  if (success) {
    NotifyObserversTrustStoreChanged();
  }

  return success;
}

bool NSSCertDatabase::ImportServerCert(
    const ScopedCERTCertificateList& certificates,
    TrustBits trust_bits,
    ImportCertFailureList* not_imported) {
  crypto::ScopedPK11Slot slot(GetPublicSlot());
  return psm::ImportServerCert(slot.get(), certificates, trust_bits,
                               not_imported);
  // TODO(mattm): should generate OnTrustStoreChanged notification? The ability
  // to set a server cert as trusted isn't hooked up anywhere currently, but
  // technically we should generate a notification.
}

NSSCertDatabase::TrustBits NSSCertDatabase::GetCertTrust(
    const CERTCertificate* cert,
    CertType type) const {
  CERTCertTrust trust;
  SECStatus srv = CERT_GetCertTrust(cert, &trust);
  if (srv != SECSuccess) {
    LOG(ERROR) << "CERT_GetCertTrust failed with error " << PORT_GetError();
    return TRUST_DEFAULT;
  }
  // We define our own more "friendly" TrustBits, which means we aren't able to
  // round-trip all possible NSS trust flag combinations.  We try to map them in
  // a sensible way.
  switch (type) {
    case CA_CERT: {
      const unsigned kTrustedCA = CERTDB_TRUSTED_CA | CERTDB_TRUSTED_CLIENT_CA;
      const unsigned kCAFlags = kTrustedCA | CERTDB_TERMINAL_RECORD;

      TrustBits trust_bits = TRUST_DEFAULT;
      if ((trust.sslFlags & kCAFlags) == CERTDB_TERMINAL_RECORD)
        trust_bits |= DISTRUSTED_SSL;
      else if (trust.sslFlags & kTrustedCA)
        trust_bits |= TRUSTED_SSL;

      if ((trust.emailFlags & kCAFlags) == CERTDB_TERMINAL_RECORD)
        trust_bits |= DISTRUSTED_EMAIL;
      else if (trust.emailFlags & kTrustedCA)
        trust_bits |= TRUSTED_EMAIL;

      if ((trust.objectSigningFlags & kCAFlags) == CERTDB_TERMINAL_RECORD)
        trust_bits |= DISTRUSTED_OBJ_SIGN;
      else if (trust.objectSigningFlags & kTrustedCA)
        trust_bits |= TRUSTED_OBJ_SIGN;

      return trust_bits;
    }
    case SERVER_CERT:
      if (trust.sslFlags & CERTDB_TERMINAL_RECORD) {
        if (trust.sslFlags & CERTDB_TRUSTED)
          return TRUSTED_SSL;
        return DISTRUSTED_SSL;
      }
      return TRUST_DEFAULT;
    default:
      return TRUST_DEFAULT;
  }
}

bool NSSCertDatabase::DeleteCertAndKey(CERTCertificate* cert) {
  // This makes the assumption that if there was a matching private key, the
  // cert was probably a client cert, and if not, it may have been a trust
  // anchor or intemediate CA cert. This is used as a simple approximation as
  // otherwise this requires checking and combining multiple things
  // (basicConstraints if present, trust settings, etc).
  switch (DeleteCertAndKeyImpl(cert)) {
    case DeleteCertAndKeyResult::OK_NO_KEY:
      NotifyObserversTrustStoreChanged();
      return true;
    case DeleteCertAndKeyResult::OK_FOUND_KEY:
      NotifyObserversClientCertStoreChanged();
      return true;
    case DeleteCertAndKeyResult::ERROR:
      return false;
  }
}

void NSSCertDatabase::DeleteCertAndKeyAsync(ScopedCERTCertificate cert,
                                            DeleteCertCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&NSSCertDatabase::DeleteCertAndKeyImplScoped,
                     std::move(cert)),
      base::BindOnce(&NSSCertDatabase::NotifyCertRemovalAndCallBack,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

// static
bool NSSCertDatabase::IsUntrusted(const CERTCertificate* cert) {
  CERTCertTrust nsstrust;
  SECStatus rv = CERT_GetCertTrust(cert, &nsstrust);
  if (rv != SECSuccess) {
    LOG(ERROR) << "CERT_GetCertTrust failed with error " << PORT_GetError();
    return false;
  }

  // The CERTCertTrust structure contains three trust records:
  // sslFlags, emailFlags, and objectSigningFlags.  The three
  // trust records are independent of each other.
  //
  // If the CERTDB_TERMINAL_RECORD bit in a trust record is set,
  // then that trust record is a terminal record.  A terminal
  // record is used for explicit trust and distrust of an
  // end-entity or intermediate CA cert.
  //
  // In a terminal record, if neither CERTDB_TRUSTED_CA nor
  // CERTDB_TRUSTED is set, then the terminal record means
  // explicit distrust.  On the other hand, if the terminal
  // record has either CERTDB_TRUSTED_CA or CERTDB_TRUSTED bit
  // set, then the terminal record means explicit trust.
  //
  // For a root CA, the trust record does not have
  // the CERTDB_TERMINAL_RECORD bit set.

  static const unsigned int kTrusted = CERTDB_TRUSTED_CA | CERTDB_TRUSTED;
  if ((nsstrust.sslFlags & CERTDB_TERMINAL_RECORD) != 0 &&
      (nsstrust.sslFlags & kTrusted) == 0) {
    return true;
  }
  if ((nsstrust.emailFlags & CERTDB_TERMINAL_RECORD) != 0 &&
      (nsstrust.emailFlags & kTrusted) == 0) {
    return true;
  }
  if ((nsstrust.objectSigningFlags & CERTDB_TERMINAL_RECORD) != 0 &&
      (nsstrust.objectSigningFlags & kTrusted) == 0) {
    return true;
  }

  // Self-signed certificates that don't have any trust bits set are untrusted.
  // Other certificates that don't have any trust bits set may still be trusted
  // if they chain up to a trust anchor.
  if (SECITEM_CompareItem(&cert->derIssuer, &cert->derSubject) == SECEqual) {
    return (nsstrust.sslFlags & kTrusted) == 0 &&
           (nsstrust.emailFlags & kTrusted) == 0 &&
           (nsstrust.objectSigningFlags & kTrusted) == 0;
  }

  return false;
}

// static
bool NSSCertDatabase::IsWebTrustAnchor(const CERTCertificate* cert) {
  CERTCertTrust nsstrust;
  SECStatus rv = CERT_GetCertTrust(cert, &nsstrust);
  if (rv != SECSuccess) {
    LOG(ERROR) << "CERT_GetCertTrust failed with error " << PORT_GetError();
    return false;
  }

  // Note: This should return true iff a net::TrustStoreNSS instantiated with
  // SECTrustType trustSSL would classify |cert| as a trust anchor.
  const unsigned int ssl_trust_flags = nsstrust.sslFlags;

  // Determine if the certificate is a trust anchor.
  if ((ssl_trust_flags & CERTDB_TRUSTED_CA) == CERTDB_TRUSTED_CA) {
    return true;
  }

  return false;
}

// static
bool NSSCertDatabase::IsReadOnly(const CERTCertificate* cert) {
  PK11SlotInfo* slot = cert->slot;
  return slot && PK11_IsReadOnly(slot);
}

// static
bool NSSCertDatabase::IsHardwareBacked(const CERTCertificate* cert) {
  PK11SlotInfo* slot = cert->slot;
  if (!slot)
    return false;

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // For keys in Chaps, it's possible that they are truly hardware backed, or
  // they can be software-backed, such as if the creator requested it, or if the
  // TPM does not support the key algorithm. Chaps sets a kKeyInSoftware
  // attribute to true for private keys that aren't wrapped by the TPM.
  if (crypto::IsSlotProvidedByChaps(slot)) {
    constexpr CK_ATTRIBUTE_TYPE kKeyInSoftware = CKA_VENDOR_DEFINED + 5;
    SECKEYPrivateKey* private_key = PK11_FindPrivateKeyFromCert(
        slot, const_cast<CERTCertificate*>(cert), nullptr);
    // PK11_HasAttributeSet returns true if the object in the given slot has
    // the attribute set to true. Otherwise it returns false.
    if (private_key &&
        PK11_HasAttributeSet(slot, private_key->pkcs11ID, kKeyInSoftware,
                             /*haslock=*/PR_FALSE)) {
      return false;
    }
    // All keys in chaps without the attribute are hardware backed.
    return true;
  }
#endif
  return PK11_IsHW(slot);
}

void NSSCertDatabase::AddObserver(Observer* observer) {
  observer_list_->AddObserver(observer);
}

void NSSCertDatabase::RemoveObserver(Observer* observer) {
  observer_list_->RemoveObserver(observer);
}

// static
ScopedCERTCertificateList NSSCertDatabase::ExtractCertificates(
    CertInfoList certs_info) {
  ScopedCERTCertificateList certs;
  certs.reserve(certs_info.size());

  for (auto& cert_info : certs_info)
    certs.push_back(std::move(cert_info.cert));

  return certs;
}

// static
ScopedCERTCertificateList NSSCertDatabase::ListCertsImpl(
    crypto::ScopedPK11Slot slot) {
  CertInfoList certs_info = ListCertsInfoImpl(
      std::move(slot), /*add_certs_info=*/false, NSSRootsHandling::kInclude);

  return ExtractCertificates(std::move(certs_info));
}

// static
NSSCertDatabase::CertInfoList NSSCertDatabase::ListCertsInfoImpl(
    crypto::ScopedPK11Slot slot,
    bool add_certs_info,
    NSSRootsHandling nss_roots_handling) {
  // This method may acquire the NSS lock or reenter this code via extension
  // hooks (such as smart card UI). To ensure threads are not starved or
  // deadlocked, the base::ScopedBlockingCall below increments the thread pool
  // capacity if this method takes too much time to run.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (nss_roots_handling == NSSRootsHandling::kExclude) {
    // This assumes that using a new TrustStoreNSS instance on each
    // ListCertsInfo call is not expensive. If that ever changes this might
    // need to be rethought.
    TrustStoreNSS trust_store_nss(
        slot ? TrustStoreNSS::UserSlotTrustSetting(
                   crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot.get())))
             : TrustStoreNSS::UseTrustFromAllUserSlots());

    std::vector<TrustStoreNSS::ListCertsResult> cert_list(
        trust_store_nss.ListCertsIgnoringNSSRoots());

    CertInfoList certs_info;
    for (const auto& node : cert_list) {
      CertInfo cert_info;
      cert_info.cert = x509_util::DupCERTCertificate(node.cert.get());
      if (add_certs_info) {
        cert_info.untrusted =
            IsUntrustedUsingTrustStore(cert_info.cert.get(), node.trust);
        cert_info.web_trust_anchor = node.trust.IsTrustAnchor();
        cert_info.on_read_only_slot = IsReadOnly(cert_info.cert.get());
        cert_info.hardware_backed = IsHardwareBacked(cert_info.cert.get());
      }
      certs_info.push_back(std::move(cert_info));
    }
    return certs_info;
  } else {
    CertInfoList certs_info;
    crypto::ScopedCERTCertList cert_list = nullptr;
    if (slot) {
      cert_list.reset(PK11_ListCertsInSlot(slot.get()));
    } else {
      cert_list.reset(PK11_ListCerts(PK11CertListUnique, nullptr));
    }
    // PK11_ListCerts[InSlot] can return nullptr, e.g. because the PKCS#11 token
    // that was backing the specified slot is not available anymore.
    // Treat it as no certificates being present on the slot.
    if (!cert_list) {
      LOG(WARNING) << (slot ? "PK11_ListCertsInSlot" : "PK11_ListCerts")
                   << " returned null";
      return certs_info;
    }

    CERTCertListNode* node;
    for (node = CERT_LIST_HEAD(cert_list); !CERT_LIST_END(node, cert_list);
         node = CERT_LIST_NEXT(node)) {
      CertInfo cert_info;
      cert_info.cert = x509_util::DupCERTCertificate(node->cert);

      if (add_certs_info) {
        cert_info.on_read_only_slot = IsReadOnly(cert_info.cert.get());
        cert_info.untrusted = IsUntrusted(cert_info.cert.get());
        cert_info.web_trust_anchor = IsWebTrustAnchor(cert_info.cert.get());
        cert_info.hardware_backed = IsHardwareBacked(cert_info.cert.get());
      }

      certs_info.push_back(std::move(cert_info));
    }
    return certs_info;
  }
}

void NSSCertDatabase::NotifyCertRemovalAndCallBack(
    DeleteCertCallback callback,
    DeleteCertAndKeyResult result) {
  // This makes the assumption that if there was a matching private key, the
  // cert was probably a client cert, and if not, it may have been a trust
  // anchor or intemediate CA cert.
  switch (result) {
    case DeleteCertAndKeyResult::OK_NO_KEY:
      NotifyObserversTrustStoreChanged();
      std::move(callback).Run(true);
      break;
    case DeleteCertAndKeyResult::OK_FOUND_KEY:
      NotifyObserversClientCertStoreChanged();
      std::move(callback).Run(true);
      break;
    case DeleteCertAndKeyResult::ERROR:
      std::move(callback).Run(false);
      break;
  }
}

void NSSCertDatabase::NotifyObserversTrustStoreChanged() {
  observer_list_->Notify(FROM_HERE, &Observer::OnTrustStoreChanged);
}

void NSSCertDatabase::NotifyObserversClientCertStoreChanged() {
  observer_list_->Notify(FROM_HERE, &Observer::OnClientCertStoreChanged);
}

// static
NSSCertDatabase::DeleteCertAndKeyResult NSSCertDatabase::DeleteCertAndKeyImpl(
    CERTCertificate* cert) {
  // This method may acquire the NSS lock or reenter this code via extension
  // hooks (such as smart card UI). To ensure threads are not starved or
  // deadlocked, the base::ScopedBlockingCall below increments the thread pool
  // capacity if this method takes too much time to run.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // For some reason, PK11_DeleteTokenCertAndKey only calls
  // SEC_DeletePermCertificate if the private key is found.  So, we check
  // whether a private key exists before deciding which function to call to
  // delete the cert.
  SECKEYPrivateKey* privKey = PK11_FindKeyByAnyCert(cert, nullptr);
  if (privKey) {
    SECKEY_DestroyPrivateKey(privKey);
    if (PK11_DeleteTokenCertAndKey(cert, nullptr)) {
      LOG(ERROR) << "PK11_DeleteTokenCertAndKey failed: " << PORT_GetError();
      return DeleteCertAndKeyResult::ERROR;
    }
    return DeleteCertAndKeyResult::OK_FOUND_KEY;
  } else {
    if (SEC_DeletePermCertificate(cert)) {
      LOG(ERROR) << "SEC_DeletePermCertificate failed: " << PORT_GetError();
      return DeleteCertAndKeyResult::ERROR;
    }
    return DeleteCertAndKeyResult::OK_NO_KEY;
  }
}

// static
NSSCertDatabase::DeleteCertAndKeyResult
NSSCertDatabase::DeleteCertAndKeyImplScoped(ScopedCERTCertificate cert) {
  return NSSCertDatabase::DeleteCertAndKeyImpl(cert.get());
}

}  // namespace net
