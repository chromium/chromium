// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_FROM_STRING_URL_LOADER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_FROM_STRING_URL_LOADER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace mojo {
class SimpleWatcher;
}

namespace net {
class StringIOBuffer;
}

namespace content {

class PrefetchedMainframeResponseContainer;
struct PrefetchResponseSizes;

class PrefetchFromStringURLLoader : public network::mojom::URLLoader {
 public:
  PrefetchFromStringURLLoader(
      std::unique_ptr<PrefetchedMainframeResponseContainer> prefetched_response,
      const absl::optional<PrefetchResponseSizes>& response_sizes,
      const network::ResourceRequest& tenative_resource_request);
  ~PrefetchFromStringURLLoader() override;

  PrefetchFromStringURLLoader(const PrefetchFromStringURLLoader&) = delete;
  PrefetchFromStringURLLoader& operator=(const PrefetchFromStringURLLoader&) =
      delete;

  using RequestHandler = base::OnceCallback<void(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client)>;

  // Called when the response should be served to the user. Returns a handler.
  RequestHandler ServingResponseHandler();

 private:
  // network::mojom::URLLoader
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // Binds |this| to the mojo handlers and starts the network request using
  // |request|. After this method is called, |this| manages its own lifetime.
  void BindAndStart(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client);

  // Called when the mojo handle's state changes, either by being ready for more
  // data or an error.
  void OnHandleReady(MojoResult result, const mojo::HandleSignalsState& state);

  // Finishes the request with the given net error.
  void Finish(int error);

  // Sends data on the mojo pipe.
  void TransferRawData();

  // Unbinds and deletes |this|.
  void OnMojoDisconnect();

  // Deletes |this| if it is not bound to the mojo pipes.
  void MaybeDeleteSelf();

  // The response that will be sent to mojo.
  network::mojom::URLResponseHeadPtr head_;
  scoped_refptr<net::StringIOBuffer> body_buffer_;

  // Keeps track of the position of the data transfer.
  int write_position_ = 0;

  // The length of |body_buffer_|.
  const int bytes_of_raw_data_to_transfer_ = 0;

  // Mojo plumbing.
  mojo::Receiver<network::mojom::URLLoader> receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  std::unique_ptr<mojo::SimpleWatcher> handle_watcher_;

  const absl::optional<PrefetchResponseSizes>& response_sizes_;

  base::WeakPtrFactory<PrefetchFromStringURLLoader> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_FROM_STRING_URL_LOADER_H_
