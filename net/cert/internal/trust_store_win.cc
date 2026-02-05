// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_win.h"

#include <algorithm>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/scoped_blocking_call.h"
#include "crypto/obsolete/sha1.h"
#include "net/base/features.h"
#include "net/cert/cert_database.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_win.h"
#include "net/third_party/mozilla_win/cert/win_util.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"

namespace net {

std::array<uint8_t, crypto::obsolete::Sha1::kSize> Sha1ForWinTrust(
    base::span<const uint8_t> data) {
  return crypto::obsolete::Sha1::Hash(data);
}

namespace {

// Certificates in the Windows roots store may be used as either trust
// anchors or trusted leafs (if self-signed).
constexpr bssl::CertificateTrust kRootCertTrust =
    bssl::CertificateTrust::ForTrustAnchorOrLeaf()
        .WithEnforceAnchorExpiry()
        .WithEnforceAnchorConstraints()
        .WithRequireLeafSelfSigned();

// Certificates in the Trusted People store may be trusted leafs (if
// self-signed).
constexpr bssl::CertificateTrust kTrustedPeopleTrust =
    bssl::CertificateTrust::ForTrustedLeaf().WithRequireLeafSelfSigned();

// Returns true if the cert can be used for server authentication, based on
// certificate properties.
//
// While there are a variety of certificate properties that can affect how
// trust is computed, the main property is CERT_ENHKEY_USAGE_PROP_ID, which
// is intersected with the certificate's EKU extension (if present).
// The intersection is documented in the Remarks section of
// CertGetEnhancedKeyUsage, and is as follows:
// - No EKU property, and no EKU extension = Trusted for all purpose
// - Either an EKU property, or EKU extension, but not both = Trusted only
//   for the listed purposes
// - Both an EKU property and an EKU extension = Trusted for the set
//   intersection of the listed purposes
// CertGetEnhancedKeyUsage handles this logic, and if an empty set is
// returned, the distinction between the first and third case can be
// determined by GetLastError() returning CRYPT_E_NOT_FOUND.
//
// See:
// https://docs.microsoft.com/en-us/windows/win32/api/wincrypt/nf-wincrypt-certgetenhancedkeyusage
//
// If we run into any errors reading the certificate properties, we fail
// closed.
bool IsCertTrustedForServerAuth(PCCERT_CONTEXT cert) {
  DWORD usage_size = 0;

  if (!::CertGetEnhancedKeyUsage(cert, 0, nullptr, &usage_size)) {
    return false;
  }

  auto usage_buffer = base::HeapArray<uint8_t>::WithSize(usage_size);
  auto* usage = reinterpret_cast<CERT_ENHKEY_USAGE*>(usage_buffer.data());
  if (!::CertGetEnhancedKeyUsage(cert, 0, usage, &usage_size)) {
    return false;
  }

  if (usage->cUsageIdentifier == 0) {
    // check GetLastError
    HRESULT error_code = GetLastError();

    switch (error_code) {
      case CRYPT_E_NOT_FOUND:
        return true;
      case S_OK:
        return false;
      default:
        return false;
    }
  }

  // SAFETY: `usage->rgpszUsageIdentifier` is an array of LPSTR (pointer to null
  // terminated string) of length `usage->cUsageIdentifier`.
  base::span<LPSTR> usage_identifiers = UNSAFE_BUFFERS(
      base::span(usage->rgpszUsageIdentifier, usage->cUsageIdentifier));
  for (std::string_view eku : usage_identifiers) {
    if ((eku == szOID_PKIX_KP_SERVER_AUTH) ||
        (eku == szOID_ANY_ENHANCED_KEY_USAGE)) {
      return true;
    }
  }
  return false;
}

void AddCertWithTrust(
    PCCERT_CONTEXT cert,
    const bssl::CertificateTrust trust,
    std::vector<net::PlatformTrustStore::CertWithTrust>* certs) {
  certs->push_back(net::PlatformTrustStore::CertWithTrust(
      base::ToVector(x509_util::CertContextAsSpan(cert)), trust));
}

}  // namespace

TrustStoreWin::CertStores::CertStores() = default;
TrustStoreWin::CertStores::~CertStores() = default;
TrustStoreWin::CertStores::CertStores(CertStores&& other) = default;
TrustStoreWin::CertStores& TrustStoreWin::CertStores::operator=(
    CertStores&& other) = default;

// static
TrustStoreWin::CertStores
TrustStoreWin::CertStores::CreateInMemoryStoresForTesting() {
  TrustStoreWin::CertStores stores;
  stores.roots = crypto::ScopedHCERTSTORE(CertOpenStore(
      CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING, NULL, 0, nullptr));
  stores.intermediates = crypto::ScopedHCERTSTORE(CertOpenStore(
      CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING, NULL, 0, nullptr));
  stores.trusted_people = crypto::ScopedHCERTSTORE(CertOpenStore(
      CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING, NULL, 0, nullptr));
  stores.disallowed = crypto::ScopedHCERTSTORE(CertOpenStore(
      CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING, NULL, 0, nullptr));
  stores.InitializeAllCertsStore();
  return stores;
}

TrustStoreWin::CertStores
TrustStoreWin::CertStores::CreateNullStoresForTesting() {
  return TrustStoreWin::CertStores();
}

// static
TrustStoreWin::CertStores TrustStoreWin::CertStores::CreateWithCollections() {
  TrustStoreWin::CertStores stores;
  stores.roots = crypto::ScopedHCERTSTORE(
      CertOpenStore(CERT_STORE_PROV_COLLECTION, 0, NULL, 0, nullptr));
  stores.intermediates = crypto::ScopedHCERTSTORE(
      CertOpenStore(CERT_STORE_PROV_COLLECTION, 0, NULL, 0, nullptr));
  stores.trusted_people = crypto::ScopedHCERTSTORE(
      CertOpenStore(CERT_STORE_PROV_COLLECTION, 0, NULL, 0, nullptr));
  stores.disallowed = crypto::ScopedHCERTSTORE(
      CertOpenStore(CERT_STORE_PROV_COLLECTION, 0, NULL, 0, nullptr));
  stores.InitializeAllCertsStore();
  return stores;
}

void TrustStoreWin::CertStores::InitializeAllCertsStore() {
  all = crypto::ScopedHCERTSTORE(
      CertOpenStore(CERT_STORE_PROV_COLLECTION, 0, NULL, 0, nullptr));
  if (is_null()) {
    return;
  }
  // Add intermediate and root cert stores to the all_cert_store collection so
  // SyncGetIssuersOf will find them. disallowed_cert_store is not added
  // because the certs are distrusted; making them non-findable in
  // SyncGetIssuersOf helps us fail path-building faster.
  // `trusted_people` is not added because it can only contain end-entity
  // certs, so checking it for issuers during path building is not necessary.
  if (!CertAddStoreToCollection(all.get(), intermediates.get(),
                                /*dwUpdateFlags=*/0, /*dwPriority=*/0)) {
    return;
  }
  if (!CertAddStoreToCollection(all.get(), roots.get(),
                                /*dwUpdateFlags=*/0, /*dwPriority=*/0)) {
    return;
  }
}

class TrustStoreWin::Impl {
 public:
  // Creates a TrustStoreWin.
  Impl() {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    CertStores stores = CertStores::CreateWithCollections();
    if (stores.is_null()) {
      // If there was an error initializing the cert store collections, give
      // up. The Impl object will still be created but any calls to its public
      // methods will return no results.
      return;
    }

    // Grab the user-added roots.
    GatherEnterpriseCertsForLocation(stores.roots.get(),
                                     CERT_SYSTEM_STORE_LOCAL_MACHINE, L"ROOT");
    GatherEnterpriseCertsForLocation(
        stores.roots.get(), CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY,
        L"ROOT");
    GatherEnterpriseCertsForLocation(stores.roots.get(),
                                     CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE,
                                     L"ROOT");
    GatherEnterpriseCertsForLocation(stores.roots.get(),
                                     CERT_SYSTEM_STORE_CURRENT_USER, L"ROOT");
    GatherEnterpriseCertsForLocation(
        stores.roots.get(), CERT_SYSTEM_STORE_CURRENT_USER_GROUP_POLICY,
        L"ROOT");

    // Grab the user-added intermediates.
    GatherEnterpriseCertsForLocation(stores.intermediates.get(),
                                     CERT_SYSTEM_STORE_LOCAL_MACHINE, L"CA");
    GatherEnterpriseCertsForLocation(
        stores.intermediates.get(),
        CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY, L"CA");
    GatherEnterpriseCertsForLocation(stores.intermediates.get(),
                                     CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE,
                                     L"CA");
    GatherEnterpriseCertsForLocation(stores.intermediates.get(),
                                     CERT_SYSTEM_STORE_CURRENT_USER, L"CA");
    GatherEnterpriseCertsForLocation(
        stores.intermediates.get(), CERT_SYSTEM_STORE_CURRENT_USER_GROUP_POLICY,
        L"CA");

    // Grab the user-added trusted server certs. Trusted end-entity certs are
    // only allowed for server auth in the "local machine" store, but not in the
    // "current user" store.
    GatherEnterpriseCertsForLocation(stores.trusted_people.get(),
                                     CERT_SYSTEM_STORE_LOCAL_MACHINE,
                                     L"TrustedPeople");
    GatherEnterpriseCertsForLocation(
        stores.trusted_people.get(),
        CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY, L"TrustedPeople");
    GatherEnterpriseCertsForLocation(stores.trusted_people.get(),
                                     CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE,
                                     L"TrustedPeople");

    // Grab the user-added disallowed certs.
    GatherEnterpriseCertsForLocation(stores.disallowed.get(),
                                     CERT_SYSTEM_STORE_LOCAL_MACHINE,
                                     L"Disallowed");
    GatherEnterpriseCertsForLocation(
        stores.disallowed.get(), CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY,
        L"Disallowed");
    GatherEnterpriseCertsForLocation(stores.disallowed.get(),
                                     CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE,
                                     L"Disallowed");
    GatherEnterpriseCertsForLocation(
        stores.disallowed.get(), CERT_SYSTEM_STORE_CURRENT_USER, L"Disallowed");
    GatherEnterpriseCertsForLocation(
        stores.disallowed.get(), CERT_SYSTEM_STORE_CURRENT_USER_GROUP_POLICY,
        L"Disallowed");

    root_cert_store_ = std::move(stores.roots);
    intermediate_cert_store_ = std::move(stores.intermediates);
    trusted_people_cert_store_ = std::move(stores.trusted_people);
    disallowed_cert_store_ = std::move(stores.disallowed);
    all_certs_store_ = std::move(stores.all);

    // Monitor certificate store changes for cache invalidation.
    SetupCollectionStoreMonitoring();
  }

