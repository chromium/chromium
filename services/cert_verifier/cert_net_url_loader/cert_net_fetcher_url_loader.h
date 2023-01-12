// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CERT_VERIFIER_CERT_NET_URL_LOADER_CERT_NET_FETCHER_URL_LOADER_H_
#define SERVICES_CERT_VERIFIER_CERT_NET_URL_LOADER_CERT_NET_FETCHER_URL_LOADER_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cert/cert_net_fetcher.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace base {
class SequencedTaskRunner;
}

namespace cert_verifier {

// A CertNetFetcher that issues requests through the provided
// URLLoaderFactory. The CertNetFetcher MUST be Shutdown on the same thread it
// was created on, prior to destruction, and the actual fetching will be done on
// that thread. The CertNetFetcher's Fetch methods are to be used on a
// *different* thread, since it gives a blocking interface to URL fetching.
class COMPONENT_EXPORT(CERT_VERIFIER_CPP) CertNetFetcherURLLoader
    : public net::CertNetFetcher {
 public:
  class AsyncCertNetFetcherURLLoader;
  class RequestCore;
  struct RequestParams;

  // The CertNetFetcherURLLoader will immediately fail all requests until
  // SetURLLoaderFactoryAndReconnector() is called.
  CertNetFetcherURLLoader();

  // Enables this CertNetFetcher to load URLs using |factory|.
  // If the other side of the |factory| remote disconnects, the
  // CertNetFetcherURLLoader will attempt to reconnect using
  // |bind_new_url_loader_factory_cb|. This must be called before ever
  // performing a fetch. It is recommended, but not required, to provide a
  // functional |bind_new_url_loader_factory_cb|.
  void SetURLLoaderFactoryAndReconnector(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> factory,
      base::RepeatingCallback<
          void(mojo::PendingReceiver<network::mojom::URLLoaderFactory>)>
          bind_new_url_loader_factory_cb);

  // Returns the default timeout value. Intended for test use only.
  static base::TimeDelta GetDefaultTimeoutForTesting();

  // Disconnects the URLLoaderFactory used for fetches.
  void DisconnectURLLoaderFactoryForTesting();

  // CertNetFetcher impl:
  void Shutdown() override;
  std::unique_ptr<Request> FetchCaIssuers(const GURL& url,
                                          int timeout_milliseconds,
                                          int max_response_bytes) override;
  std::unique_ptr<Request> FetchCrl(const GURL& url,
                                    int timeout_milliseconds,
                                    int max_response_bytes) override;
  [[nodiscard]] std::unique_ptr<Request> FetchOcsp(
      const GURL& url,
      int timeout_milliseconds,
      int max_response_bytes) override;

 private:
  ~CertNetFetcherURLLoader() override;

  void DoFetchOnTaskRunner(std::unique_ptr<RequestParams> request_params,
                           scoped_refptr<RequestCore> request);

  std::unique_ptr<Request> DoFetch(
      std::unique_ptr<RequestParams> request_params);

  // The task runner of the creation thread.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<AsyncCertNetFetcherURLLoader> impl_;
};

}  // namespace cert_verifier

#endif  // SERVICES_CERT_VERIFIER_CERT_NET_URL_LOADER_CERT_NET_FETCHER_URL_LOADER_H_
