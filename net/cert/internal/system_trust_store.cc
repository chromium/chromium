// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/system_trust_store.h"

#include "build/build_config.h"
#include "crypto/crypto_buildflags.h"

#if BUILDFLAG(USE_NSS_CERTS)
#include "net/cert/internal/system_trust_store_nss.h"
#endif  // BUILDFLAG(USE_NSS_CERTS)

#if BUILDFLAG(USE_NSS_CERTS)
#include <cert.h>
#include <pk11pub.h>
#elif BUILDFLAG(IS_MAC)
#include <Security/Security.h>
#endif

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/internal/trust_store_collection.h"
#include "net/cert/internal/trust_store_in_memory.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

#if BUILDFLAG(USE_NSS_CERTS)
#include "crypto/nss_util.h"
#include "net/cert/internal/trust_store_nss.h"
#include "net/cert/known_roots_nss.h"
#include "net/cert/scoped_nss_types.h"
#elif BUILDFLAG(IS_MAC)
#include "net/base/features.h"
#include "net/cert/internal/trust_store_mac.h"
#include "net/cert/x509_util_mac.h"
#elif BUILDFLAG(IS_FUCHSIA)
#include "base/lazy_instance.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#elif BUILDFLAG(IS_WIN)
#include "net/cert/internal/trust_store_win.h"
#endif
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "net/cert/internal/trust_store_chrome.h"
#endif  // CHROME_ROOT_STORE_SUPPORTED

namespace net {

namespace {

class DummySystemTrustStore : public SystemTrustStore {
 public:
  TrustStore* GetTrustStore() override { return &trust_store_; }

  bool UsesSystemTrustStore() const override { return false; }

  bool IsKnownRoot(const ParsedCertificate* trust_anchor) const override {
    return false;
  }

 private:
  TrustStoreCollection trust_store_;
};

}  // namespace

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
class SystemTrustStoreChrome : public SystemTrustStore {
 public:
  explicit SystemTrustStoreChrome(
      std::unique_ptr<TrustStoreChrome> trust_store_chrome,
      std::unique_ptr<TrustStore> trust_store_system)
      : trust_store_chrome_(std::move(trust_store_chrome)),
        trust_store_system_(std::move(trust_store_system)) {
    trust_store_collection_.AddTrustStore(trust_store_chrome_.get());
    trust_store_collection_.AddTrustStore(trust_store_system_.get());
  }

  TrustStore* GetTrustStore() override { return &trust_store_collection_; }

  bool UsesSystemTrustStore() const override { return true; }

  // IsKnownRoot returns true if the given trust anchor is a standard one (as
  // opposed to a user-installed root)
  bool IsKnownRoot(const ParsedCertificate* trust_anchor) const override {
    return trust_store_chrome_->Contains(trust_anchor);
  }

 private:
  std::unique_ptr<TrustStoreChrome> trust_store_chrome_;
  std::unique_ptr<TrustStore> trust_store_system_;
  TrustStoreCollection trust_store_collection_;
};

std::unique_ptr<SystemTrustStore> CreateSystemTrustStoreChromeForTesting(
    std::unique_ptr<TrustStoreChrome> trust_store_chrome,
    std::unique_ptr<TrustStore> trust_store_system) {
  return std::make_unique<SystemTrustStoreChrome>(
      std::move(trust_store_chrome), std::move(trust_store_system));
}

#endif  // CHROME_ROOT_STORE_SUPPORTED

#if BUILDFLAG(USE_NSS_CERTS)
namespace {

class SystemTrustStoreNSS : public SystemTrustStore {
 public:
  explicit SystemTrustStoreNSS(std::unique_ptr<TrustStoreNSS> trust_store_nss)
      : trust_store_nss_(std::move(trust_store_nss)) {}

  TrustStore* GetTrustStore() override { return trust_store_nss_.get(); }

  bool UsesSystemTrustStore() const override { return true; }