  Impl(CertStores stores)
      : root_cert_store_(std::move(stores.roots)),
        intermediate_cert_store_(std::move(stores.intermediates)),
        all_certs_store_(std::move(stores.all)),
        trusted_people_cert_store_(std::move(stores.trusted_people)),
        disallowed_cert_store_(std::move(stores.disallowed)) {}

  ~Impl() {
    // Clean up certificate store monitoring resources.
    UnregisterWaitHandle();
    CloseChangeEventHandle();
  }
  Impl(const Impl& other) = delete;
  Impl& operator=(const Impl& other) = delete;

 private:
  // Closes and cleans up the shared change event handle.
  void CloseChangeEventHandle() EXCLUSIVE_LOCKS_REQUIRED(stores_resync_lock_) {
    if (shared_change_event_) {
      CloseHandle(shared_change_event_);
      shared_change_event_ = nullptr;
    }
  }

  // Unregisters the wait handle using WaitableEvent to avoid deadlock issues.
  void UnregisterWaitHandle() EXCLUSIVE_LOCKS_REQUIRED(stores_resync_lock_) {
    if (!shared_wait_handle_) {
      return;
    }

    // Create a WaitableEvent and pass its handle to UnregisterWaitEx.
    // This avoids the deadlock that can occur when calling UnregisterWaitEx
    // with INVALID_HANDLE_VALUE from the UI thread while a callback is running.
    base::WaitableEvent event;
    if (!UnregisterWaitEx(shared_wait_handle_, event.handle())) {
      if (const auto error = GetLastError(); error != ERROR_IO_PENDING) {
        PLOG(ERROR) << "UnregisterWaitEx failed";
        shared_wait_handle_ = nullptr;
        return;
      }
    }
    // Wait for unregistration to complete.
    event.Wait();
    shared_wait_handle_ = nullptr;
  }

