// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/trial_comparison_cert_verifier_mojo.h"

#include <utility>

#include "build/build_config.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_proc_builtin.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/trial_comparison_cert_verifier.h"
#include "net/der/encode_values.h"
#include "net/der/parse_values.h"
#include "net/net_buildflags.h"

#if BUILDFLAG(IS_MAC)
#include "net/cert/cert_verify_proc_mac.h"
#include "net/cert/internal/trust_store_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "net/cert/cert_verify_proc_win.h"
#endif

namespace {

#if BUILDFLAG(IS_MAC)
cert_verifier::mojom::CertVerifierDebugInfo::MacTrustImplType
TrustImplTypeToMojom(net::TrustStoreMac::TrustImplType input) {
  switch (input) {
    case net::TrustStoreMac::TrustImplType::kUnknown:
      return cert_verifier::mojom::CertVerifierDebugInfo::MacTrustImplType::
          kUnknown;
    case net::TrustStoreMac::TrustImplType::kSimple:
      return cert_verifier::mojom::CertVerifierDebugInfo::MacTrustImplType::
          kSimple;
    case net::TrustStoreMac::TrustImplType::kDomainCacheFullCerts:
      return cert_verifier::mojom::CertVerifierDebugInfo::MacTrustImplType::
          kDomainCacheFullCerts;
    case net::TrustStoreMac::TrustImplType::kKeychainCacheFullCerts:
      return cert_verifier::mojom::CertVerifierDebugInfo::MacTrustImplType::
          kKeychainCacheFullCerts;
  }
}
#endif

}  // namespace

