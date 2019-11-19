// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verifier.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/crl_set.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

#if defined(OS_NACL)
#include "base/logging.h"
#else
#include "net/cert/caching_cert_verifier.h"
#include "net/cert/coalescing_cert_verifier.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#endif

namespace net {

CertVerifier::Config::Config() = default;
CertVerifier::Config::Config(const Config&) = default;
CertVerifier::Config::Config(Config&&) = default;
CertVerifier::Config::~Config() = default;
CertVerifier::Config& CertVerifier::Config::operator=(const Config&) = default;
CertVerifier::Config& CertVerifier::Config::operator=(Config&&) = default;

CertVerifier::RequestParams::RequestParams(
    scoped_refptr<X509Certificate> certificate,
    const std::string& hostname,
    int flags,
    const std::string& ocsp_response,
    const std::string& sct_list)
    : certificate_(std::move(certificate)),
      hostname_(hostname),
      flags_(flags),
      ocsp_response_(ocsp_response),
      sct_list_(sct_list) {
  // For efficiency sake, rather than compare all of the fields for each
  // comparison, compute a hash of their values. This is done directly in
  // this class, rather than as an overloaded hash operator, for efficiency's
  // sake.
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, CRYPTO_BUFFER_data(certificate_->cert_buffer()),
                CRYPTO_BUFFER_len(certificate_->cert_buffer()));
  for (const auto& cert_handle : certificate_->intermediate_buffers()) {
    SHA256_Update(&ctx, CRYPTO_BUFFER_data(cert_handle.get()),
                  CRYPTO_BUFFER_len(cert_handle.get()));
  }
  SHA256_Update(&ctx, hostname_.data(), hostname.size());
  SHA256_Update(&ctx, &flags, sizeof(flags));
  SHA256_Update(&ctx, ocsp_response.data(), ocsp_response.size());
  SHA256_Update(&ctx, sct_list.data(), sct_list.size());
  SHA256_Final(reinterpret_cast<uint8_t*>(
                   base::WriteInto(&key_, SHA256_DIGEST_LENGTH + 1)),
               &ctx);
}

CertVerifier::RequestParams::RequestParams(const RequestParams& other) =
    default;
CertVerifier::RequestParams::~RequestParams() = default;

bool CertVerifier::RequestParams::operator==(
    const CertVerifier::RequestParams& other) const {
  return key_ == other.key_;
}

bool CertVerifier::RequestParams::operator<(
    const CertVerifier::RequestParams& other) const {
  return key_ < other.key_;
}

// static
std::unique_ptr<CertVerifier> CertVerifier::CreateDefault(
    scoped_refptr<CertNetFetcher> cert_net_fetcher) {
#if defined(OS_NACL)
  NOTIMPLEMENTED();
  return std::unique_ptr<CertVerifier>();
#else
  scoped_refptr<CertVerifyProc> verify_proc;
#if defined(OS_FUCHSIA)
  verify_proc =
      CertVerifyProc::CreateBuiltinVerifyProc(std::move(cert_net_fetcher));
#elif BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
  if (base::FeatureList::IsEnabled(features::kCertVerifierBuiltinFeature)) {
    verify_proc =
        CertVerifyProc::CreateBuiltinVerifyProc(std::move(cert_net_fetcher));
  } else {
    verify_proc =
        CertVerifyProc::CreateSystemVerifyProc(std::move(cert_net_fetcher));
  }
#else
  verify_proc =
      CertVerifyProc::CreateSystemVerifyProc(std::move(cert_net_fetcher));
#endif

  return std::make_unique<CachingCertVerifier>(
      std::make_unique<CoalescingCertVerifier>(
          std::make_unique<MultiThreadedCertVerifier>(std::move(verify_proc))));
#endif
}

bool operator==(const CertVerifier::Config& lhs,
                const CertVerifier::Config& rhs) {
  return std::tie(
             lhs.enable_rev_checking, lhs.require_rev_checking_local_anchors,
             lhs.enable_sha1_local_anchors, lhs.disable_symantec_enforcement,
             lhs.crl_set, lhs.additional_trust_anchors) ==
         std::tie(
             rhs.enable_rev_checking, rhs.require_rev_checking_local_anchors,
             rhs.enable_sha1_local_anchors, rhs.disable_symantec_enforcement,
             rhs.crl_set, rhs.additional_trust_anchors);
}

bool operator!=(const CertVerifier::Config& lhs,
                const CertVerifier::Config& rhs) {
  return !(lhs == rhs);
}

}  // namespace net