  // Sets up certificate store change monitoring using Windows event handles.
  // Registers a manual-reset event with all stores and a thread pool callback
  // that fires once per change (WT_EXECUTEONLYONCE), requiring explicit
  // re-registration after each sync to detect subsequent changes.
  void SetupCollectionStoreMonitoring()
      EXCLUSIVE_LOCKS_REQUIRED(stores_resync_lock_) {
    // Manual-reset event: stays signaled until explicitly reset, allowing us to
    // control when we clear it (in EnsureStoresAreSynced before resyncing).
    shared_change_event_ = CreateEvent(/*lpEventAttributes=*/nullptr,
                                       /*bManualReset=*/TRUE,
                                       /*bInitialState=*/FALSE,
                                       /*lpName=*/nullptr);
    if (!shared_change_event_) {
      PLOG(ERROR) << "Failed to create certificate store change event";
      return;
    }

    // Register change notifications with all stores. Windows will signal
    // shared_change_event_ when any certificate is added/removed/modified.
    if (!ApplyCertStoreControlToAllStores(CERT_STORE_CTRL_NOTIFY_CHANGE)) {
      PLOG(ERROR) << "Failed to register change notifications with cert stores";
      CloseChangeEventHandle();
      return;
    }

    if (!RegisterWaitForChangeNotification()) {
      PLOG(ERROR) << "Failed to register certificate store change callback";
      CloseChangeEventHandle();
    }
  }

