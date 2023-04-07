// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IGNORE_ERRORS_CERT_VERIFIER_H_
#define SERVICES_NETWORK_IGNORE_ERRORS_CERT_VERIFIER_H_

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/component_export.h"
#include "net/base/completion_once_callback.h"
#include "net/cert/cert_verifier.h"
#include "services/network/public/cpp/spki_hash_set.h"

namespace network {

// IgnoreErrorsCertVerifier wraps another CertVerifier in order to ignore
// verification errors from certificate chains that match a allowlist of SPKI
// fingerprints.
class COMPONENT_EXPORT(NETWORK_SERVICE) IgnoreErrorsCertVerifier
    : public net::CertVerifier {
 public:
  // If the |user_data_dir_switch| is passed in as a valid pointer but
  // --user-data-dir flag is missing, or --ignore-certificate-errors-spki-list
  // flag is missing then MaybeWrapCertVerifier returns the supplied verifier.
  // Otherwise it returns an IgnoreErrorsCertVerifier wrapping the supplied
  // verifier using the allowlist from the
  // --ignore-certificate-errors-spki-list flag.
  //
  // As the --user-data-dir flag is embedder defined, the flag to check for
  // needs to be passed in from |user_data_dir_switch|.
  static std::unique_ptr<net::CertVerifier> MaybeWrapCertVerifier(
      const base::CommandLine& command_line,
      const char* user_data_dir_switch,
      std::unique_ptr<net::CertVerifier> verifier);

  IgnoreErrorsCertVerifier(std::unique_ptr<net::CertVerifier> verifier,
                           SPKIHashSet allowlist);

  IgnoreErrorsCertVerifier(const IgnoreErrorsCertVerifier&) = delete;
  IgnoreErrorsCertVerifier& operator=(const IgnoreErrorsCertVerifier&) = delete;

  ~IgnoreErrorsCertVerifier() override;

  // Verify skips certificate verification and returns OK if any of the
  // certificates from the chain in |params| match one of the SPKI fingerprints
  // from the allowlist. Otherwise, it invokes Verify on the wrapped verifier
  // and returns the result.
  int Verify(const RequestParams& params,
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const net::NetLogWithSource& net_log) override;
  void SetConfig(const Config& config) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  friend class IgnoreErrorsCertVerifierTest;

  void SetAllowlistForTesting(const SPKIHashSet& allowlist);

  std::unique_ptr<net::CertVerifier> verifier_;
  SPKIHashSet allowlist_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_IGNORE_ERRORS_CERT_VERIFIER_H_