  // IsKnownRoot returns true if the given trust anchor is a standard one (as
  // opposed to a user-installed root)
  bool IsKnownRoot(const ParsedCertificate* trust_anchor) const override {
    // TODO(eroman): The overall approach of IsKnownRoot() is inefficient -- it
    // requires searching for the trust anchor by DER in NSS, however path
    // building already had a handle to it.
    SECItem der_cert;
    der_cert.data = const_cast<uint8_t*>(trust_anchor->der_cert().UnsafeData());
    der_cert.len = trust_anchor->der_cert().Length();
    der_cert.type = siDERCertBuffer;
    ScopedCERTCertificate nss_cert(
        CERT_FindCertByDERCert(CERT_GetDefaultCertDB(), &der_cert));
    if (!nss_cert)
      return false;

    if (!net::IsKnownRoot(nss_cert.get()))
      return false;

    return trust_anchor->der_cert() ==
           der::Input(nss_cert->derCert.data, nss_cert->derCert.len);
  }

 private:
  std::unique_ptr<TrustStoreNSS> trust_store_nss_;
};

}  // namespace

std::unique_ptr<SystemTrustStore> CreateSslSystemTrustStore() {
  return std::make_unique<SystemTrustStoreNSS>(
      std::make_unique<TrustStoreNSS>(trustSSL));
}

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
std::unique_ptr<SystemTrustStore> CreateSslSystemTrustStoreChromeRoot(
    std::unique_ptr<TrustStoreChrome> chrome_root) {
  return std::make_unique<SystemTrustStoreChrome>(
      std::move(chrome_root),
      std::make_unique<TrustStoreNSS>(
          trustSSL, TrustStoreNSS::IgnoreSystemTrustSettings()));
}

#endif  // CHROME_ROOT_STORE_SUPPORTED

std::unique_ptr<SystemTrustStore>
CreateSslSystemTrustStoreNSSWithUserSlotRestriction(
    crypto::ScopedPK11Slot user_slot) {
  return std::make_unique<SystemTrustStoreNSS>(
      std::make_unique<TrustStoreNSS>(trustSSL, std::move(user_slot)));
}

std::unique_ptr<SystemTrustStore>
CreateSslSystemTrustStoreNSSWithNoUserSlots() {
  return std::make_unique<SystemTrustStoreNSS>(std::make_unique<TrustStoreNSS>(
      trustSSL, TrustStoreNSS::DisallowTrustForCertsOnUserSlots()));
}

#elif BUILDFLAG(IS_MAC)

class SystemTrustStoreMac : public SystemTrustStore {
 public:
  SystemTrustStoreMac() = default;

  TrustStore* GetTrustStore() override { return GetGlobalTrustStoreMac(); }

  bool UsesSystemTrustStore() const override { return true; }

  // IsKnownRoot returns true if the given trust anchor is a standard one (as
  // opposed to a user-installed root)
  bool IsKnownRoot(const ParsedCertificate* trust_anchor) const override {
    return GetGlobalTrustStoreMac()->IsKnownRoot(trust_anchor);
  }

  static void InitializeTrustCacheOnWorkerThread() {
    GetGlobalTrustStoreMac()->InitializeTrustCache();
  }

 private:
  static constexpr TrustStoreMac::TrustImplType kDefaultTrustImpl =
      TrustStoreMac::TrustImplType::kLruCache;

  static TrustStoreMac::TrustImplType ParamToTrustImplType(int param) {
    switch (param) {
      case 1:
        return TrustStoreMac::TrustImplType::kDomainCache;
      case 2:
        return TrustStoreMac::TrustImplType::kSimple;
      case 3:
        return TrustStoreMac::TrustImplType::kLruCache;
      default:
        return kDefaultTrustImpl;
    }
  }

  static TrustStoreMac::TrustImplType GetTrustStoreImplParam() {
    if (base::FeatureList::IsEnabled(features::kCertVerifierBuiltinFeature))
      return ParamToTrustImplType(features::kCertVerifierBuiltinImpl.Get());
    if (base::FeatureList::IsEnabled(
            features::kCertDualVerificationTrialFeature))
      return ParamToTrustImplType(
          features::kCertDualVerificationTrialImpl.Get());
    return kDefaultTrustImpl;
  }

