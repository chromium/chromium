// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_FAKE_TEST_CERT_VERIFIER_PARAMS_FACTORY_H_
#define SERVICES_NETWORK_TEST_FAKE_TEST_CERT_VERIFIER_PARAMS_FACTORY_H_

#include "net/cert/cert_verifier.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace network {

// FakeTestCertVerifierParamsFactory::GetCertVerifierParams returns a
// mojom::CertVerifierParamsPtr, which either contains the parameters for a real
// in-network-service CertVerifier, or contains a pipe to a fake
// CertVerifierService that successfully verifies every certificate, even
// invalid ones. This is useful for tests that need to set up a NetworkContext,
// which requires CertVerifierParams, but the test doesn't actually need to test
// cert verifier behavior.
class FakeTestCertVerifierParamsFactory
    : public cert_verifier::mojom::CertVerifierService {
 public:
  FakeTestCertVerifierParamsFactory();
  ~FakeTestCertVerifierParamsFactory() override;

  static mojom::CertVerifierServiceRemoteParamsPtr GetCertVerifierParams();

 private:
  // cert_verifier::mojom::CertVerifierService implementation:
  void Verify(const net::CertVerifier::RequestParams& params,
              const net::NetLogSource& net_log_source,
              mojo::PendingRemote<cert_verifier::mojom::CertVerifierRequest>
                  cert_verifier_request) override;
  void SetConfig(const net::CertVerifier::Config& config) override {}
  void EnableNetworkAccess(
      mojo::PendingRemote<mojom::URLLoaderFactory>,
      mojo::PendingRemote<cert_verifier::mojom::URLLoaderFactoryConnector>
          reconnector) override {}
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_FAKE_TEST_CERT_VERIFIER_PARAMS_FACTORY_H_
