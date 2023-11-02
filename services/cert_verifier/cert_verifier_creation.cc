// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/cert_verifier_creation.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/base/features.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/cert_net/cert_net_fetcher_url_request.h"
#include "net/net_buildflags.h"

#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
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

#if BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)
#include "services/cert_verifier/trial_comparison_cert_verifier_mojo.h"
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

// CertVerifyProcFactory that returns a CertVerifyProc that supports the old
// configuration for platforms where we are transitioning from one cert
// configuration to another. If the platform only supports one configuration,
// return a CertVerifyProc that supports that configuration.
class OldDefaultCertVerifyProcFactory : public net::CertVerifyProcFactory {
 public:
  explicit OldDefaultCertVerifyProcFactory(
      mojom::CertVerifierCreationParams* creation_params) {
#if BUILDFLAG(IS_CHROMEOS)
    user_slot_restriction_ =
        GetUserSlotRestrictionForChromeOSParams(creation_params);
#endif
  }

  scoped_refptr<net::CertVerifyProc> CreateCertVerifyProc(
      scoped_refptr<net::CertNetFetcher> cert_net_fetcher,
      const net::ChromeRootStoreData* root_store_data) override {
    scoped_refptr<net::CertVerifyProc> verify_proc;
#if BUILDFLAG(IS_CHROMEOS)
    verify_proc = net::CreateCertVerifyProcBuiltin(
        std::move(cert_net_fetcher),
        net::CreateSslSystemTrustStoreNSSWithUserSlotRestriction(
            user_slot_restriction_ ? crypto::ScopedPK11Slot(PK11_ReferenceSlot(
                                         user_slot_restriction_.get()))
                                   : nullptr));
#elif BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX)
    verify_proc = net::CreateCertVerifyProcBuiltin(
        std::move(cert_net_fetcher), net::CreateSslSystemTrustStore());
#else
    verify_proc = net::CertVerifyProc::CreateSystemVerifyProc(
        std::move(cert_net_fetcher));
#endif
    return verify_proc;
  }

 protected:
  ~OldDefaultCertVerifyProcFactory() override = default;

#if BUILDFLAG(IS_CHROMEOS)
  crypto::ScopedPK11Slot user_slot_restriction_;
#endif
};

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
// CertVerifyProcFactory that returns a CertVerifyProc that uses the
// Chrome Cert Verifier with the Chrome Root Store.
class NewCertVerifyProcChromeRootStoreFactory
    : public net::CertVerifyProcFactory {
 public:
  explicit NewCertVerifyProcChromeRootStoreFactory(
      mojom::CertVerifierCreationParams* creation_params) {
#if BUILDFLAG(IS_CHROMEOS)
    user_slot_restriction_ =
        GetUserSlotRestrictionForChromeOSParams(creation_params);
#endif
  }

  scoped_refptr<net::CertVerifyProc> CreateCertVerifyProc(
      scoped_refptr<net::CertNetFetcher> cert_net_fetcher,
      const net::ChromeRootStoreData* root_store_data) override {
    std::unique_ptr<net::TrustStoreChrome> chrome_root;
    if (!root_store_data) {
      chrome_root = std::make_unique<net::TrustStoreChrome>();
    } else {
      chrome_root = std::make_unique<net::TrustStoreChrome>(*root_store_data);
    }
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
    return net::CreateCertVerifyProcBuiltin(std::move(cert_net_fetcher),
                                            std::move(trust_store));
  }

 protected:
  ~NewCertVerifyProcChromeRootStoreFactory() override = default;

#if BUILDFLAG(IS_CHROMEOS)
  crypto::ScopedPK11Slot user_slot_restriction_;
#endif
};
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

