// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_win.h"

#include "base/hash/sha1.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "net/base/features.h"
#include "net/cert/internal/trust_store_features.h"
#include "net/cert/pki/cert_errors.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/x509_util.h"
#include "net/third_party/mozilla_win/cert/win_util.h"

namespace net {

namespace {

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

  if (!CertGetEnhancedKeyUsage(cert, 0, nullptr, &usage_size)) {
    return false;
  }

  std::vector<BYTE> usage_bytes(usage_size);
  CERT_ENHKEY_USAGE* usage =
      reinterpret_cast<CERT_ENHKEY_USAGE*>(usage_bytes.data());
  if (!CertGetEnhancedKeyUsage(cert, 0, usage, &usage_size)) {
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
  for (DWORD i = 0; i < usage->cUsageIdentifier; i++) {
    base::StringPiece eku = base::StringPiece(usage->rgpszUsageIdentifier[i]);
    if ((eku == szOID_PKIX_KP_SERVER_AUTH) ||
        (eku == szOID_ANY_ENHANCED_KEY_USAGE)) {
      return true;
    }
  }
  return false;
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

    // Auto-sync all of the cert stores to get updates to the cert store.
    // Auto-syncing on all_certs_store seems to work to resync the nested
    // stores, although the docs at
    // https://docs.microsoft.com/en-us/windows/win32/api/wincrypt/nf-wincrypt-certcontrolstore
    // are somewhat unclear. If and when root store changes are linked to
    // clearing various caches, this should be replaced with
    // CERT_STORE_CTRL_NOTIFY_CHANGE and CERT_STORE_CTRL_RESYNC.
    if (!CertControlStore(stores.all.get(), 0, CERT_STORE_CTRL_AUTO_RESYNC,
                          0) ||
        !CertControlStore(stores.trusted_people.get(), 0,
                          CERT_STORE_CTRL_AUTO_RESYNC, 0) ||
        !CertControlStore(stores.disallowed.get(), 0,
                          CERT_STORE_CTRL_AUTO_RESYNC, 0)) {
      PLOG(ERROR) << "Error enabling CERT_STORE_CTRL_AUTO_RESYNC";
    }

    root_cert_store_ = std::move(stores.roots);
    intermediate_cert_store_ = std::move(stores.intermediates);
    trusted_people_cert_store_ = std::move(stores.trusted_people);
    disallowed_cert_store_ = std::move(stores.disallowed);
    all_certs_store_ = std::move(stores.all);
  }

  Impl(CertStores stores)
      : root_cert_store_(std::move(stores.roots)),
        intermediate_cert_store_(std::move(stores.intermediates)),
        all_certs_store_(std::move(stores.all)),
        trusted_people_cert_store_(std::move(stores.trusted_people)),
        disallowed_cert_store_(std::move(stores.disallowed)) {}

  ~Impl() = default;
  Impl(const Impl& other) = delete;
  Impl& operator=(const Impl& other) = delete;

  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) {
    if (!root_cert_store_.get() || !intermediate_cert_store_.get() ||
        !trusted_people_cert_store_.get() || !all_certs_store_.get() ||
        !disallowed_cert_store_.get()) {
      return;
    }
    base::span<const uint8_t> issuer_span = cert->issuer_tlv().AsSpan();

    CERT_NAME_BLOB cert_issuer_blob;
    cert_issuer_blob.cbData = static_cast<DWORD>(issuer_span.size());
    cert_issuer_blob.pbData = const_cast<uint8_t*>(issuer_span.data());

    PCCERT_CONTEXT cert_from_store = nullptr;
    // TODO(https://crbug.com/1239270): figure out if this is thread-safe or if
    // we need locking here
    while ((cert_from_store = CertFindCertificateInStore(
                all_certs_store_.get(), X509_ASN_ENCODING, 0,
                CERT_FIND_SUBJECT_NAME, &cert_issuer_blob, cert_from_store))) {
      bssl::UniquePtr<CRYPTO_BUFFER> der_crypto =
          x509_util::CreateCryptoBuffer(base::make_span(
              cert_from_store->pbCertEncoded, cert_from_store->cbCertEncoded));
      CertErrors errors;
      ParsedCertificate::CreateAndAddToVector(
          std::move(der_crypto), x509_util::DefaultParseCertificateOptions(),
          issuers, &errors);
    }
  }

