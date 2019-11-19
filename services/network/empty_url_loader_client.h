// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_EMPTY_URL_LOADER_CLIENT_H_
#define SERVICES_NETWORK_EMPTY_URL_LOADER_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace network {

// Helper for draining/discarding data and callbacks that go to URLLoaderClient.
class EmptyURLLoaderClient : public mojom::URLLoaderClient,
                             public mojo::DataPipeDrainer::Client {
 public:
  // Binds |client_receiver| to a newly constructed EmptyURLLoaderClient which
  // will drain/discard all callbacks/data.  Takes ownership of |url_loader| and
  // discards it (together with EmptyURLLoaderClient) when the URL request has
  // been completed.
  static void DrainURLRequest(
      mojo::PendingReceiver<mojom::URLLoaderClient> client_receiver,
      mojom::URLLoaderPtr url_loader);

 private:
  EmptyURLLoaderClient(
      mojo::PendingReceiver<mojom::URLLoaderClient> client_receiver,
      mojom::URLLoaderPtr url_loader);

  ~EmptyURLLoaderClient() override;
  void DeleteSelf();

  // mojom::URLLoaderClient overrides:
  void OnReceiveResponse(mojom::URLResponseHeadPtr head) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const URLLoaderCompletionStatus& status) override;

  // mojo::DataPipeDrainer::Client overrides:
  void OnDataAvailable(const void* data, size_t num_bytes) override;
  void OnDataComplete() override;

  mojo::Receiver<mojom::URLLoaderClient> receiver_;

  std::unique_ptr<mojo::DataPipeDrainer> response_body_drainer_;

  mojom::URLLoaderPtr url_loader_;

  DISALLOW_COPY_AND_ASSIGN(EmptyURLLoaderClient);
};

}  // namespace network

#endif  // SERVICES_NETWORK_EMPTY_URL_LOADER_CLIENT_H_