#if BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)
// Returns true if creation_params are requesting the creation of a
// TrialComparisonCertVerifier.
bool IsTrialVerificationOn(
    const mojom::CertVerifierCreationParams* creation_params) {
#if BUILDFLAG(IS_CHROMEOS)
#error "Trial comparisons not supported on ChromeOS yet. Code changes needed."
#endif
  // Check to see if we have trial comparison cert verifier params.
  return creation_params &&
         creation_params->trial_comparison_cert_verifier_params;
}

// Should only be called if IsTrialVerificationOn(creation_params) == true.
std::unique_ptr<net::CertVerifierWithUpdatableProc> CreateTrialCertVerifier(
    mojom::CertVerifierCreationParams* creation_params,
    scoped_refptr<net::CertNetFetcher> cert_net_fetcher,
    const net::ChromeRootStoreData* root_store_data) {
  DCHECK(IsTrialVerificationOn(creation_params));

  // If we're doing trial verification, we always do it between the old
  // default and the proposed new default, giving the user the value computed
  // by the old default.
  auto primary_proc_factory =
      base::MakeRefCounted<OldDefaultCertVerifyProcFactory>(creation_params);
  scoped_refptr<net::CertVerifyProc> primary_proc =
      primary_proc_factory->CreateCertVerifyProc(cert_net_fetcher,
                                                 root_store_data);

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  auto trial_proc_factory =
      base::MakeRefCounted<NewCertVerifyProcChromeRootStoreFactory>(
          creation_params);
#else
#error "CHROME_ROOT_STORE_SUPPORTED must be true"
#endif

  scoped_refptr<net::CertVerifyProc> trial_proc =
      trial_proc_factory->CreateCertVerifyProc(cert_net_fetcher,
                                               root_store_data);

  return std::make_unique<TrialComparisonCertVerifierMojo>(
      creation_params->trial_comparison_cert_verifier_params->initial_allowed,
      std::move(creation_params->trial_comparison_cert_verifier_params
                    ->config_client_receiver),
      std::move(creation_params->trial_comparison_cert_verifier_params
                    ->report_client),
      std::move(primary_proc), std::move(primary_proc_factory),
      std::move(trial_proc), std::move(trial_proc_factory));
}
#endif  // BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)

}  // namespace

bool IsUsingCertNetFetcher() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) ||      \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) ||       \
    BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED) || \
    BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  return true;
#else
  return false;
#endif
}

std::unique_ptr<net::CertVerifierWithUpdatableProc> CreateCertVerifier(
    mojom::CertVerifierServiceParams* impl_params,
    mojom::CertVerifierCreationParams* creation_params,
    scoped_refptr<net::CertNetFetcher> cert_net_fetcher,
    const net::ChromeRootStoreData* root_store_data) {
  DCHECK(cert_net_fetcher || !IsUsingCertNetFetcher());
  std::unique_ptr<net::CertVerifierWithUpdatableProc> cert_verifier;

#if BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)
  if (!cert_verifier && IsTrialVerificationOn(creation_params)) {
    cert_verifier = CreateTrialCertVerifier(creation_params, cert_net_fetcher,
                                            root_store_data);
  }
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  if (!cert_verifier && impl_params->use_chrome_root_store) {
    scoped_refptr<NewCertVerifyProcChromeRootStoreFactory> proc_factory =
        base::MakeRefCounted<NewCertVerifyProcChromeRootStoreFactory>(
            creation_params);
    cert_verifier = std::make_unique<net::MultiThreadedCertVerifier>(
        proc_factory->CreateCertVerifyProc(cert_net_fetcher, root_store_data),
        proc_factory);
  }
#endif

  if (!cert_verifier) {
    scoped_refptr<OldDefaultCertVerifyProcFactory> proc_factory =
        base::MakeRefCounted<OldDefaultCertVerifyProcFactory>(creation_params);
    cert_verifier = std::make_unique<net::MultiThreadedCertVerifier>(
        proc_factory->CreateCertVerifyProc(cert_net_fetcher, root_store_data),
        proc_factory);
  }
  return cert_verifier;
}

}  // namespace cert_verifier
