// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_URL_LOADER_CLIENT_CHECKER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_URL_LOADER_CLIENT_CHECKER_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/early_hints.mojom-forward.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {

// When used as replacement of a
// `mojo::Remote<network::mojom::URLLoaderClient>`, this checks the calling
// order of `network::mojom::URLLoaderClient` methods.
//
// Currently this only implements the assertions and interfaces needed for the
// investigation of https://crbug.com/1346074, and this can be only used to call
// `network::mojom::URLLoaderClient` methods.
class URLLoaderClientCheckedRemote final {
 public:
  // `URLLoaderClient` methods shouldn't have been called previously via
  // `client`.
  explicit URLLoaderClientCheckedRemote(
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  class Proxy final {
   public:
    explicit Proxy(mojo::PendingRemote<network::mojom::URLLoaderClient> client);
    ~Proxy();

    void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints);
    void OnReceiveResponse(
        network::mojom::URLResponseHeadPtr head,
        mojo::ScopedDataPipeConsumerHandle body,
        std::optional<mojo_base::BigBuffer> cached_metadata) {
      on_receive_response_called_ = true;
      client_->OnReceiveResponse(std::move(head), std::move(body),
                                 std::move(cached_metadata));
    }
    void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                           network::mojom::URLResponseHeadPtr head) {
      client_->OnReceiveRedirect(redirect_info, std::move(head));
    }
    void OnUploadProgress(
        int64_t current_position,
        int64_t total_size,
        network::mojom::URLLoaderClient::OnUploadProgressCallback callback) {
      client_->OnUploadProgress(current_position, total_size,
                                std::move(callback));
    }
    void OnTransferSizeUpdated(int32_t transfer_size_diff);
    void OnComplete(const network::URLLoaderCompletionStatus& status);

    explicit operator bool() const { return static_cast<bool>(client_); }

   private:
    mojo::Remote<network::mojom::URLLoaderClient> client_;
    bool on_receive_response_called_ = false;
  };

  Proxy* operator->() { return &proxy_; }
  explicit operator bool() const { return static_cast<bool>(proxy_); }

 private:
  Proxy proxy_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_URL_LOADER_CLIENT_CHECKER_H_