  CertificateTrust GetTrust(const ParsedCertificate* cert) {
    if (!root_cert_store_.get() || !intermediate_cert_store_.get() ||
        !trusted_people_cert_store_.get() || !all_certs_store_.get() ||
        !disallowed_cert_store_.get()) {
      return CertificateTrust::ForUnspecified();
    }

    base::span<const uint8_t> cert_span = cert->der_cert().AsSpan();
    base::SHA1Digest cert_hash = base::SHA1HashSpan(cert_span);
    CRYPT_HASH_BLOB cert_hash_blob;
    cert_hash_blob.cbData = static_cast<DWORD>(cert_hash.size());
    cert_hash_blob.pbData = cert_hash.data();

    PCCERT_CONTEXT cert_from_store = nullptr;

    // Check Disallowed store first.
    while ((cert_from_store = CertFindCertificateInStore(
                disallowed_cert_store_.get(), X509_ASN_ENCODING, 0,
                CERT_FIND_SHA1_HASH, &cert_hash_blob, cert_from_store))) {
      base::span<const uint8_t> cert_from_store_span = base::make_span(
          cert_from_store->pbCertEncoded, cert_from_store->cbCertEncoded);
      // If a cert is in the windows distruted store, it is considered
      // distrusted for all purporses. EKU isn't checked. See crbug.com/1355961.
      if (base::ranges::equal(cert_span, cert_from_store_span)) {
        return CertificateTrust::ForDistrusted();
      }
    }

    // TODO(https://crbug.com/1239270): figure out if this is thread-safe or if
    // we need locking here
    while ((cert_from_store = CertFindCertificateInStore(
                root_cert_store_.get(), X509_ASN_ENCODING, 0,
                CERT_FIND_SHA1_HASH, &cert_hash_blob, cert_from_store))) {
      base::span<const uint8_t> cert_from_store_span = base::make_span(
          cert_from_store->pbCertEncoded, cert_from_store->cbCertEncoded);
      if (base::ranges::equal(cert_span, cert_from_store_span)) {
        // If we find at least one version of the cert that is trusted for TLS
        // Server Auth, we will trust the cert.
        if (IsCertTrustedForServerAuth(cert_from_store)) {
          if (base::FeatureList::IsEnabled(
                  features::kTrustStoreTrustedLeafSupport)) {
            // Certificates in the Roots store may be used as either trust
            // anchors or trusted leafs (if self-signed).
            return CertificateTrust::ForTrustAnchorOrLeaf()
                .WithEnforceAnchorExpiry()
                .WithEnforceAnchorConstraints(
                    IsLocalAnchorConstraintsEnforcementEnabled())
                .WithRequireLeafSelfSigned();
          } else {
            return CertificateTrust::ForTrustAnchor()
                .WithEnforceAnchorExpiry()
                .WithEnforceAnchorConstraints(
                    IsLocalAnchorConstraintsEnforcementEnabled());
          }
        }
      }
    }

    if (base::FeatureList::IsEnabled(features::kTrustStoreTrustedLeafSupport)) {
      while ((cert_from_store = CertFindCertificateInStore(
                  trusted_people_cert_store_.get(), X509_ASN_ENCODING, 0,
                  CERT_FIND_SHA1_HASH, &cert_hash_blob, cert_from_store))) {
        base::span<const uint8_t> cert_from_store_span = base::make_span(
            cert_from_store->pbCertEncoded, cert_from_store->cbCertEncoded);
        if (base::ranges::equal(cert_span, cert_from_store_span)) {
          // If we find at least one version of the cert that is trusted for TLS
          // Server Auth, we will trust the cert.
          if (IsCertTrustedForServerAuth(cert_from_store)) {
            // Certificates in the Trusted People store may be trusted leafs (if
            // self-signed).
            return CertificateTrust::ForTrustedLeaf()
                .WithRequireLeafSelfSigned();
          }
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
    return CertificateTrust::ForUnspecified();
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
};

// TODO(https://crbug.com/1239268): support CTLs.
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

void TrustStoreWin::SyncGetIssuersOf(const ParsedCertificate* cert,
                                     ParsedCertificateList* issuers) {
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
CertificateTrust TrustStoreWin::GetTrust(const ParsedCertificate* cert) {
  return MaybeInitializeAndGetImpl()->GetTrust(cert);
}

}  // namespace net
