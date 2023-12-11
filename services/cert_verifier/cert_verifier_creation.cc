// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/cert_verifier_creation.h"

#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/base/features.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/crl_set.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/net_buildflags.h"

#if BUILDFLAG(IS_FUCHSIA)
#include "net/cert/cert_verify_proc_builtin.h"
#include "net/cert/internal/system_trust_store.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "crypto/nss_util_internal.h"
#include "net/cert/internal/system_trust_store_nss.h"
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "net/cert/cert_verify_proc_builtin.h"
#include "net/cert/internal/system_trust_store.h"
#include "net/cert/internal/trust_store_chrome.h"
#endif

namespace cert_verifier {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
crypto::ScopedPK11Slot GetUserSlotRestrictionForChromeOSParams(
    mojom::CertVerifierCreationParams* creation_params) {
  crypto::ScopedPK11Slot public_slot;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (creation_params && creation_params->nss_full_path.has_value()) {
    public_slot =
        crypto::OpenSoftwareNSSDB(creation_params->nss_full_path.value(),
                                  /*description=*/"cert_db");
    // `public_slot` can contain important security related settings. Crash if
    // failed to load it.
    CHECK(public_slot);
  }
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  if (creation_params && !creation_params->username_hash.empty()) {
    // Make sure NSS is initialized for the user.
    crypto::InitializeNSSForChromeOSUser(creation_params->username_hash,
                                         creation_params->nss_path.value());
    public_slot =
        crypto::GetPublicSlotForChromeOSUser(creation_params->username_hash);
  }
#else
#error IS_CHROMEOS set without IS_CHROMEOS_LACROS or IS_CHROMEOS_ASH
#endif
  return public_slot;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

class CertVerifyProcFactoryImpl : public net::CertVerifyProcFactory {
 public:
  explicit CertVerifyProcFactoryImpl(
      mojom::CertVerifierCreationParams* creation_params) {
#if BUILDFLAG(IS_CHROMEOS)
    user_slot_restriction_ =
        GetUserSlotRestrictionForChromeOSParams(creation_params);
#endif
  }

  scoped_refptr<net::CertVerifyProc> CreateCertVerifyProc(
      scoped_refptr<net::CertNetFetcher> cert_net_fetcher,
      const net::CertVerifyProc::ImplParams& impl_params,
      const net::CertVerifyProc::InstanceParams& instance_params) override {
#if BUILDFLAG(CHROME_ROOT_STORE_ONLY)
    return CreateNewCertVerifyProc(
        cert_net_fetcher, impl_params.crl_set,
        base::OptionalToPtr(impl_params.root_store_data), instance_params);
#else
#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
    if (impl_params.use_chrome_root_store) {
      return CreateNewCertVerifyProc(
          cert_net_fetcher, impl_params.crl_set,
          base::OptionalToPtr(impl_params.root_store_data), instance_params);
    }
#endif
    return CreateOldCertVerifyProc(cert_net_fetcher, impl_params.crl_set,
                                   instance_params);
#endif
  }

 protected:
  ~CertVerifyProcFactoryImpl() override = default;

#if !BUILDFLAG(CHROME_ROOT_STORE_ONLY)
  // Factory function that returns a CertVerifyProc that supports the old
  // configuration for platforms where we are transitioning from one cert
  // configuration to another. If the platform only supports one configuration,
  // return a CertVerifyProc that supports that configuration.
  scoped_refptr<net::CertVerifyProc> CreateOldCertVerifyProc(
      scoped_refptr<net::CertNetFetcher> cert_net_fetcher,
      scoped_refptr<net::CRLSet> crl_set,
      const net::CertVerifyProc::InstanceParams& instance_params) {
#if BUILDFLAG(IS_FUCHSIA)
    return net::CreateCertVerifyProcBuiltin(
        std::move(cert_net_fetcher), std::move(crl_set),
        net::CreateSslSystemTrustStore(), instance_params);
#else
    return net::CertVerifyProc::CreateSystemVerifyProc(
        std::move(cert_net_fetcher), std::move(crl_set));
#endif
  }
#endif  // !BUILDFLAG(CHROME_ROOT_STORE_ONLY)

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  // CertVerifyProcFactory that returns a CertVerifyProc that uses the
  // Chrome Cert Verifier with the Chrome Root Store.
  scoped_refptr<net::CertVerifyProc> CreateNewCertVerifyProc(
      scoped_refptr<net::CertNetFetcher> cert_net_fetcher,
      scoped_refptr<net::CRLSet> crl_set,
      const net::ChromeRootStoreData* root_store_data,
      const net::CertVerifyProc::InstanceParams& instance_params) {
    std::unique_ptr<net::TrustStoreChrome> chrome_root =
        root_store_data
            ? std::make_unique<net::TrustStoreChrome>(*root_store_data)
            : std::make_unique<net::TrustStoreChrome>();

    std::unique_ptr<net::SystemTrustStore> trust_store;
#if BUILDFLAG(IS_CHROMEOS)
    trust_store =
        net::CreateSslSystemTrustStoreChromeRootWithUserSlotRestriction(
            std::move(chrome_root),
            user_slot_restriction_ ? crypto::ScopedPK11Slot(PK11_ReferenceSlot(
                                         user_slot_restriction_.get()))
                                   : nullptr);
#else
    trust_store =
        net::CreateSslSystemTrustStoreChromeRoot(std::move(chrome_root));
#endif

#if BUILDFLAG(IS_WIN)
    // Start initialization of TrustStoreWin on a separate thread if it hasn't
    // been done already. We do this here instead of in the TrustStoreWin
    // constructor to avoid any unnecessary threading in unit tests that don't
    // use threads otherwise.
    net::InitializeTrustStoreWinSystem();
#endif
#if BUILDFLAG(IS_ANDROID)
    // Start initialization of TrustStoreAndroid on a separate thread if it
    // hasn't been done already. We do this here instead of in the
    // TrustStoreAndroid constructor to avoid any unnecessary threading in unit
    // tests that don't use threads otherwise.
    net::InitializeTrustStoreAndroid();
#endif
    return net::CreateCertVerifyProcBuiltin(
        std::move(cert_net_fetcher), std::move(crl_set), std::move(trust_store),
        instance_params);
  }
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

#if BUILDFLAG(IS_CHROMEOS)
  crypto::ScopedPK11Slot user_slot_restriction_;
#endif
};

}  // namespace

bool IsUsingCertNetFetcher() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) ||      \
    BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  return true;
#else
  return false;
#endif
}

std::unique_ptr<net::CertVerifierWithUpdatableProc> CreateCertVerifier(
    mojom::CertVerifierCreationParams* creation_params,
    scoped_refptr<net::CertNetFetcher> cert_net_fetcher,
    const net::CertVerifyProc::ImplParams& impl_params,
    const net::CertVerifyProc::InstanceParams& instance_params) {
  DCHECK(cert_net_fetcher || !IsUsingCertNetFetcher());

  scoped_refptr<net::CertVerifyProcFactory> proc_factory =
      base::MakeRefCounted<CertVerifyProcFactoryImpl>(creation_params);
  return std::make_unique<net::MultiThreadedCertVerifier>(
      proc_factory->CreateCertVerifyProc(cert_net_fetcher, impl_params,
                                         instance_params),
      proc_factory);
}

}  // namespace cert_verifier
