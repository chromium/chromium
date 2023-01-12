// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/cert_net_url_loader/cert_net_fetcher_test.h"

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/cert_verifier/cert_net_url_loader/cert_net_fetcher_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace cert_verifier {
CertNetFetcherTestUtil::CertNetFetcherTestUtil() {
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      pending_remote_url_loader_factory;
  pending_receiver_ =
      pending_remote_url_loader_factory.InitWithNewPipeAndPassReceiver();

  fetcher_ = base::MakeRefCounted<CertNetFetcherURLLoader>();
  fetcher_->SetURLLoaderFactoryAndReconnector(
      std::move(pending_remote_url_loader_factory),
      base::BindRepeating(
          &CertNetFetcherTestUtil::RebindURLLoaderFactoryTrampoline,
          base::Unretained(this)));
}

CertNetFetcherTestUtil::~CertNetFetcherTestUtil() = default;

void CertNetFetcherTestUtil::RebindURLLoaderFactoryTrampoline(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver) {
  RebindURLLoaderFactory(std::move(pending_receiver));
}

CertNetFetcherTestUtilFakeLoader::~CertNetFetcherTestUtilFakeLoader() = default;

CertNetFetcherTestUtilFakeLoader::CertNetFetcherTestUtilFakeLoader()
    : test_url_loader_factory_(
          std::make_unique<network::TestURLLoaderFactory>()),
      receiver_(
          std::make_unique<mojo::Receiver<network::mojom::URLLoaderFactory>>(
              test_url_loader_factory_.get(),
              TakePendingReceiver())) {}

void CertNetFetcherTestUtilFakeLoader::ResetURLLoaderFactory() {
  receiver_.reset();
}
void CertNetFetcherTestUtilFakeLoader::RebindURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver) {
  receiver_ =
      std::make_unique<mojo::Receiver<network::mojom::URLLoaderFactory>>(
          test_url_loader_factory_.get(), std::move(pending_receiver));
}

CertNetFetcherTestUtilRealLoader::~CertNetFetcherTestUtilRealLoader() = default;

CertNetFetcherTestUtilRealLoader::CertNetFetcherTestUtilRealLoader()
    : test_shared_url_loader_factory_(
          base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
              nullptr /* network_service */,
              true /* is_trusted */)),
      receiver_(
          std::make_unique<mojo::Receiver<network::mojom::URLLoaderFactory>>(
              test_shared_url_loader_factory_.get(),
              TakePendingReceiver())) {}

void CertNetFetcherTestUtilRealLoader::ResetURLLoaderFactory() {
  receiver_.reset();
}

void CertNetFetcherTestUtilRealLoader::RebindURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver) {
  receiver_ =
      std::make_unique<mojo::Receiver<network::mojom::URLLoaderFactory>>(
          test_shared_url_loader_factory_.get(), std::move(pending_receiver));
}
}  // namespace cert_verifier
