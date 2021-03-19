// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/cert_verifier_creation.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/base/features.h"
#include "net/cert_net/cert_net_fetcher_url_request.h"
#include "net/net_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "crypto/nss_util_internal.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_proc_builtin.h"
#include "net/cert/internal/system_trust_store.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "services/cert_verifier/system_trust_store_provider_chromeos.h"
#endif

#if BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED) || \
    BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_proc_builtin.h"
#include "net/cert/multi_threaded_cert_verifier.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
scoped_refptr<net::CertVerifyProc> CreateCertVerifyProcForUser(
    scoped_refptr<net::CertNetFetcher> net_fetcher,
    crypto::ScopedPK11Slot user_public_slot) {
  return net::CreateCertVerifyProcBuiltin(
      std::move(net_fetcher),
      std::make_unique<SystemTrustStoreProviderChromeOS>(
          std::move(user_public_slot)));
}

scoped_refptr<net::CertVerifyProc> CreateCertVerifyProcWithoutUserSlots(
    scoped_refptr<net::CertNetFetcher> net_fetcher) {
  return net::CreateCertVerifyProcBuiltin(
      std::move(net_fetcher),
      std::make_unique<SystemTrustStoreProviderChromeOS>());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

bool IsUsingCertNetFetcher() {
#if defined(OS_ANDROID) || defined(OS_FUCHSIA) || defined(OS_CHROMEOS) || \
    defined(OS_LINUX) ||                                                  \
    BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED) ||                \
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

  bool use_builtin_cert_verifier;
#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
  use_builtin_cert_verifier =
      creation_params
          ? UsingBuiltinCertVerifier(creation_params->use_builtin_cert_verifier)
          : UsingBuiltinCertVerifier(
                mojom::CertVerifierCreationParams::CertVerifierImpl::kDefault);
#else
  use_builtin_cert_verifier = false;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  cert_verifier =
      std::make_unique<net::MultiThreadedCertVerifier>(std::move(verify_proc));
#endif
#if BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)
  if (!cert_verifier && creation_params &&
      creation_params->trial_comparison_cert_verifier_params) {
    cert_verifier = std::make_unique<TrialComparisonCertVerifierMojo>(
        creation_params->trial_comparison_cert_verifier_params->initial_allowed,
        std::move(creation_params->trial_comparison_cert_verifier_params
                      ->config_client_receiver),
        std::move(creation_params->trial_comparison_cert_verifier_params
                      ->report_client),
        net::CertVerifyProc::CreateSystemVerifyProc(cert_net_fetcher),
        net::CertVerifyProc::CreateBuiltinVerifyProc(cert_net_fetcher));
  }
#endif
#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
  if (!cert_verifier) {
    cert_verifier = std::make_unique<net::MultiThreadedCertVerifier>(
        use_builtin_cert_verifier
            ? net::CertVerifyProc::CreateBuiltinVerifyProc(
                  std::move(cert_net_fetcher))
            : net::CertVerifyProc::CreateSystemVerifyProc(
                  std::move(cert_net_fetcher)));
  }
#endif

  if (!cert_verifier) {
    cert_verifier = net::CertVerifier::CreateDefaultWithoutCaching(
        std::move(cert_net_fetcher));
  }

  return cert_verifier;
}
}  // namespace cert_verifier
