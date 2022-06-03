// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CERT_VERIFIER_CERT_VERIFIER_SERVICE_FACTORY_H_
#define SERVICES_CERT_VERIFIER_CERT_VERIFIER_SERVICE_FACTORY_H_

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/cert_verifier/cert_net_url_loader/cert_net_fetcher_url_loader.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"

namespace cert_verifier {

// Implements mojom::CertVerifierServiceFactory, and calls
// network::CreateCertVerifier to instantiate the concrete net::CertVerifier
// used to service requests.
class CertVerifierServiceFactoryImpl
    : public mojom::CertVerifierServiceFactory {
 public:
  explicit CertVerifierServiceFactoryImpl(
      mojo::PendingReceiver<mojom::CertVerifierServiceFactory> receiver);
  ~CertVerifierServiceFactoryImpl() override;

  // mojom::CertVerifierServiceFactory implementation:
  void GetNewCertVerifier(
      mojo::PendingReceiver<mojom::CertVerifierService> receiver,
      mojom::CertVerifierCreationParamsPtr creation_params) override;

  // Performs the same function as above, but stores a ref to the new
  // CertNetFetcherURLLoader in |*cert_net_fetcher_ptr|, if the
  // CertNetFetcherURLLoader is in use.
  void GetNewCertVerifierForTesting(
      mojo::PendingReceiver<mojom::CertVerifierService> receiver,
      mojom::CertVerifierCreationParamsPtr creation_params,
      scoped_refptr<CertNetFetcherURLLoader>* cert_net_fetcher_ptr);

 private:
  mojo::Receiver<mojom::CertVerifierServiceFactory> receiver_;
};

}  // namespace cert_verifier

#endif  // SERVICES_CERT_VERIFIER_CERT_VERIFIER_SERVICE_FACTORY_H_
