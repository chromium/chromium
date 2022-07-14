// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CERT_VERIFIER_TEST_CERT_VERIFIER_SERVICE_FACTORY_H_
#define SERVICES_CERT_VERIFIER_TEST_CERT_VERIFIER_SERVICE_FACTORY_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/net_buildflags.h"
#include "services/cert_verifier/cert_verifier_service_factory.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace cert_verifier {

// Captures the params passed to GetNewCertVerifier, and sends them to a wrapped
// CertVerifierServiceFactoryImpl when instructed to.
class TestCertVerifierServiceFactoryImpl
    : public mojom::CertVerifierServiceFactory {
 public:
  TestCertVerifierServiceFactoryImpl();
  ~TestCertVerifierServiceFactoryImpl() override;

  struct GetNewCertVerifierParams {
    GetNewCertVerifierParams();
    GetNewCertVerifierParams(GetNewCertVerifierParams&&);
    GetNewCertVerifierParams& operator=(GetNewCertVerifierParams&& other);
    GetNewCertVerifierParams(const GetNewCertVerifierParams&) = delete;
    GetNewCertVerifierParams& operator=(const GetNewCertVerifierParams&) =
        delete;
    ~GetNewCertVerifierParams();

    mojo::PendingReceiver<mojom::CertVerifierService> receiver;
    mojom::CertVerifierCreationParamsPtr creation_params;
  };

  // mojom::CertVerifierServiceFactory implementation:
  void GetNewCertVerifier(
      mojo::PendingReceiver<mojom::CertVerifierService> receiver,
      mojom::CertVerifierCreationParamsPtr creation_params) override;

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  void UpdateChromeRootStore(mojom::ChromeRootStorePtr new_root_store) override;
#endif

  // Pops the first request off the back of the list and forwards it to the
  // delegate CertVerifierServiceFactory.
  void ReleaseNextCertVerifierParams();
  void ReleaseAllCertVerifierParams();

  size_t num_captured_params() const { return captured_params_.size(); }
  // Ordered from most recent to least recent.
  const GetNewCertVerifierParams* GetParamsAtIndex(int i) {
    return &captured_params_[i];
  }

 private:
  void InitDelegate();

  mojo::Remote<mojom::CertVerifierServiceFactory> delegate_remote_;
  std::unique_ptr<CertVerifierServiceFactoryImpl> delegate_;

  base::circular_deque<GetNewCertVerifierParams> captured_params_;
};

}  // namespace cert_verifier

#endif  // SERVICES_CERT_VERIFIER_TEST_CERT_VERIFIER_SERVICE_FACTORY_H_
