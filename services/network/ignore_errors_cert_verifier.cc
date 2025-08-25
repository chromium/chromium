// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ignore_errors_cert_verifier.h"

#include <algorithm>
#include <iterator>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_split.h"
#include "crypto/hash.h"
#include "net/base/hash_value.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/spki_hash_set.h"

using ::net::CertVerifier;
using ::net::HashValue;
using ::net::SHA256HashValue;
using ::net::X509Certificate;

namespace network {

// static
std::unique_ptr<CertVerifier> IgnoreErrorsCertVerifier::MaybeWrapCertVerifier(
    const base::CommandLine& command_line,
    const char* user_data_dir_switch,
    std::unique_ptr<CertVerifier> verifier) {
  if ((user_data_dir_switch && !command_line.HasSwitch(user_data_dir_switch)) ||
      !command_line.HasSwitch(switches::kIgnoreCertificateErrorsSPKIList)) {
    return verifier;
  }
  auto spki_list =
      base::SplitString(command_line.GetSwitchValueASCII(
                            switches::kIgnoreCertificateErrorsSPKIList),
                        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  return std::make_unique<IgnoreErrorsCertVerifier>(
      std::move(verifier), CreateSPKIHashSet(spki_list));
}

IgnoreErrorsCertVerifier::IgnoreErrorsCertVerifier(
    std::unique_ptr<CertVerifier> verifier,
    SPKIHashSet allowlist)
    : verifier_(std::move(verifier)), allowlist_(std::move(allowlist)) {}

IgnoreErrorsCertVerifier::~IgnoreErrorsCertVerifier() {}

int IgnoreErrorsCertVerifier::Verify(const RequestParams& params,
                                     net::CertVerifyResult* verify_result,
                                     net::CompletionOnceCallback callback,
                                     std::unique_ptr<Request>* out_req,
                                     const net::NetLogWithSource& net_log) {
  std::vector<SHA256HashValue> public_key_hashes;
  for (const auto& buffer : params.certificate()->cert_buffers()) {
    std::string_view cert_spki;
    if (net::asn1::ExtractSPKIFromDERCert(
            net::x509_util::CryptoBufferAsStringPiece(buffer.get()),
            &cert_spki)) {
      public_key_hashes.push_back(crypto::hash::Sha256(cert_spki));
    }
  }

  // Intersect SPKI hashes from the chain with the allowlist.
  bool ignore_errors = false;
  for (const auto& spki : public_key_hashes) {
    if (allowlist_.contains(spki)) {
      ignore_errors = true;
      break;
    }
  }

  if (ignore_errors) {
    verify_result->Reset();
    verify_result->verified_cert = params.certificate();
    verify_result->public_key_hashes = std::move(public_key_hashes);
    if (!params.ocsp_response().empty()) {
      verify_result->ocsp_result.response_status =
          bssl::OCSPVerifyResult::PROVIDED;
      verify_result->ocsp_result.revocation_status =
          bssl::OCSPRevocationStatus::GOOD;
    }
    return net::OK;
  }

  return verifier_->Verify(params, verify_result, std::move(callback), out_req,
                           net_log);
}

void IgnoreErrorsCertVerifier::Verify2QwacBinding(
    const std::string& binding,
    const std::string& hostname,
    const scoped_refptr<X509Certificate>& tls_cert,
    base::OnceCallback<void(const scoped_refptr<X509Certificate>&)> callback,
    const net::NetLogWithSource& net_log) {
  verifier_->Verify2QwacBinding(binding, hostname, tls_cert,
                                std::move(callback), net_log);
}

void IgnoreErrorsCertVerifier::SetConfig(const Config& config) {
  verifier_->SetConfig(config);
}

void IgnoreErrorsCertVerifier::AddObserver(Observer* observer) {
  verifier_->AddObserver(observer);
}

void IgnoreErrorsCertVerifier::RemoveObserver(Observer* observer) {
  verifier_->RemoveObserver(observer);
}

void IgnoreErrorsCertVerifier::SetAllowlistForTesting(
    const SPKIHashSet& allowlist) {
  allowlist_ = allowlist;
}

}  // namespace network
