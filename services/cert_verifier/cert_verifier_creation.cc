// Copyright 2020 The Chromium Authors. All rights reserved.
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

#if BUILDFLAG(IS_CHROMEOS)
#include "crypto/nss_util_internal.h"
#include "net/cert/cert_verify_proc_builtin.h"
#include "net/cert/internal/system_trust_store.h"
#include "net/cert/internal/system_trust_store_nss.h"
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "net/cert/cert_verify_proc_builtin.h"
#include "net/cert/internal/system_trust_store.h"
#endif

#if BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)
#include "services/cert_verifier/trial_comparison_cert_verifier_mojo.h"
#endif

namespace cert_verifier {

namespace {

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
bool UsingBuiltinCertVerifier(
    mojom::CertVerifierCreationParams::CertVerifierImpl mode) {
  switch (mode) {
    case mojom::CertVerifierCreationParams::CertVerifierImpl::kDefault:
      return base::FeatureList::IsEnabled(
          net::features::kCertVerifierBuiltinFeature);
    case mojom::CertVerifierCreationParams::CertVerifierImpl::kBuiltin:
      return true;
    case mojom::CertVerifierCreationParams::CertVerifierImpl::kSystem:
      return false;
  }
}
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

bool UsingChromeRootStore(
    mojom::CertVerifierCreationParams::ChromeRootImpl mode) {
  switch (mode) {
    case mojom::CertVerifierCreationParams::ChromeRootImpl::kRootDefault:
      return base::FeatureList::IsEnabled(net::features::kChromeRootStoreUsed);
    case mojom::CertVerifierCreationParams::ChromeRootImpl::kRootChrome:
      return true;
    case mojom::CertVerifierCreationParams::ChromeRootImpl::kRootSystem:
      return false;
  }
}

#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

#if BUILDFLAG(IS_CHROMEOS)
scoped_refptr<net::CertVerifyProc> CreateCertVerifyProcForUser(
    scoped_refptr<net::CertNetFetcher> net_fetcher,
    crypto::ScopedPK11Slot user_public_slot) {
  return net::CreateCertVerifyProcBuiltin(
      std::move(net_fetcher),
      net::CreateSslSystemTrustStoreNSSWithUserSlotRestriction(
          std::move(user_public_slot)));
}

scoped_refptr<net::CertVerifyProc> CreateCertVerifyProcWithoutUserSlots(
    scoped_refptr<net::CertNetFetcher> net_fetcher) {
  return net::CreateCertVerifyProcBuiltin(
      std::move(net_fetcher),
      net::CreateSslSystemTrustStoreNSSWithNoUserSlots());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Create the CertVerifyProc that is well-tested and stable for the platform in
// question.
scoped_refptr<net::CertVerifyProc> CreateOldDefaultWithoutCaching(
    scoped_refptr<net::CertNetFetcher> cert_net_fetcher) {
  scoped_refptr<net::CertVerifyProc> verify_proc;
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  verify_proc =
      net::CertVerifyProc::CreateBuiltinVerifyProc(std::move(cert_net_fetcher));
#else
  verify_proc =
      net::CertVerifyProc::CreateSystemVerifyProc(std::move(cert_net_fetcher));
#endif
  return verify_proc;
}

// In certain instances/platforms, we are trying to roll out a new
// CertVerifyProc (or a CertVerifyProc with different arguments). Create this
// new version, if one exists. Otherwise, we will defer to
// CreateOldDefaultWithoutCaching().
scoped_refptr<net::CertVerifyProc> CreateNewDefaultWithoutCaching(
    scoped_refptr<net::CertNetFetcher> cert_net_fetcher) {
  scoped_refptr<net::CertVerifyProc> verify_proc;
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN))
  verify_proc = net::CreateCertVerifyProcBuiltin(
      std::move(cert_net_fetcher), net::CreateSslSystemTrustStoreChromeRoot());
#elif BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
  verify_proc =
      net::CertVerifyProc::CreateBuiltinVerifyProc(std::move(cert_net_fetcher));
#else
  verify_proc = CreateOldDefaultWithoutCaching(std::move(cert_net_fetcher));
#endif
  return verify_proc;
}

}  // namespace

bool IsUsingCertNetFetcher() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) ||      \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) ||       \
    BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED) || \
    BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
  return true;
#else
  return false;
#endif
}

