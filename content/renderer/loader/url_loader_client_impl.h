// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_URL_LOADER_CLIENT_IMPL_H_
#define CONTENT_RENDERER_LOADER_URL_LOADER_CLIENT_IMPL_H_

#include <stdint.h>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom-forward.h"
#include "third_party/blink/public/platform/web_url_loader.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
struct RedirectInfo;
}  // namespace net

namespace network {
struct URLLoaderCompletionStatus;
}  // namespace network

namespace content {
class ResourceDispatcher;

class CONTENT_EXPORT URLLoaderClientImpl final
    : public network::mojom::URLLoaderClient {
 public:
  URLLoaderClientImpl(int request_id,
                      ResourceDispatcher* resource_dispatcher,
                      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                      bool bypass_redirect_checks,
                      const GURL& request_url);
  ~URLLoaderClientImpl() override;

  // Set the defer status. If loading is deferred, received messages are not
  // dispatched to clients until it is set not deferred.
  void SetDefersLoading(blink::WebURLLoader::DeferType value);

  // Dispatches the messages received after SetDefersLoading is called.
  void FlushDeferredMessages();

  // Binds this instance to the given URLLoaderClient endpoints so that it can
  // start getting the mojo calls from the given loader. This is used only for
  // the main resource loading. Otherwise (in regular subresource loading cases)
  // |this| is not bound to a client request, but used via ThrottlingURLLoader
  // to get client upcalls from the loader.
  void Bind(
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints);

  // network::mojom::URLLoaderClient implementation
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  void EvictFromBackForwardCache(blink::mojom::RendererEvictionReason reason);
  bool IsDeferredWithBackForwardCache() {
    return deferred_state_ ==
           blink::WebURLLoader::DeferType::kDeferredWithBackForwardCache;
  }

  const GURL& last_loaded_url() const { return last_loaded_url_; }

 private:
  class BodyBuffer;
  class DeferredMessage;
  class DeferredOnReceiveResponse;
  class DeferredOnReceiveRedirect;
  class DeferredOnUploadProgress;
  class DeferredOnReceiveCachedMetadata;
  class DeferredOnStartLoadingResponseBody;
  class DeferredOnComplete;

  bool NeedsStoringMessage() const;
  void StoreAndDispatch(std::unique_ptr<DeferredMessage> message);
  void OnConnectionClosed();

  void EvictFromBackForwardCacheDueToTimeout();
  void StopBackForwardCacheEvictionTimer();

  std::vector<std::unique_ptr<DeferredMessage>> deferred_messages_;
  std::unique_ptr<BodyBuffer> body_buffer_;
  base::OneShotTimer back_forward_cache_eviction_timer_;
  base::TimeDelta back_forward_cache_timeout_;
  const int request_id_;
  bool has_received_response_head_ = false;
  bool has_received_response_body_ = false;
  bool has_received_complete_ = false;
  blink::WebURLLoader::DeferType deferred_state_ =
      blink::WebURLLoader::DeferType::kNotDeferred;
  int32_t accumulated_transfer_size_diff_during_deferred_ = 0;
  ResourceDispatcher* const resource_dispatcher_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  bool bypass_redirect_checks_ = false;
  GURL last_loaded_url_;

  // For UMA.
  base::TimeTicks on_receive_response_time_;

  mojo::Remote<network::mojom::URLLoader> url_loader_;
  mojo::Receiver<network::mojom::URLLoaderClient> url_loader_client_receiver_{
      this};

  base::WeakPtrFactory<URLLoaderClientImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_URL_LOADER_CLIENT_IMPL_H_
