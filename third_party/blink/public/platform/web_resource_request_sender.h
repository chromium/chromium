// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RESOURCE_REQUEST_SENDER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RESOURCE_REQUEST_SENDER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom-forward.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/common/loader/previews_state.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_mojo_url_loader_client_observer.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "url/gurl.h"

namespace base {
class WaitableEvent;
}

namespace net {
struct RedirectInfo;
}

namespace network {
struct ResourceRequest;
struct URLLoaderCompletionStatus;
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

namespace blink {
class ResourceLoadInfoNotifierWrapper;
class ThrottlingURLLoader;
class MojoURLLoaderClient;
class WebRequestPeer;
class WebResourceRequestSenderDelegate;
struct SyncLoadResponse;

// This class creates a PendingRequestInfo object and handles sending a resource
// request asynchronously or synchronously, and it's owned by
// WebURLLoaderImpl::Context or SyncLoadContext.
class BLINK_PLATFORM_EXPORT WebResourceRequestSender
    : public WebMojoURLLoaderClientObserver {
 public:
  // Generates ids for requests initiated by child processes unique to the
  // particular process, counted up from 0 (browser initiated requests count
  // down from -2).
  //
  // Public to be used by URLLoaderFactory and/or URLLoader implementations with
  // the need to perform additional requests besides the main request, e.g.,
  // CORS preflight requests.
  static int MakeRequestID();

  WebResourceRequestSender();
  WebResourceRequestSender(const WebResourceRequestSender&) = delete;
  WebResourceRequestSender& operator=(const WebResourceRequestSender&) = delete;
  ~WebResourceRequestSender() override;

  // Call this method to load the resource synchronously (i.e., in one shot).
  // This is an alternative to the StartAsync method. Be warned that this method
  // will block the calling thread until the resource is fully downloaded or an
  // error occurs. It could block the calling thread for a long time, so only
  // use this if you really need it!  There is also no way for the caller to
  // interrupt this method. Errors are reported via the status field of the
  // response parameter.
  //
  // |routing_id| is used to associated the bridge with a frame's network
  // context.
  // |timeout| is used to abort the sync request on timeouts. TimeDelta::Max()
  // is interpreted as no-timeout.
  // If |download_to_blob_registry| is not null, it is used to redirect the
  // download to a blob.
  virtual void SendSync(
      std::unique_ptr<network::ResourceRequest> request,
      int routing_id,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      uint32_t loader_options,
      SyncLoadResponse* response,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
      base::TimeDelta timeout,
      const std::vector<std::string>& cors_exempt_header_list,
      base::WaitableEvent* terminate_sync_load_event,
      mojo::PendingRemote<mojom::BlobRegistry> download_to_blob_registry,
      scoped_refptr<WebRequestPeer> peer,
      std::unique_ptr<ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper);

  // Call this method to initiate the request. If this method succeeds, then
  // the peer's methods will be called asynchronously to report various events.
  // Returns the request id. |url_loader_factory| must be non-null.
  //
  // |routing_id| is used to associated the bridge with a frame's network
  // context.
  //
  // You need to pass a non-null |loading_task_runner| to specify task queue to
  // execute loading tasks on.
  virtual int SendAsync(
      std::unique_ptr<network::ResourceRequest> request,
      int routing_id,
      scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      uint32_t loader_options,
      const std::vector<std::string>& cors_exempt_header_list,
      scoped_refptr<WebRequestPeer> peer,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
      std::unique_ptr<ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper);

  // Cancels the current request and `request_info_` will be released.
  virtual void Cancel(scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Toggles the is_deferred attribute for the specified request.
  virtual void SetDefersLoading(WebURLLoader::DeferType value);

  // Indicates the priority of the specified request changed.
  void DidChangePriority(net::RequestPriority new_priority,
                         int intra_priority_value);

  virtual void DeletePendingRequest(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

 private:
  friend class URLLoaderClientImpl;
  friend class URLResponseBodyConsumer;

  struct PendingRequestInfo {
    PendingRequestInfo(scoped_refptr<WebRequestPeer> peer,
                       network::mojom::RequestDestination request_destination,
                       int render_frame_id,
                       const GURL& request_url,
                       std::unique_ptr<ResourceLoadInfoNotifierWrapper>
                           resource_load_info_notifier_wrapper);

    ~PendingRequestInfo();

    scoped_refptr<WebRequestPeer> peer;
    network::mojom::RequestDestination request_destination;
    int render_frame_id;
    WebURLLoader::DeferType is_deferred = WebURLLoader::DeferType::kNotDeferred;
    // Original requested url.
    GURL url;
    // The url, method and referrer of the latest response even in case of
    // redirection.
    GURL response_url;
    bool has_pending_redirect = false;
    base::TimeTicks local_request_start;
    base::TimeTicks local_response_start;
    base::TimeTicks remote_request_start;
    net::LoadTimingInfo load_timing_info;
    bool should_follow_redirect = true;
    bool redirect_requires_loader_restart = false;
    // Network error code the request completed with, or net::ERR_IO_PENDING if
    // it's not completed. Used both to distinguish completion from
    // cancellation, and to log histograms.
    int net_error = net::ERR_IO_PENDING;
    PreviewsState previews_state = PreviewsTypes::PREVIEWS_UNSPECIFIED;

    // For mojo loading.
    std::unique_ptr<ThrottlingURLLoader> url_loader;
    std::unique_ptr<MojoURLLoaderClient> url_loader_client;

    // The Client Hints headers that need to be removed from a redirect.
    WebVector<WebString> removed_headers;

    // Used to notify the loading stats.
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper;
  };

  // Follows redirect, if any, for the given request.
  void FollowPendingRedirect(PendingRequestInfo* request_info);

  // Implements WebMojoURLLoaderClientObserver.
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnUploadProgress(int64_t position, int64_t size) override;
  void OnReceivedResponse(network::mojom::URLResponseHeadPtr) override;
  void OnReceivedCachedMetadata(mojo_base::BigBuffer data) override;
  void OnReceivedRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr head,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnRequestComplete(
      const network::URLLoaderCompletionStatus& status) override;
  void EvictFromBackForwardCache(mojom::RendererEvictionReason reason) override;
  void DidBufferLoadWhileInBackForwardCache(size_t num_bytes) override;
  bool CanContinueBufferingWhileInBackForwardCache() override;

  void ToLocalURLResponseHead(
      const PendingRequestInfo& request_info,
      network::mojom::URLResponseHead& response_head) const;

  // `delegate_` is expected to live longer than `this`.
  WebResourceRequestSenderDelegate* delegate_;

  // The instance is created on StartAsync() or StartSync(), and it's deleted
  // when the response has finished, or when the request is canceled.
  std::unique_ptr<PendingRequestInfo> request_info_;

  base::WeakPtrFactory<WebResourceRequestSender> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RESOURCE_REQUEST_SENDER_H_
