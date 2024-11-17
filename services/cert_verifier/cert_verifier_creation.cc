// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/cert_verifier_creation.h"

#include "base/memory/scoped_refptr.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/network_time/time_tracker/time_tracker.h"
#include "crypto/sha2.h"
#include "net/base/features.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/crl_set.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/cert/x509_util.h"
#include "net/net_buildflags.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/mojom/cert_verifier_service_updater.mojom.h"

#if BUILDFLAG(IS_CT_SUPPORTED)
#include "net/cert/multi_log_ct_verifier.h"
#endif

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
    std::unique_ptr<net::CTVerifier> ct_verifier;
    scoped_refptr<net::CTPolicyEnforcer> ct_policy_enforcer;
#if BUILDFLAG(IS_CT_SUPPORTED)
    if (!impl_params.ct_logs.empty()) {
      ct_verifier =
          std::make_unique<net::MultiLogCTVerifier>(impl_params.ct_logs);
    }
    ct_policy_enforcer = impl_params.ct_policy_enforcer;
#endif
    if (!ct_verifier) {
      ct_verifier = std::make_unique<net::DoNothingCTVerifier>();
    }
    if (!ct_policy_enforcer) {
      ct_policy_enforcer = base::MakeRefCounted<net::DefaultCTPolicyEnforcer>();
    }
#if BUILDFLAG(CHROME_ROOT_STORE_ONLY)
    return CreateNewCertVerifyProc(
        cert_net_fetcher, impl_params.crl_set, std::move(ct_verifier),
        std::move(ct_policy_enforcer),
        base::OptionalToPtr(impl_params.root_store_data), instance_params,
        impl_params.time_tracker);
#else
#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
    if (impl_params.use_chrome_root_store) {
      return CreateNewCertVerifyProc(
          cert_net_fetcher, impl_params.crl_set, std::move(ct_verifier),
          std::move(ct_policy_enforcer),
          base::OptionalToPtr(impl_params.root_store_data), instance_params,
          impl_params.time_tracker);
    }
#endif
    return CreateOldCertVerifyProc(cert_net_fetcher, impl_params.crl_set,
                                   std::move(ct_verifier),
                                   std::move(ct_policy_enforcer),
                                   instance_params, impl_params.time_tracker);
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
      std::unique_ptr<net::CTVerifier> ct_verifier,
      scoped_refptr<net::CTPolicyEnforcer> ct_policy_enforcer,
      const net::CertVerifyProc::InstanceParams& instance_params,
      std::optional<network_time::TimeTracker> time_tracker) {
#if BUILDFLAG(IS_FUCHSIA)
    return net::CreateCertVerifyProcBuiltin(
        std::move(cert_net_fetcher), std::move(crl_set), std::move(ct_verifier),
        std::move(ct_policy_enforcer), net::CreateSslSystemTrustStore(),
        instance_params, std::move(time_tracker));
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
      std::unique_ptr<net::CTVerifier> ct_verifier,
      scoped_refptr<net::CTPolicyEnforcer> ct_policy_enforcer,
      const net::ChromeRootStoreData* root_store_data,
      const net::CertVerifyProc::InstanceParams& instance_params,
      std::optional<network_time::TimeTracker> time_tracker) {
    std::unique_ptr<net::TrustStoreChrome> chrome_root =
        root_store_data
            ? std::make_unique<net::TrustStoreChrome>(*root_store_data)
            : std::make_unique<net::TrustStoreChrome>();

    std::unique_ptr<net::SystemTrustStore> trust_store;
#if BUILDFLAG(IS_CHROMEOS)
    if (user_slot_restriction_) {
      trust_store =
          net::CreateSslSystemTrustStoreChromeRootWithUserSlotRestriction(
              std::move(chrome_root), crypto::ScopedPK11Slot(PK11_ReferenceSlot(
                                          user_slot_restriction_.get())));
    } else {
      trust_store =
          net::CreateChromeOnlySystemTrustStore(std::move(chrome_root));
    }
#else
    if (instance_params.include_system_trust_store) {
      trust_store =
          net::CreateSslSystemTrustStoreChromeRoot(std::move(chrome_root));
    } else {
      trust_store =
          net::CreateChromeOnlySystemTrustStore(std::move(chrome_root));
    }
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
        std::move(cert_net_fetcher), std::move(crl_set), std::move(ct_verifier),
        std::move(ct_policy_enforcer), std::move(trust_store), instance_params,
        std::move(time_tracker));
  }
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

#if BUILDFLAG(IS_CHROMEOS)
  crypto::ScopedPK11Slot user_slot_restriction_;
#endif
};

std::vector<net::CertVerifyProc::CertificateWithConstraints>
ConvertMojoListToInternalList(
    const std::vector<mojom::CertWithConstraintsPtr>& mojo_cert_list) {
  std::vector<net::CertVerifyProc::CertificateWithConstraints> cert_list;

  for (const auto& cert_with_constraints_mojo : mojo_cert_list) {
    bssl::UniquePtr<CRYPTO_BUFFER> cert_buffer =
        net::x509_util::CreateCryptoBuffer(
            cert_with_constraints_mojo->certificate);
    std::shared_ptr<const bssl::ParsedCertificate> cert =
        bssl::ParsedCertificate::Create(
            std::move(cert_buffer),
            net::x509_util::DefaultParseCertificateOptions(), nullptr);
    if (!cert) {
      continue;
    }

    net::CertVerifyProc::CertificateWithConstraints cert_with_constraints;
    cert_with_constraints.certificate = std::move(cert);
    cert_with_constraints.permitted_dns_names =
        cert_with_constraints_mojo->permitted_dns_names;

    for (const auto& cidr : cert_with_constraints_mojo->permitted_cidrs) {
      cert_with_constraints.permitted_cidrs.push_back({cidr->ip, cidr->mask});
    }

    cert_list.push_back(std::move(cert_with_constraints));
  }
  return cert_list;
}

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

void UpdateCertVerifierInstanceParams(
    const mojom::AdditionalCertificatesPtr& additional_certificates,
    net::CertVerifyProc::InstanceParams* instance_params) {
  instance_params->additional_trust_anchors =
      net::x509_util::ParseAllValidCerts(
          net::x509_util::ConvertToX509CertificatesIgnoreErrors(
              additional_certificates->trust_anchors));

  instance_params->additional_untrusted_authorities =
      net::x509_util::ParseAllValidCerts(
          net::x509_util::ConvertToX509CertificatesIgnoreErrors(
              additional_certificates->all_certificates));

  instance_params->additional_trust_anchors_with_enforced_constraints =
      net::x509_util::ParseAllValidCerts(
          net::x509_util::ConvertToX509CertificatesIgnoreErrors(
              additional_certificates
                  ->trust_anchors_with_enforced_constraints));

  instance_params->additional_distrusted_spkis =
      additional_certificates->distrusted_spkis;

#if !BUILDFLAG(IS_CHROMEOS)
  instance_params->include_system_trust_store =
      additional_certificates->include_system_trust_store;
#endif

  instance_params->additional_trust_anchors_with_constraints =
      ConvertMojoListToInternalList(
          additional_certificates->trust_anchors_with_additional_constraints);
  instance_params->additional_trust_anchors_and_leafs =
      ConvertMojoListToInternalList(
          additional_certificates->trust_anchors_and_leafs);
  instance_params->additional_trust_leafs =
      ConvertMojoListToInternalList(additional_certificates->trust_leafs);
}

}  // namespace cert_verifier
