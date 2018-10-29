// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_URL_LOADER_CLIENT_IMPL_H_
#define CONTENT_RENDERER_LOADER_URL_LOADER_CLIENT_IMPL_H_

#include <stdint.h>
#include <vector>
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
struct RedirectInfo;
}  // namespace net

namespace network {
struct ResourceResponseHead;
struct URLLoaderCompletionStatus;
}  // namespace network

namespace content {
class ResourceDispatcher;
class URLResponseBodyConsumer;

class CONTENT_EXPORT URLLoaderClientImpl final
    : public network::mojom::URLLoaderClient {
 public:
  URLLoaderClientImpl(int request_id,
                      ResourceDispatcher* resource_dispatcher,
                      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                      bool bypass_redirect_checks,
                      const GURL& request_url);
  ~URLLoaderClientImpl() override;

  // Sets |is_deferred_|. From now, the received messages are not dispatched
  // to clients until UnsetDefersLoading is called.
  void SetDefersLoading();

  // Unsets |is_deferred_|.
  void UnsetDefersLoading();

  // Dispatches the messages received after SetDefersLoading is called.
  void FlushDeferredMessages();

  // If set to true, this causes the raw datapipe containing the response body
  // to be passed on to the ResourceDispatcher. Otherwise a
  // URLResponseBodyConsumer is created that passes individual chunks of data
  // from the body to the dispatcher.
  void SetPassResponsePipeToDispatcher(bool pass_pipe) {
    pass_response_pipe_to_dispatcher_ = pass_pipe;
  }

  // Binds this instance to the given URLLoaderClient endpoints so that it can
  // start getting the mojo calls from the given loader. This is used only for
  // the main resource loading. Otherwise (in regular subresource loading cases)
  // |this| is not bound to a client request, but used via ThrottlingURLLoader
  // to get client upcalls from the loader.
  void Bind(
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints);

  // network::mojom::URLLoaderClient implementation
  void OnReceiveResponse(
      const network::ResourceResponseHead& response_head) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

 private:
  class DeferredMessage;
  class DeferredOnReceiveResponse;
  class DeferredOnReceiveRedirect;
  class DeferredOnUploadProgress;
  class DeferredOnReceiveCachedMetadata;
  class DeferredOnComplete;

  bool NeedsStoringMessage() const;
  void StoreAndDispatch(std::unique_ptr<DeferredMessage> message);
  void OnConnectionClosed();

  scoped_refptr<URLResponseBodyConsumer> body_consumer_;
  std::vector<std::unique_ptr<DeferredMessage>> deferred_messages_;
  const int request_id_;
  bool has_received_response_ = false;
  bool has_received_complete_ = false;
  bool is_deferred_ = false;
  bool pass_response_pipe_to_dispatcher_ = false;
  int32_t accumulated_transfer_size_diff_during_deferred_ = 0;
  ResourceDispatcher* const resource_dispatcher_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  bool bypass_redirect_checks_ = false;
  GURL last_loaded_url_;

  network::mojom::URLLoaderPtr url_loader_;
  mojo::Binding<network::mojom::URLLoaderClient> url_loader_client_binding_;

  base::WeakPtrFactory<URLLoaderClientImpl> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_URL_LOADER_CLIENT_IMPL_H_