  // Registers the wait callback for certificate store change notifications.
  // Uses WT_EXECUTEONLYONCE to fire the callback exactly once, then
  // automatically unregister. This gives explicit control over when to
  // re-register (after processing the change in EnsureStoresAreSynced).
  bool RegisterWaitForChangeNotification()
      EXCLUSIVE_LOCKS_REQUIRED(stores_resync_lock_) {
    UnregisterWaitHandle();
    if (RegisterWaitForSingleObject(&shared_wait_handle_, shared_change_event_,
                                    CollectionStoreChangeCallback, this,
                                    INFINITE, WT_EXECUTEONLYONCE)) {
      return true;
    }
    return false;
  }

  static void CALLBACK CollectionStoreChangeCallback(PVOID context,
                                                     BOOLEAN timed_out) {
    // We registered with INFINITE timeout, so timed_out should always be FALSE.
    // The callback is only invoked when the event is signaled, never by
    // timeout.
    DCHECK(!timed_out);
    static_cast<TrustStoreWin::Impl*>(context)->OnCollectionStoreChange();
  }

  // Runs CertControlStore operations on all certificate stores using the passed
  // control code. Called from:
  //   - SetupCollectionStoreMonitoring() with CERT_STORE_CTRL_NOTIFY_CHANGE
  //   - EnsureStoresAreSynced() with CERT_STORE_CTRL_RESYNC
  // Returns true only if all stores successfully complete the operation.
  bool ApplyCertStoreControlToAllStores(DWORD control_code)
      EXCLUSIVE_LOCKS_REQUIRED(stores_resync_lock_) {
    if (!shared_change_event_) {
      PLOG(ERROR) << "Certificate store monitoring not active";
      return false;
    }

    if (!CertControlStore(root_cert_store_.get(), 0, control_code,
                          &shared_change_event_) ||
        !CertControlStore(intermediate_cert_store_.get(), 0, control_code,
                          &shared_change_event_) ||
        !CertControlStore(trusted_people_cert_store_.get(), 0, control_code,
                          &shared_change_event_) ||
        !CertControlStore(disallowed_cert_store_.get(), 0, control_code,
                          &shared_change_event_)) {
      PLOG(ERROR) << "CertControlStore failed with control code: "
                  << control_code;
      return false;
    }
    return true;
  }

  void OnCollectionStoreChange() {
    // The actual sync is deferred until the next certificate access which
    // ensures we don't sync multiple times when the store is in a state of
    // flux.
    stores_need_to_be_resynced_ = true;

    // Notify observers immediately to invalidate cached certificate decisions.
    // This callback fires only once due to WT_EXECUTEONLYONCE, so observers
    // are notified once even if multiple changes occur before we re-register.
    CertDatabase::GetInstance()->NotifyObserversTrustStoreChanged();
  }

