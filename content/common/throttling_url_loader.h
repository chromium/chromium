// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_THROTTLING_URL_LOADER_H_
#define CONTENT_COMMON_THROTTLING_URL_LOADER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/common/content_export.h"
#include "content/common/possibly_associated_interface_ptr.h"
#include "content/public/common/url_loader_throttle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

// ThrottlingURLLoader is a wrapper around the
// network::mojom::URLLoader[Factory] interfaces. It applies a list of
// URLLoaderThrottle instances which could defer, resume or cancel the URL
// loading. If the Mojo connection fails during the request it is canceled with
// net::ERR_ABORTED.
class CONTENT_EXPORT ThrottlingURLLoader
    : public network::mojom::URLLoaderClient {
 public:
  // |client| must stay alive during the lifetime of the returned object. Please
  // note that the request may not start immediately since it could be deferred
  // by throttles.
  static std::unique_ptr<ThrottlingURLLoader> CreateLoaderAndStart(
      scoped_refptr<network::SharedURLLoaderFactory> factory,
      std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      network::ResourceRequest* url_request,
      network::mojom::URLLoaderClient* client,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  ~ThrottlingURLLoader() override;

  void FollowRedirect(
      const base::Optional<net::HttpRequestHeaders>& modified_request_headers);
  // Follows a redirect, calling CreateLoaderAndStart() on the factory. This
  // is useful if the factory uses different loaders for different URLs.
  void FollowRedirectForcingRestart();
  void SetPriority(net::RequestPriority priority, int32_t intra_priority_value);

  // Restarts the load immediately with |factory| and |url_loader_options|.
  // It must only be called when the following conditions are met:
  // 1. The request already started and the original factory decided to not
  //    handle the request. This condition is required because throttles are not
  //    consulted prior to restarting.
  // 2. The original factory did not call URLLoaderClient callbacks (e.g.,
  //    OnReceiveResponse).
  // This function is useful in the case of service worker network fallback.
  void RestartWithFactory(
      scoped_refptr<network::SharedURLLoaderFactory> factory,
      uint32_t url_loader_options);

  // Disconnect the forwarding URLLoaderClient and the URLLoader. Returns the
  // datapipe endpoints.
  network::mojom::URLLoaderClientEndpointsPtr Unbind();

  // Sets the forwarding client to receive all subsequent notifications.
  void set_forwarding_client(network::mojom::URLLoaderClient* client) {
    forwarding_client_ = client;
  }

  bool response_intercepted() const { return response_intercepted_; }

 private:
  class ForwardingThrottleDelegate;

  ThrottlingURLLoader(
      std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
      network::mojom::URLLoaderClient* client,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  void Start(scoped_refptr<network::SharedURLLoaderFactory> factory,
             int32_t routing_id,
             int32_t request_id,
             uint32_t options,
             network::ResourceRequest* url_request,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  void StartNow();

  // Processes the result of a URLLoaderThrottle call, adding the throttle to
  // the blocking set if it deferred and updating |*should_defer| accordingly.
  // Returns |true| if the request should continue to be processed (regardless
  // of whether it's been deferred) or |false| if it's been cancelled.
  bool HandleThrottleResult(URLLoaderThrottle* throttle,
                            bool throttle_deferred,
                            bool* should_defer);

  // Stops a given throttle from deferring the request. If this was not the last
  // deferring throttle, the request remains deferred. Otherwise it resumes
  // progress.
  void StopDeferringForThrottle(URLLoaderThrottle* throttle);

  // network::mojom::URLLoaderClient implementation:
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

  void OnClientConnectionError();

  void CancelWithError(int error_code, base::StringPiece custom_reason);
  void Resume();
  void SetPriority(net::RequestPriority priority);
  void UpdateDeferredResponseHead(
      const network::ResourceResponseHead& new_response_head);
  void PauseReadingBodyFromNet(URLLoaderThrottle* throttle);
  void ResumeReadingBodyFromNet(URLLoaderThrottle* throttle);
  void InterceptResponse(
      network::mojom::URLLoaderPtr new_loader,
      network::mojom::URLLoaderClientRequest new_client_request,
      network::mojom::URLLoaderPtr* original_loader,
      network::mojom::URLLoaderClientRequest* original_client_request);

  // Disconnects the client connection and releases the URLLoader.
  void DisconnectClient(base::StringPiece custom_description);

  // TODO(crbug.com/882661): Remove when the linked bug is fixed.
  void Crash();

  enum DeferredStage {
    DEFERRED_NONE,
    DEFERRED_START,
    DEFERRED_REDIRECT,
    DEFERRED_RESPONSE
  };
  DeferredStage deferred_stage_ = DEFERRED_NONE;
  bool loader_completed_ = false;

  struct ThrottleEntry {
    ThrottleEntry(ThrottlingURLLoader* loader,
                  std::unique_ptr<URLLoaderThrottle> the_throttle);
    ThrottleEntry(ThrottleEntry&& other);
    ~ThrottleEntry();

    ThrottleEntry& operator=(ThrottleEntry&& other);

    std::unique_ptr<ForwardingThrottleDelegate> delegate;
    std::unique_ptr<URLLoaderThrottle> throttle;

   private:
    DISALLOW_COPY_AND_ASSIGN(ThrottleEntry);
  };

  std::vector<ThrottleEntry> throttles_;
  std::set<URLLoaderThrottle*> deferring_throttles_;
  std::set<URLLoaderThrottle*> pausing_reading_body_from_net_throttles_;

  // NOTE: This may point to a native implementation (instead of a Mojo proxy
  // object). And it is possible that the implementation of |forwarding_client_|
  // destroys this object synchronously when this object is calling into it.
  network::mojom::URLLoaderClient* forwarding_client_;
  mojo::Binding<network::mojom::URLLoaderClient> client_binding_;

  network::mojom::URLLoaderPtr url_loader_;

  struct StartInfo {
    StartInfo(
        scoped_refptr<network::SharedURLLoaderFactory> in_url_loader_factory,
        int32_t in_routing_id,
        int32_t in_request_id,
        uint32_t in_options,
        network::ResourceRequest* in_url_request,
        scoped_refptr<base::SingleThreadTaskRunner> in_task_runner);
    ~StartInfo();

    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
    int32_t routing_id;
    int32_t request_id;
    uint32_t options;

    network::ResourceRequest url_request;
    // |task_runner_| is used to set up |client_binding_|.
    scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  };
  // Holds any info needed to start or restart the request. Used when start is
  // deferred or when FollowRedirectForcingRestart() is called.
  std::unique_ptr<StartInfo> start_info_;

  struct ResponseInfo {
    explicit ResponseInfo(
        const network::ResourceResponseHead& in_response_head);
    ~ResponseInfo();

    network::ResourceResponseHead response_head;
  };
  // Set if response is deferred.
  std::unique_ptr<ResponseInfo> response_info_;

  struct RedirectInfo {
    RedirectInfo(const net::RedirectInfo& in_redirect_info,
                 const network::ResourceResponseHead& in_response_head);
    ~RedirectInfo();

    net::RedirectInfo redirect_info;
    network::ResourceResponseHead response_head;
  };
  // Set if redirect is deferred.
  std::unique_ptr<RedirectInfo> redirect_info_;

  struct PriorityInfo {
    PriorityInfo(net::RequestPriority in_priority,
                 int32_t in_intra_priority_value);
    ~PriorityInfo();

    net::RequestPriority priority;
    int32_t intra_priority_value;
  };
  // Set if request is deferred and SetPriority() is called.
  std::unique_ptr<PriorityInfo> priority_info_;

  // Set if a throttle changed the URL in WillStartRequest.
  GURL throttle_redirect_url_;

  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  uint32_t inside_delegate_calls_ = 0;

  // The latest request URL from where we expect a response
  GURL response_url_;

  bool response_intercepted_ = false;

  // TODO(crbug.com/882661): Remove these when the linked bug is fixed.
  bool sent_on_receive_response_ = false;
  std::vector<std::string> debug_log_;

  base::Optional<std::vector<std::string>> to_be_removed_request_headers_;
  base::Optional<net::HttpRequestHeaders> modified_request_headers_;

  base::WeakPtrFactory<ThrottlingURLLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThrottlingURLLoader);
};

}  // namespace content

#endif  // CONTENT_COMMON_THROTTLING_URL_LOADER_H_