std::unique_ptr<net::CertVerifier> CreateCertVerifier(
    mojom::CertVerifierCreationParams* creation_params,
    scoped_refptr<net::CertNetFetcher> cert_net_fetcher) {
  DCHECK(cert_net_fetcher || !IsUsingCertNetFetcher());
  std::unique_ptr<net::CertVerifier> cert_verifier;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!cert_verifier) {
    scoped_refptr<net::CertVerifyProc> verify_proc;
    if (!creation_params || !creation_params->nss_full_path.has_value()) {
      verify_proc =
          CreateCertVerifyProcWithoutUserSlots(std::move(cert_net_fetcher));
    } else {
      crypto::ScopedPK11Slot public_slot =
          crypto::OpenSoftwareNSSDB(creation_params->nss_full_path.value(),
                                    /*description=*/"cert_db");
      // `public_slot` can contain important security related settings. Crash if
      // failed to load it.
      CHECK(public_slot);
      verify_proc = CreateCertVerifyProcForUser(std::move(cert_net_fetcher),
                                                std::move(public_slot));
    }
    cert_verifier = std::make_unique<net::MultiThreadedCertVerifier>(
        std::move(verify_proc));
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // CHROMEOS_ASH does something special, so we do this first before we do
  // anything else. If the trial comparisons feature ever gets supported in
  // CHROMEOS_ASH we'll need to fix this.
  if (!cert_verifier) {
    scoped_refptr<net::CertVerifyProc> verify_proc;
    if (!creation_params || creation_params->username_hash.empty()) {
      verify_proc =
          CreateCertVerifyProcWithoutUserSlots(std::move(cert_net_fetcher));
    } else {
      // Make sure NSS is initialized for the user.
      crypto::InitializeNSSForChromeOSUser(creation_params->username_hash,
                                           creation_params->nss_path.value());

      crypto::ScopedPK11Slot public_slot =
          crypto::GetPublicSlotForChromeOSUser(creation_params->username_hash);
      verify_proc = CreateCertVerifyProcForUser(std::move(cert_net_fetcher),
                                                std::move(public_slot));
    }
    cert_verifier = std::make_unique<net::MultiThreadedCertVerifier>(
        std::move(verify_proc));
  }
#endif

#if BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)
  // If we're doing trial verification, we always do it between the old
  // default and the proposed new default, giving the user the value computed
  // by the old default.
  if (!cert_verifier && creation_params &&
      creation_params->trial_comparison_cert_verifier_params) {
    scoped_refptr<net::CertVerifyProc> primary =
        CreateOldDefaultWithoutCaching(cert_net_fetcher);
    scoped_refptr<net::CertVerifyProc> trial =
        CreateNewDefaultWithoutCaching(cert_net_fetcher);

    cert_verifier = std::make_unique<TrialComparisonCertVerifierMojo>(
        creation_params->trial_comparison_cert_verifier_params->initial_allowed,
        std::move(creation_params->trial_comparison_cert_verifier_params
                      ->config_client_receiver),
        std::move(creation_params->trial_comparison_cert_verifier_params
                      ->report_client),
        primary, trial);
  }
#endif

  bool use_new_default_for_platform = true;
#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
  use_new_default_for_platform &=
      creation_params
          ? UsingBuiltinCertVerifier(creation_params->use_builtin_cert_verifier)
          : UsingBuiltinCertVerifier(
                mojom::CertVerifierCreationParams::CertVerifierImpl::kDefault);
#endif
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  use_new_default_for_platform &=
      creation_params
          ? UsingChromeRootStore(creation_params->use_chrome_root_store)
          : UsingChromeRootStore(mojom::CertVerifierCreationParams::
                                     ChromeRootImpl::kRootDefault);
#endif

  if (!cert_verifier) {
    scoped_refptr<net::CertVerifyProc> verify_proc;
    // If we're trying to use the new cert verifier for the platform (either
    // the builtin_verifier, or the Chrome Root Store, or both at the same
    // time), use the new default. Otherwise use the old default.
    if (use_new_default_for_platform) {
      verify_proc = CreateNewDefaultWithoutCaching(std::move(cert_net_fetcher));
    } else {
      verify_proc = CreateOldDefaultWithoutCaching(std::move(cert_net_fetcher));
    }

    cert_verifier =
        std::make_unique<net::MultiThreadedCertVerifier>(verify_proc);
  }

  return cert_verifier;
}

}  // namespace cert_verifier