  // Syncs certificate stores if stores_need_to_be_resynced_ is set.
  // Called before each store access to ensure stores are up to date.
  // Uses double-checked locking: first check without lock (fast path),
  // then acquire lock and check again to ensure only one thread resyncs.
  void EnsureStoresAreSynced() {
    // Fast path: if no resync needed, avoid acquiring the lock.
    if (!stores_need_to_be_resynced_) {
      return;
    }

    base::AutoLock lock(stores_resync_lock_);
    // Check again under lock - another thread may have already resynced.
    if (!stores_need_to_be_resynced_) {
      return;
    }

    // Reset event before syncing to minimize race window.
    if (shared_change_event_) {
      ResetEvent(shared_change_event_);
    }

    // Sync all stores with their physical Windows counterparts.
    if (!ApplyCertStoreControlToAllStores(CERT_STORE_CTRL_RESYNC)) {
      PLOG(ERROR) << "Failed to resync certificate stores";
      return;
    }

    // Re-register for the next change.
    if (!RegisterWaitForChangeNotification()) {
      PLOG(ERROR) << "Failed to re-register change notification callback";
    }

    // Clear flag only after resync completes, so other threads don't
    // access stores while resync is in progress.
    stores_need_to_be_resynced_ = false;
  }

 public:
  void SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                        bssl::ParsedCertificateList* issuers) {
    EnsureStoresAreSynced();

    if (!root_cert_store_.get() || !intermediate_cert_store_.get() ||
        !trusted_people_cert_store_.get() || !all_certs_store_.get() ||
        !disallowed_cert_store_.get()) {
      return;
    }
    base::span<const uint8_t> issuer_span = cert->issuer_tlv();

    CERT_NAME_BLOB cert_issuer_blob;
    cert_issuer_blob.cbData = static_cast<DWORD>(issuer_span.size());
    cert_issuer_blob.pbData = const_cast<uint8_t*>(issuer_span.data());

    PCCERT_CONTEXT cert_from_store = nullptr;
    while ((cert_from_store = CertFindCertificateInStore(
                all_certs_store_.get(), X509_ASN_ENCODING, 0,
                CERT_FIND_SUBJECT_NAME, &cert_issuer_blob, cert_from_store))) {
      bssl::UniquePtr<CRYPTO_BUFFER> der_crypto = x509_util::CreateCryptoBuffer(
          x509_util::CertContextAsSpan(cert_from_store));
      bssl::CertErrors errors;
      bssl::ParsedCertificate::CreateAndAddToVector(
          std::move(der_crypto), x509_util::DefaultParseCertificateOptions(),
          issuers, &errors);
    }
  }

