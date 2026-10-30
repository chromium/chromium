// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_win.h"

#include <algorithm>
#include <atomic>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_handle.h"
#include "crypto/obsolete/sha1.h"
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
bssl::CertificateTrust RootCertTrust() {
  return bssl::CertificateTrust::ForTrustAnchorOrLeaf()
      .WithEnforceAnchorExpiry()
      .WithEnforceAnchorConstraints()
      .WithRequireLeafSelfSigned();
}

// Certificates in the Trusted People store may be trusted leafs (if
// self-signed).
bssl::CertificateTrust TrustedPeopleTrust() {
  return bssl::CertificateTrust::ForTrustedLeaf().WithRequireLeafSelfSigned();
}

// A single logical certificate store change (an enterprise policy refresh, for
// instance) typically signals the change event several times in quick
// succession, once per affected physical store. Waiting for the event to stay
// quiet for this long coalesces such a burst into one refresh.
constexpr base::TimeDelta kDefaultStoreChangeDebounceDelay = base::Seconds(1);

std::atomic<base::TimeDelta>& StoreChangeDebounceDelay() {
  static std::atomic<base::TimeDelta> delay{kDefaultStoreChangeDebounceDelay};
  return delay;
}

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

class TrustStoreWin::ChangeMonitor
    : public base::RefCountedDeleteOnSequence<TrustStoreWin::ChangeMonitor>,
      public base::win::ObjectWatcher::Delegate {
 public:
  // Returns null if the change event could not be created, in which case the
  // caller simply does not monitor for changes.
  static scoped_refptr<ChangeMonitor> Create() {
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
    auto monitor = base::MakeRefCounted<ChangeMonitor>(task_runner);
    if (!monitor->event_.is_valid()) {
      return nullptr;
    }
    // `watcher_` is sequence-affine, so every operation on it, including its
    // destruction, happens on `task_runner`.
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&ChangeMonitor::StartWatching, monitor));
    return monitor;
  }

  explicit ChangeMonitor(scoped_refptr<base::SequencedTaskRunner> task_runner)
      : base::RefCountedDeleteOnSequence<ChangeMonitor>(std::move(task_runner)),
        debounce_delay_(
            StoreChangeDebounceDelay().load(std::memory_order_relaxed)),
        event_(::CreateEvent(/*lpEventAttributes=*/nullptr,
                             /*bManualReset=*/TRUE,
                             /*bInitialState=*/FALSE,
                             /*lpName=*/nullptr)) {
    if (!event_.is_valid()) {
      PLOG(ERROR) << "Failed to create certificate store change event";
    }
  }

  ChangeMonitor(const ChangeMonitor& other) = delete;
  ChangeMonitor& operator=(const ChangeMonitor& other) = delete;

  // The event stays open for as long as any reference is held, which includes
  // every Impl whose stores are registered against it. It is therefore never
  // closed while Windows may still signal it.
  HANDLE event() const { return event_.get(); }

  // Safe to call from any sequence.
  bool stores_are_stale() const {
    return stores_are_stale_.load(std::memory_order_acquire);
  }
  void clear_stores_are_stale() {
    stores_are_stale_.store(false, std::memory_order_release);
  }

  // base::win::ObjectWatcher::Delegate:
  void OnObjectSignaled(HANDLE object) override {
    ::ResetEvent(event_.get());
    // Re-arm before doing anything else so that a change arriving while this
    // one is being processed is still observed.
    StartWatching();
    stores_are_stale_debounce_timer_.Start(FROM_HERE, debounce_delay_, this,
                                           &ChangeMonitor::OnChangeSettled);
  }

 private:
  friend class base::RefCountedDeleteOnSequence<ChangeMonitor>;
  friend class base::DeleteHelper<ChangeMonitor>;

  ~ChangeMonitor() override = default;

  void StartWatching() {
    if (!watcher_.StartWatchingOnce(event_.get(), this, FROM_HERE)) {
      LOG(ERROR) << "Failed to watch for certificate store changes";
    }
  }

  void OnChangeSettled() {
    // The stores are not rebuilt here. Rebuilding is left to the next call
    // into TrustStoreWin, so that the cost is not paid at all if nothing needs
    // the stores.
    stores_are_stale_.store(true, std::memory_order_release);
    CertDatabase::GetInstance()->NotifyObserversTrustStoreChanged();
  }

  const base::TimeDelta debounce_delay_;

  const base::win::ScopedHandle event_;

  // True if Windows has reported a change that the published snapshot does not
  // reflect yet.
  std::atomic<bool> stores_are_stale_{false};

  // Both are only ever used on the owning sequence.
  base::win::ObjectWatcher watcher_;
  base::OneShotTimer stores_are_stale_debounce_timer_;
};