namespace cert_verifier {

TrialComparisonCertVerifierMojo::TrialComparisonCertVerifierMojo(
    bool initial_allowed,
    mojo::PendingReceiver<mojom::TrialComparisonCertVerifierConfigClient>
        config_client_receiver,
    mojo::PendingRemote<mojom::TrialComparisonCertVerifierReportClient>
        report_client,
    scoped_refptr<net::CertVerifyProc> primary_verify_proc,
    scoped_refptr<net::CertVerifyProcFactory> primary_verify_proc_factory,
    scoped_refptr<net::CertVerifyProc> trial_verify_proc,
    scoped_refptr<net::CertVerifyProcFactory> trial_verify_proc_factory)
    : receiver_(this, std::move(config_client_receiver)),
      report_client_(std::move(report_client)) {
  trial_comparison_cert_verifier_ =
      std::make_unique<net::TrialComparisonCertVerifier>(
          primary_verify_proc, primary_verify_proc_factory, trial_verify_proc,
          trial_verify_proc_factory,
          base::BindRepeating(
              &TrialComparisonCertVerifierMojo::OnSendTrialReport,
              // Unretained safe because the report_callback will not be called
              // after trial_comparison_cert_verifier_ is destroyed.
              base::Unretained(this)));
  trial_comparison_cert_verifier_->set_trial_allowed(initial_allowed);
}

TrialComparisonCertVerifierMojo::~TrialComparisonCertVerifierMojo() = default;

int TrialComparisonCertVerifierMojo::Verify(
    const RequestParams& params,
    net::CertVerifyResult* verify_result,
    net::CompletionOnceCallback callback,
    std::unique_ptr<Request>* out_req,
    const net::NetLogWithSource& net_log) {
  return trial_comparison_cert_verifier_->Verify(
      params, verify_result, std::move(callback), out_req, net_log);
}

void TrialComparisonCertVerifierMojo::SetConfig(const Config& config) {
  trial_comparison_cert_verifier_->SetConfig(config);
}

void TrialComparisonCertVerifierMojo::UpdateChromeRootStoreData(
    scoped_refptr<net::CertNetFetcher> cert_net_fetcher,
    const net::ChromeRootStoreData* root_store_data) {
  trial_comparison_cert_verifier_->UpdateChromeRootStoreData(
      std::move(cert_net_fetcher), root_store_data);
}

void TrialComparisonCertVerifierMojo::OnTrialConfigUpdated(bool allowed) {
  trial_comparison_cert_verifier_->set_trial_allowed(allowed);
}

void TrialComparisonCertVerifierMojo::OnSendTrialReport(
    const std::string& hostname,
    const scoped_refptr<net::X509Certificate>& unverified_cert,
    bool enable_rev_checking,
    bool require_rev_checking_local_anchors,
    bool enable_sha1_local_anchors,
    bool disable_symantec_enforcement,
    const std::string& stapled_ocsp,
    const std::string& sct_list,
    const net::CertVerifyResult& primary_result,
    const net::CertVerifyResult& trial_result) {
  mojom::CertVerifierDebugInfoPtr debug_info =
      mojom::CertVerifierDebugInfo::New();

#if BUILDFLAG(IS_MAC)
  auto* mac_platform_debug_info =
      net::CertVerifyProcMac::ResultDebugData::Get(&primary_result);
  if (mac_platform_debug_info) {
    debug_info->mac_platform_debug_info =
        mojom::MacPlatformVerifierDebugInfo::New();
    debug_info->mac_platform_debug_info->trust_result =
        mac_platform_debug_info->trust_result();
    debug_info->mac_platform_debug_info->result_code =
        mac_platform_debug_info->result_code();
    for (const auto& cert_info : mac_platform_debug_info->status_chain()) {
      mojom::MacCertEvidenceInfoPtr info = mojom::MacCertEvidenceInfo::New();
      info->status_bits = cert_info.status_bits;
      info->status_codes = cert_info.status_codes;
      debug_info->mac_platform_debug_info->status_chain.push_back(
          std::move(info));
    }
  }

  auto* mac_trust_debug_info =
      net::TrustStoreMac::ResultDebugData::Get(&trial_result);
  if (mac_trust_debug_info) {
    debug_info->mac_combined_trust_debug_info =
        mac_trust_debug_info->combined_trust_debug_info();
    debug_info->mac_trust_impl =
        TrustImplTypeToMojom(mac_trust_debug_info->trust_impl());
  }
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
  auto* win_platform_debug_info =
      net::CertVerifyProcWin::ResultDebugData::Get(&primary_result);
  if (win_platform_debug_info) {
    debug_info->win_platform_debug_info =
        mojom::WinPlatformVerifierDebugInfo::New();
    debug_info->win_platform_debug_info->authroot_this_update =
        win_platform_debug_info->authroot_this_update();
    debug_info->win_platform_debug_info->authroot_sequence_number =
        win_platform_debug_info->authroot_sequence_number();
  }
#endif  // BUILDFLAG(IS_WIN)

  auto* cert_verify_proc_builtin_debug_data =
      net::CertVerifyProcBuiltinResultDebugData::Get(&trial_result);
  if (cert_verify_proc_builtin_debug_data) {
    debug_info->trial_verification_time =
        cert_verify_proc_builtin_debug_data->verification_time();
    uint8_t encoded_generalized_time[net::der::kGeneralizedTimeLength];
    if (net::der::EncodeGeneralizedTime(
            cert_verify_proc_builtin_debug_data->der_verification_time(),
            encoded_generalized_time)) {
      debug_info->trial_der_verification_time = std::string(
          encoded_generalized_time,
          encoded_generalized_time + net::der::kGeneralizedTimeLength);
    }
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
    if (cert_verify_proc_builtin_debug_data->chrome_root_store_version()) {
      debug_info->chrome_root_store_debug_info =
          mojom::ChromeRootStoreDebugInfo::New();
      debug_info->chrome_root_store_debug_info->chrome_root_store_version =
          cert_verify_proc_builtin_debug_data->chrome_root_store_version()
              .value();
    }
#endif
  }

  report_client_->SendTrialReport(
      hostname, unverified_cert, enable_rev_checking,
      require_rev_checking_local_anchors, enable_sha1_local_anchors,
      disable_symantec_enforcement,
      std::vector<uint8_t>(stapled_ocsp.begin(), stapled_ocsp.end()),
      std::vector<uint8_t>(sct_list.begin(), sct_list.end()), primary_result,
      trial_result, std::move(debug_info));
}

}  // namespace cert_verifier