  bssl::CertificateTrust GetTrust(const bssl::ParsedCertificate* cert) {
    EnsureStoresAreSynced();

    if (!root_cert_store_.get() || !intermediate_cert_store_.get() ||
        !trusted_people_cert_store_.get() || !all_certs_store_.get() ||
        !disallowed_cert_store_.get()) {
      return bssl::CertificateTrust::ForUnspecified();
    }

    base::span<const uint8_t> cert_span = cert->der_cert();
    std::array<uint8_t, crypto::obsolete::Sha1::kSize> cert_hash =
        Sha1ForWinTrust(cert_span);
    CRYPT_HASH_BLOB cert_hash_blob;
    cert_hash_blob.cbData = static_cast<DWORD>(cert_hash.size());
    cert_hash_blob.pbData = cert_hash.data();

    PCCERT_CONTEXT cert_from_store = nullptr;

    // Check Disallowed store first.
    while ((cert_from_store = CertFindCertificateInStore(
                disallowed_cert_store_.get(), X509_ASN_ENCODING, 0,
                CERT_FIND_SHA1_HASH, &cert_hash_blob, cert_from_store))) {
      base::span<const uint8_t> cert_from_store_span =
          x509_util::CertContextAsSpan(cert_from_store);
      // If a cert is in the windows distruted store, it is considered
      // distrusted for all purporses. EKU isn't checked. See crbug.com/1355961.
      if (std::ranges::equal(cert_span, cert_from_store_span)) {
        return bssl::CertificateTrust::ForDistrusted();
      }
    }

    while ((cert_from_store = CertFindCertificateInStore(
                root_cert_store_.get(), X509_ASN_ENCODING, 0,
                CERT_FIND_SHA1_HASH, &cert_hash_blob, cert_from_store))) {
      base::span<const uint8_t> cert_from_store_span =
          x509_util::CertContextAsSpan(cert_from_store);
      if (std::ranges::equal(cert_span, cert_from_store_span)) {
        // If we find at least one version of the cert that is trusted for TLS
        // Server Auth, we will trust the cert.
        if (IsCertTrustedForServerAuth(cert_from_store)) {
          return kRootCertTrust;
        }
      }
    }

    while ((cert_from_store = CertFindCertificateInStore(
                trusted_people_cert_store_.get(), X509_ASN_ENCODING, 0,
                CERT_FIND_SHA1_HASH, &cert_hash_blob, cert_from_store))) {
      base::span<const uint8_t> cert_from_store_span =
          x509_util::CertContextAsSpan(cert_from_store);
      if (std::ranges::equal(cert_span, cert_from_store_span)) {
        // If we find at least one version of the cert that is trusted for TLS
        // Server Auth, we will trust the cert.
        if (IsCertTrustedForServerAuth(cert_from_store)) {
          return kTrustedPeopleTrust;
        }
      }
    }

    // If we fall through here, we've either
    //
    // (a) found the cert but it is not usable for server auth. Treat this as
    //     Unspecified trust. Originally this was treated as Distrusted, but
    //     this is inconsistent with how the Windows verifier works, which is to
    //     union all of the EKU usages for all instances of the cert, whereas
    //     sending back Distrusted would not do that.
    //
    // or
    //
    // (b) Haven't found the cert. Tell everyone Unspecified.
    return bssl::CertificateTrust::ForUnspecified();
  }

  std::vector<net::PlatformTrustStore::CertWithTrust> GetAllUserAddedCerts() {
    EnsureStoresAreSynced();

    std::vector<net::PlatformTrustStore::CertWithTrust> certs;
    if (!root_cert_store_.get() || !intermediate_cert_store_.get() ||
        !trusted_people_cert_store_.get() || !all_certs_store_.get() ||
        !disallowed_cert_store_.get()) {
      return certs;
    }

    PCCERT_CONTEXT cert_from_store = nullptr;
    while ((cert_from_store = CertEnumCertificatesInStore(
                disallowed_cert_store_.get(), cert_from_store))) {
      AddCertWithTrust(cert_from_store, bssl::CertificateTrust::ForDistrusted(),
                       &certs);
    }

    while ((cert_from_store = CertEnumCertificatesInStore(
                trusted_people_cert_store_.get(), cert_from_store))) {
      if (IsCertTrustedForServerAuth(cert_from_store)) {
        AddCertWithTrust(cert_from_store, kTrustedPeopleTrust, &certs);
      }
    }

    while ((cert_from_store = CertEnumCertificatesInStore(
                root_cert_store_.get(), cert_from_store))) {
      if (IsCertTrustedForServerAuth(cert_from_store)) {
        AddCertWithTrust(cert_from_store, kRootCertTrust, &certs);
      }
    }

    while ((cert_from_store = CertEnumCertificatesInStore(
                intermediate_cert_store_.get(), cert_from_store))) {
      AddCertWithTrust(cert_from_store,
                       bssl::CertificateTrust::ForUnspecified(), &certs);
    }

    return certs;
  }

 private:
  // Cert Collection containing all user-added trust anchors.
  crypto::ScopedHCERTSTORE root_cert_store_;

  // Cert Collection containing all user-added intermediates.
  crypto::ScopedHCERTSTORE intermediate_cert_store_;

  // Cert Collection for searching via SyncGetIssuersOf()
  crypto::ScopedHCERTSTORE all_certs_store_;

  // Cert Collection containing all user-added trust leafs.
  crypto::ScopedHCERTSTORE trusted_people_cert_store_;

  // Cert Collection for all disallowed certs.
  crypto::ScopedHCERTSTORE disallowed_cert_store_;

