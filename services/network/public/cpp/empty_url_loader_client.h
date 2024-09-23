// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_EMPTY_URL_LOADER_CLIENT_H_
#define SERVICES_NETWORK_PUBLIC_CPP_EMPTY_URL_LOADER_CLIENT_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace network {

// Helper for draining/discarding data and callbacks that go to URLLoaderClient.
class COMPONENT_EXPORT(NETWORK_CPP) EmptyURLLoaderClient
    : public mojom::URLLoaderClient,
      public mojo::DataPipeDrainer::Client {
 public:
  EmptyURLLoaderClient();

  EmptyURLLoaderClient(const EmptyURLLoaderClient&) = delete;
  EmptyURLLoaderClient& operator=(const EmptyURLLoaderClient&) = delete;

  ~EmptyURLLoaderClient() override;

  // Calls |callback| when the request is done.
  void Drain(base::OnceCallback<void(const URLLoaderCompletionStatus&)>);

 private:
  void MaybeDone();

  // mojom::URLLoaderClient overrides:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const URLLoaderCompletionStatus& status) override;

  // mojo::DataPipeDrainer::Client overrides:
  void OnDataAvailable(base::span<const uint8_t> data) override;
  void OnDataComplete() override;

  std::unique_ptr<mojo::DataPipeDrainer> response_body_drainer_;

  std::optional<URLLoaderCompletionStatus> done_status_;
  base::OnceCallback<void(const URLLoaderCompletionStatus&)> callback_;
};

// Self-owned helper class for using EmptyURLLoaderClient.
class COMPONENT_EXPORT(NETWORK_CPP) EmptyURLLoaderClientWrapper {
 public:
  // Binds |client_receiver| to a newly constructed EmptyURLLoaderClient which
  // will drain/discard all callbacks/data. Takes ownership of |url_loader| and
  // discards it (together with EmptyURLLoaderClient) when the URL request has
  // been completed.
  static void DrainURLRequest(
      mojo::PendingReceiver<mojom::URLLoaderClient> client_receiver,
      mojo::PendingRemote<mojom::URLLoader> url_loader);

  ~EmptyURLLoaderClientWrapper();

 private:
  EmptyURLLoaderClientWrapper(
      mojo::PendingReceiver<mojom::URLLoaderClient> receiver,
      mojo::PendingRemote<mojom::URLLoader> url_loader);

  void DidDrain(const network::URLLoaderCompletionStatus& status);
  void DeleteSelf();

  EmptyURLLoaderClient client_;
  mojo::Receiver<mojom::URLLoaderClient> receiver_;
  mojo::Remote<mojom::URLLoader> url_loader_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_EMPTY_URL_LOADER_CLIENT_H_
