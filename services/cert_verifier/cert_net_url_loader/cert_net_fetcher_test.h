// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CERT_VERIFIER_CERT_NET_URL_LOADER_CERT_NET_FETCHER_TEST_H_
#define SERVICES_CERT_VERIFIER_CERT_NET_URL_LOADER_CERT_NET_FETCHER_TEST_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/cert/cert_net_fetcher.h"
#include "services/cert_verifier/cert_net_url_loader/cert_net_fetcher_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

namespace cert_verifier {

// Holds a CertNetFetcher, and either a network::TestURLLoaderFactory (for mock
// response to network requests) or a network::TestSharedURLLoaderFactory (for
// real network requests) These test-only classes should be created only on the
// network thread.

class CertNetFetcherTestUtil {
 public:
  CertNetFetcherTestUtil();
  virtual ~CertNetFetcherTestUtil();

  // Disconnect the current URLLoaderFactory Mojo pipe.
  virtual void ResetURLLoaderFactory() = 0;

  scoped_refptr<CertNetFetcherURLLoader>& fetcher() { return fetcher_; }

 protected:
  // Binds |pending_receiver| to an existing URLLoaderFactory. Used as a
  // callback passed to the CertNetFetcherURLLoader constructor to rebind a
  // URLLoaderFactory in case of disconnection.
  // This expects ResetURLLoaderFactory to have been called previously,
  // otherwise a DHCECK will fire when creating the mojo::Receiver.
  virtual void RebindURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          pending_receiver) = 0;

  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
  TakePendingReceiver() {
    return std::move(pending_receiver_);
  }

 private:
  // Just forwards |pending_receiver| to RebindURLLoaderFactory, used so that we
  // don't have to reference a virtual function in the constructor when we
  // instantiate the CertNetFetcherURLLoader.
  void RebindURLLoaderFactoryTrampoline(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver);

  scoped_refptr<CertNetFetcherURLLoader> fetcher_;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver_;
};

class CertNetFetcherTestUtilFakeLoader : public CertNetFetcherTestUtil {
 public:
  CertNetFetcherTestUtilFakeLoader();
  ~CertNetFetcherTestUtilFakeLoader() override;

  void ResetURLLoaderFactory() override;

  network::TestURLLoaderFactory* url_loader_factory() {
    return test_url_loader_factory_.get();
  }

 private:
  void RebindURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver)
      override;

  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  std::unique_ptr<mojo::Receiver<network::mojom::URLLoaderFactory>> receiver_;
};

class CertNetFetcherTestUtilRealLoader : public CertNetFetcherTestUtil {
 public:
  CertNetFetcherTestUtilRealLoader();
  ~CertNetFetcherTestUtilRealLoader() override;

  void ResetURLLoaderFactory() override;

  scoped_refptr<network::TestSharedURLLoaderFactory>
  shared_url_loader_factory() {
    return test_shared_url_loader_factory_;
  }

 private:
  void RebindURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver)
      override;

  scoped_refptr<network::TestSharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  std::unique_ptr<mojo::Receiver<network::mojom::URLLoaderFactory>> receiver_;
};

}  // namespace cert_verifier

#endif  // SERVICES_CERT_VERIFIER_CERT_NET_URL_LOADER_CERT_NET_FETCHER_TEST_H_