  // Shared Windows event handle signaled when any certificate store changes.
  // Manual-reset event that stays signaled until explicitly reset in
  // EnsureStoresAreSynced(). Accessed under stores_resync_lock_ during normal
  // operation; constructor/destructor access is exempt from analysis.
  HANDLE shared_change_event_ GUARDED_BY(stores_resync_lock_) = nullptr;

  // Wait handle registered with thread pool for shared_change_event_.
  // Re-registered on each sync via RegisterWaitForChangeNotification() using
  // WT_EXECUTEONLYONCE to give explicit control over the notification
  // lifecycle. Accessed under stores_resync_lock_ during normal operation;
  // constructor/destructor access is exempt from analysis.
  HANDLE shared_wait_handle_ GUARDED_BY(stores_resync_lock_) = nullptr;

  // True if certificate stores must be resynced before next access.
  // Set by OnCollectionStoreChange and cleared after re-syncing the store
  // contents in EnsureStoresAreSynced.
  std::atomic<bool> stores_need_to_be_resynced_{false};

  // Protects certificate store resync operations.
  // - Held by EnsureStoresAreSynced while re-syncing the store contents
  // This ensures only one thread performs the resync at a time.
  base::Lock stores_resync_lock_;
};

// TODO(crbug.com/40784681): support CTLs.
TrustStoreWin::TrustStoreWin() = default;

void TrustStoreWin::InitializeStores() {
  // Don't need return value
  MaybeInitializeAndGetImpl();
}

TrustStoreWin::Impl* TrustStoreWin::MaybeInitializeAndGetImpl() {
  base::AutoLock lock(init_lock_);
  if (!impl_) {
    impl_ = std::make_unique<TrustStoreWin::Impl>();
  }
  return impl_.get();
}

std::unique_ptr<TrustStoreWin> TrustStoreWin::CreateForTesting(
    CertStores stores) {
  return base::WrapUnique(new TrustStoreWin(
      std::make_unique<TrustStoreWin::Impl>(std::move(stores))));
}

TrustStoreWin::TrustStoreWin(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

TrustStoreWin::~TrustStoreWin() = default;

void TrustStoreWin::SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                                     bssl::ParsedCertificateList* issuers) {
  MaybeInitializeAndGetImpl()->SyncGetIssuersOf(cert, issuers);
}

// As documented in IsCertTrustedForServerAuth(), on Windows, the
// set of extended key usages present in a certificate can be further
// scoped down by user setting; effectively, disabling a given EKU for
// a given intermediate or root.
//
// Windows uses this during path building when filtering the EKUs; if it
// encounters this property, it uses the combined EKUs to determine
// whether to continue path building, but doesn't treat the certificate
// as affirmatively revoked/distrusted.
//
// This behaviour is replicated here by returning Unspecified trust if
// we find instances of the cert that do not have the correct EKUs set
// for TLS Server Auth. This allows path building to continue and allows
// us to later trust the cert if it is present in Chrome Root Store.
//
// Windows does have some idiosyncrasies here, which result in the
// following treatment:
//
//   - If a certificate is in the Disallowed store, it is distrusted for
//     all purposes regardless of any EKUs that are set.
//   - If a certificate is in the ROOT store, and usable for TLS Server Auth,
//     then it's trusted.
//   - If a certificate is in the root store, and lacks the EKU, then continue
//     path building, but don't treat it as trusted (aka Unspecified).
//   - If we can't find the cert anywhere, then continue path
//     building, but don't treat it as trusted (aka Unspecified).
//
// If a certificate is found multiple times in the ROOT store, it is trusted
// for TLS server auth if any instance of the certificate found
// is usable for TLS server auth.
bssl::CertificateTrust TrustStoreWin::GetTrust(
    const bssl::ParsedCertificate* cert) {
  return MaybeInitializeAndGetImpl()->GetTrust(cert);
}

std::shared_ptr<const bssl::MTCAnchor> TrustStoreWin::GetTrustedMTCIssuerOf(
    const bssl::ParsedCertificate* cert) {
  return nullptr;
}

std::vector<net::PlatformTrustStore::CertWithTrust>
TrustStoreWin::GetAllUserAddedCerts() {
  return MaybeInitializeAndGetImpl()->GetAllUserAddedCerts();
}

}  // namespace net