class TrustStoreWin::Impl
    : public base::RefCountedThreadSafe<TrustStoreWin::Impl> {
 public:
  // Creates a snapshot of the Windows certificate stores. If `monitor` is
  // non-null, the stores are registered to signal its event when they change.
  explicit Impl(scoped_refptr<ChangeMonitor> monitor)
      : monitor_(std::move(monitor)) {
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

    // Ask Windows to signal the change event when any of these stores change.
    // No synchronization is needed: this snapshot has not been published yet,
    // so no other thread can be reading the stores.
    if (monitor_) {
      ApplyCertStoreControlToAllStores(CERT_STORE_CTRL_NOTIFY_CHANGE);
    }
  }

  explicit Impl(CertStores stores)
      : root_cert_store_(std::move(stores.roots)),
        intermediate_cert_store_(std::move(stores.intermediates)),
        all_certs_store_(std::move(stores.all)),
        trusted_people_cert_store_(std::move(stores.trusted_people)),
        disallowed_cert_store_(std::move(stores.disallowed)) {}

  Impl(const Impl& other) = delete;
  Impl& operator=(const Impl& other) = delete;

 private:
  friend class base::RefCountedThreadSafe<Impl>;

  ~Impl() {
    // Stop Windows from signaling the change event through these stores before
    // they are closed. The `crypto::ScopedHCERTSTORE` members are destroyed
    // after this body runs, and `monitor_` is released after them, so the
    // event outlives every store that refers to it.
    if (monitor_) {
      ApplyCertStoreControlToAllStores(CERT_STORE_CTRL_CANCEL_NOTIFY);
    }
  }

  // Runs CertControlStore with `control_code` on each certificate store.
  // Every store is attempted even if an earlier one fails, so that a partial
  // failure cannot leave some stores registered and others not. Returns true
  // only if all stores succeeded.
  bool ApplyCertStoreControlToAllStores(DWORD control_code) const {
    HANDLE event = monitor_->event();
    bool all_succeeded = true;
    for (HCERTSTORE store :
         {root_cert_store_.get(), intermediate_cert_store_.get(),
          trusted_people_cert_store_.get(), disallowed_cert_store_.get()}) {
      if (!store) {
        continue;
      }
      if (!CertControlStore(store, 0, control_code, &event)) {
        PLOG(ERROR) << "CertControlStore failed with control code: "
                    << control_code;
        all_succeeded = false;
      }
    }
    return all_succeeded;
  }

 public:
  void SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                        bssl::ParsedCertificateList* issuers) const {
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
      std::shared_ptr<const bssl::ParsedCertificate> parsed =
          x509_util::CreateParsedCertificate(std::move(der_crypto), &errors);
      if (parsed) {
        issuers->push_back(std::move(parsed));
      }
    }
  }

  bssl::CertificateTrust GetTrust(const bssl::ParsedCertificate* cert) const {
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
          return RootCertTrust();
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
          return TrustedPeopleTrust();
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

  std::vector<net::PlatformTrustStore::CertWithTrust> GetAllUserAddedCerts()
      const {
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
        AddCertWithTrust(cert_from_store, TrustedPeopleTrust(), &certs);
      }
    }

    while ((cert_from_store = CertEnumCertificatesInStore(
                root_cert_store_.get(), cert_from_store))) {
      if (IsCertTrustedForServerAuth(cert_from_store)) {
        AddCertWithTrust(cert_from_store, RootCertTrust(), &certs);
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
  // Held first so that it is released last: the change event must outlive the
  // stores registered against it. Null if change monitoring is not in use.
  const scoped_refptr<ChangeMonitor> monitor_;

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
};

// TODO(crbug.com/40784681): support CTLs.
TrustStoreWin::TrustStoreWin() = default;

void TrustStoreWin::InitializeStores() {
  // Don't need return value
  GetImpl();
}

scoped_refptr<const TrustStoreWin::Impl> TrustStoreWin::GetImpl() {
  {
    base::AutoLock lock(impl_lock_);
    if (impl_ && !rebuild_in_progress_ &&
        !(monitor_ && monitor_->stores_are_stale())) {
      return impl_;
    }
  }

  // Serialize rebuilds and wait for any in-progress rebuild to publish its
  // replacement before checking whether another rebuild is needed.
  base::AutoLock rebuild_lock(rebuild_lock_);

  scoped_refptr<ChangeMonitor> monitor;
  {
    base::AutoLock lock(impl_lock_);
    if (impl_ && !(monitor_ && monitor_->stores_are_stale())) {
      // Another thread rebuilt while this one waited for `rebuild_lock_`.
      return impl_;
    }
    monitor = monitor_;
    // Keep the old snapshot's notification registrations alive until the
    // replacement has registered its stores, but block new accesses until it
    // is published.
    rebuild_in_progress_ = true;
  }

  if (!monitor) {
    // Attempt monitoring on initial construction. If event creation fails,
    // publish the snapshot without a monitor. Subsequent calls return it via
    // the fast path above, so monitoring is not retried for this TrustStoreWin.
    monitor = ChangeMonitor::Create();
  } else {
    // Clear the flag before building rather than after, so that a change
    // arriving while the snapshot is being built costs one redundant rebuild
    // instead of being dropped. The in-progress flag keeps new accesses from
    // using the old snapshot while the stale flag is cleared.
    monitor->clear_stores_are_stale();
  }

  auto fresh = base::MakeRefCounted<Impl>(monitor);

  base::AutoLock lock(impl_lock_);
  monitor_ = std::move(monitor);
  impl_ = std::move(fresh);
  rebuild_in_progress_ = false;
  return impl_;
}

std::unique_ptr<TrustStoreWin> TrustStoreWin::CreateForTesting(
    CertStores stores) {
  return base::WrapUnique(new TrustStoreWin(
      base::MakeRefCounted<TrustStoreWin::Impl>(std::move(stores))));
}

TrustStoreWin::TrustStoreWin(scoped_refptr<Impl> impl)
    : impl_(std::move(impl)) {}

TrustStoreWin::~TrustStoreWin() = default;

void TrustStoreWin::SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                                     bssl::ParsedCertificateList* issuers) {
  GetImpl()->SyncGetIssuersOf(cert, issuers);
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
  return GetImpl()->GetTrust(cert);
}

std::shared_ptr<const bssl::MTCAnchor> TrustStoreWin::GetTrustedMTCIssuerOf(
    const bssl::ParsedCertificate* cert) {
  return nullptr;
}

std::vector<net::PlatformTrustStore::CertWithTrust>
TrustStoreWin::GetAllUserAddedCerts() {
  return GetImpl()->GetAllUserAddedCerts();
}

HANDLE TrustStoreWin::GetChangeEventForTesting() {
  base::AutoLock lock(impl_lock_);
  return monitor_ ? monitor_->event() : nullptr;
}

// static
base::TimeDelta TrustStoreWin::SetChangeDebounceDelayForTesting(
    base::TimeDelta delay) {
  return StoreChangeDebounceDelay().exchange(delay, std::memory_order_relaxed);
}

}  // namespace net
