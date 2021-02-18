// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/cert_verifier_service_factory.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_verifier.h"
#include "services/cert_verifier/cert_net_url_loader/cert_net_fetcher_url_loader.h"
#include "services/cert_verifier/cert_verifier_creation.h"
#include "services/cert_verifier/cert_verifier_service.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace cert_verifier {
namespace {

void GetNewCertVerifierImpl(
    mojo::PendingReceiver<mojom::CertVerifierService> receiver,
    mojom::CertVerifierCreationParamsPtr creation_params,
    scoped_refptr<CertNetFetcherURLLoader>* cert_net_fetcher_ptr) {
  scoped_refptr<CertNetFetcherURLLoader> cert_net_fetcher;

  // Sometimes the cert_net_fetcher isn't used by CreateCertVerifier.
  // But losing the last ref without calling Shutdown() will cause a CHECK
  // failure, so keep a ref.
  if (IsUsingCertNetFetcher())
    cert_net_fetcher = base::MakeRefCounted<CertNetFetcherURLLoader>();

  // Create a new CertVerifier to back our service. This will be instantiated
  // without the coalescing or caching layers, because those layers will work
  // better in the network process, and will give us NetLog visibility if
  // running in the network process.
  std::unique_ptr<net::CertVerifier> cert_verifier =
      CreateCertVerifier(creation_params.get(), cert_net_fetcher);

  // As an optimization, if the CertNetFetcher isn't used by the CertVerifier,
  // shut it down immediately.
  if (cert_net_fetcher && cert_net_fetcher->HasOneRef()) {
    cert_net_fetcher->Shutdown();
    cert_net_fetcher.reset();
  }

  if (cert_net_fetcher_ptr)
    *cert_net_fetcher_ptr = cert_net_fetcher;

  // The service will delete itself upon disconnection.
  new internal::CertVerifierServiceImpl(std::move(cert_verifier),
                                        std::move(receiver),
                                        std::move(cert_net_fetcher));
}

}  // namespace

CertVerifierServiceFactoryImpl::CertVerifierServiceFactoryImpl(
    mojo::PendingReceiver<mojom::CertVerifierServiceFactory> receiver)
    : receiver_(this, std::move(receiver)) {}

CertVerifierServiceFactoryImpl::~CertVerifierServiceFactoryImpl() = default;

void CertVerifierServiceFactoryImpl::GetNewCertVerifier(
    mojo::PendingReceiver<mojom::CertVerifierService> receiver,
    mojom::CertVerifierCreationParamsPtr creation_params) {
  GetNewCertVerifierImpl(std::move(receiver), std::move(creation_params),
                         nullptr);
}

void CertVerifierServiceFactoryImpl::GetNewCertVerifierForTesting(
    mojo::PendingReceiver<mojom::CertVerifierService> receiver,
    mojom::CertVerifierCreationParamsPtr creation_params,
    scoped_refptr<CertNetFetcherURLLoader>* cert_net_fetcher_ptr) {
  GetNewCertVerifierImpl(std::move(receiver), std::move(creation_params),
                         cert_net_fetcher_ptr);
}

}  // namespace cert_verifier