  static size_t GetTrustStoreCacheSize() {
    if (base::FeatureList::IsEnabled(features::kCertVerifierBuiltinFeature) &&
        features::kCertVerifierBuiltinCacheSize.Get() > 0) {
      return features::kCertVerifierBuiltinCacheSize.Get();
    }
    if (base::FeatureList::IsEnabled(
            features::kCertDualVerificationTrialFeature) &&
        features::kCertDualVerificationTrialCacheSize.Get() > 0) {
      return features::kCertDualVerificationTrialCacheSize.Get();
    }
    constexpr size_t kDefaultCacheSize = 512;
    return kDefaultCacheSize;
  }

  static TrustStoreMac* GetGlobalTrustStoreMac() {
    static base::NoDestructor<TrustStoreMac> static_trust_store_mac(
        kSecPolicyAppleSSL, GetTrustStoreImplParam(), GetTrustStoreCacheSize());
    return static_trust_store_mac.get();
  }
};

std::unique_ptr<SystemTrustStore> CreateSslSystemTrustStore() {
  return std::make_unique<SystemTrustStoreMac>();
}

void InitializeTrustStoreMacCache() {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SystemTrustStoreMac::InitializeTrustCacheOnWorkerThread));
}

#elif BUILDFLAG(IS_FUCHSIA)

namespace {

constexpr char kRootCertsFileFuchsia[] = "/config/ssl/cert.pem";

class FuchsiaSystemCerts {
 public:
  FuchsiaSystemCerts() {
    base::FilePath filename(kRootCertsFileFuchsia);
    std::string certs_file;
    if (!base::ReadFileToString(filename, &certs_file)) {
      LOG(ERROR) << "Can't load root certificates from " << filename;
      return;
    }

    CertificateList certs = X509Certificate::CreateCertificateListFromBytes(
        base::as_bytes(base::make_span(certs_file)),
        X509Certificate::FORMAT_AUTO);

    for (const auto& cert : certs) {
      CertErrors errors;
      auto parsed = ParsedCertificate::Create(
          bssl::UpRef(cert->cert_buffer()),
          x509_util::DefaultParseCertificateOptions(), &errors);
      CHECK(parsed) << errors.ToDebugString();
      system_trust_store_.AddTrustAnchor(parsed);
    }
  }

  TrustStoreInMemory* system_trust_store() { return &system_trust_store_; }

 private:
  TrustStoreInMemory system_trust_store_;
};

base::LazyInstance<FuchsiaSystemCerts>::Leaky g_root_certs_fuchsia =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

class SystemTrustStoreFuchsia : public SystemTrustStore {
 public:
  SystemTrustStoreFuchsia() = default;

  TrustStore* GetTrustStore() override {
    return g_root_certs_fuchsia.Get().system_trust_store();
  }

  bool UsesSystemTrustStore() const override { return true; }

  bool IsKnownRoot(const ParsedCertificate* trust_anchor) const override {
    return g_root_certs_fuchsia.Get().system_trust_store()->Contains(
        trust_anchor);
  }
};

std::unique_ptr<SystemTrustStore> CreateSslSystemTrustStore() {
  return std::make_unique<SystemTrustStoreFuchsia>();
}

#elif BUILDFLAG(IS_WIN)

// Using the Builtin Verifier w/o the Chrome Root Store is unsupported on
// Windows.
std::unique_ptr<SystemTrustStore> CreateSslSystemTrustStore() {
  return std::make_unique<DummySystemTrustStore>();
}

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

std::unique_ptr<SystemTrustStore> CreateSslSystemTrustStoreChromeRoot(
    std::unique_ptr<TrustStoreChrome> chrome_root) {
  return std::make_unique<SystemTrustStoreChrome>(std::move(chrome_root),
                                                  TrustStoreWin::Create());
}

#endif  // CHROME_ROOT_STORE_SUPPORTED

#else

std::unique_ptr<SystemTrustStore> CreateSslSystemTrustStore() {
  return std::make_unique<DummySystemTrustStore>();
}

#endif

std::unique_ptr<SystemTrustStore> CreateEmptySystemTrustStore() {
  return std::make_unique<DummySystemTrustStore>();
}

}  // namespace net
