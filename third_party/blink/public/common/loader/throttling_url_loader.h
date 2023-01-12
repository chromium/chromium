// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_THROTTLING_URL_LOADER_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_THROTTLING_URL_LOADER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/accept_ch_frame_observer.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "services/network/public/mojom/web_client_hints_types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

// ThrottlingURLLoader is a wrapper around the
// network::mojom::URLLoader[Factory] interfaces. It applies a list of
// URLLoaderThrottle instances which could defer, resume restart or cancel the
// URL loading. If the Mojo connection fails during the request it is canceled
// with net::ERR_ABORTED.
class BLINK_COMMON_EXPORT ThrottlingURLLoader
    : public network::mojom::URLLoaderClient {
 public:
  // Reason used when resetting the URLLoader to follow a redirect.
  static const char kFollowRedirectReason[];

  // |url_request| can be mutated by this function, and doesn't need to stay
  // alive after calling this function.
  //
  // |client| must stay alive during the lifetime of the returned object. Please
  // note that the request may not start immediately since it could be deferred
  // by throttles.
  static std::unique_ptr<ThrottlingURLLoader> CreateLoaderAndStart(
      scoped_refptr<network::SharedURLLoaderFactory> factory,
      std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
      int32_t request_id,
      uint32_t options,
      network::ResourceRequest* url_request,
      network::mojom::URLLoaderClient* client,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      absl::optional<std::vector<std::string>> cors_exempt_header_list =
          absl::nullopt);

  ThrottlingURLLoader(const ThrottlingURLLoader&) = delete;
  ThrottlingURLLoader& operator=(const ThrottlingURLLoader&) = delete;
  ~ThrottlingURLLoader() override;

  // Follows a redirect, calling CreateLoaderAndStart() on the factory. This
  // is useful if the factory uses different loaders for different URLs.
  void FollowRedirectForcingRestart();
  // This should be called if the loader will be recreated to follow a redirect
  // instead of calling FollowRedirect(). This can be used if a loader is
  // implementing similar logic to FollowRedirectForcingRestart(). If this is
  // called, a future request for the redirect should be guaranteed to be sent
  // with the same request_id.
  // `removed_headers`, `modified_headers` and `modified_cors_exempt_headers`
  // will be merged to corresponding members in the ThrottlingURLLoader, and
  // then apply updates against `resource_request`.
  void ResetForFollowRedirect(
      network::ResourceRequest& resource_request,
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers);

  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers);
  void SetPriority(net::RequestPriority priority, int32_t intra_priority_value);
  void PauseReadingBodyFromNet();
  void ResumeReadingBodyFromNet();

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

  void CancelWithError(int error_code, base::StringPiece custom_reason);

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
             int32_t request_id,
             uint32_t options,
             network::ResourceRequest* url_request,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner,
             absl::optional<std::vector<std::string>> cors_exempt_header_list);

  void StartNow();
  void RestartWithFlagsNow();

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

  void RestartWithFlags(int additional_load_flags);

  // Restart the request using |original_url_|.
  void RestartWithURLResetAndFlags(int additional_load_flags);

  // Restart the request immediately if the response has not started yet.
  void RestartWithURLResetAndFlagsNow(int additional_load_flags);

  // network::mojom::URLLoaderClient implementation:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle body,
      absl::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  void OnClientConnectionError();

  void Resume();
  void SetPriority(net::RequestPriority priority);
  void UpdateDeferredRequestHeaders(
      const net::HttpRequestHeaders& modified_request_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_request_headers);
  void UpdateRequestHeaders(network::ResourceRequest& resource_request);
  void UpdateDeferredResponseHead(
      network::mojom::URLResponseHeadPtr new_response_head,
      mojo::ScopedDataPipeConsumerHandle body);
  void PauseReadingBodyFromNet(URLLoaderThrottle* throttle);
  void ResumeReadingBodyFromNet(URLLoaderThrottle* throttle);
  void InterceptResponse(
      mojo::PendingRemote<network::mojom::URLLoader> new_loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>
          new_client_receiver,
      mojo::PendingRemote<network::mojom::URLLoader>* original_loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>*
          original_client_receiver,
      mojo::ScopedDataPipeConsumerHandle* body);

  // Disconnects the client connection and releases the URLLoader.
  void DisconnectClient(base::StringPiece custom_description);

  enum DeferredStage {
    DEFERRED_NONE,
    DEFERRED_START,
    DEFERRED_REDIRECT,
    DEFERRED_BEFORE_RESPONSE,
    DEFERRED_RESPONSE,
    DEFERRED_COMPLETE
  };
  const char* GetStageNameForHistogram(DeferredStage stage);

  DeferredStage deferred_stage_ = DEFERRED_NONE;
  bool loader_completed_ = false;
  bool did_receive_response_ = false;

  struct ThrottleEntry {
    ThrottleEntry(ThrottlingURLLoader* loader,
                  std::unique_ptr<URLLoaderThrottle> the_throttle);
    ThrottleEntry(ThrottleEntry&& other);
    ThrottleEntry& operator=(ThrottleEntry&& other);
    ~ThrottleEntry();

    std::unique_ptr<URLLoaderThrottle> throttle;
    std::unique_ptr<ForwardingThrottleDelegate> delegate;
  };

  std::vector<ThrottleEntry> throttles_;
  std::map<URLLoaderThrottle*, /*start=*/base::Time> deferring_throttles_;
  // nullptr is used when this loader is directly requested to pause reading
  // body from net by calling PauseReadingBodyFromNet().
  std::set<URLLoaderThrottle*> pausing_reading_body_from_net_throttles_;

  // NOTE: This may point to a native implementation (instead of a Mojo proxy
  // object). And it is possible that the implementation of |forwarding_client_|
  // destroys this object synchronously when this object is calling into it.
  raw_ptr<network::mojom::URLLoaderClient, DanglingUntriaged>
      forwarding_client_;
  mojo::Remote<network::mojom::URLLoader> url_loader_;

  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver_{this};

  struct StartInfo {
    StartInfo(
        scoped_refptr<network::SharedURLLoaderFactory> in_url_loader_factory,
        int32_t in_request_id,
        uint32_t in_options,
        network::ResourceRequest* in_url_request,
        scoped_refptr<base::SingleThreadTaskRunner> in_task_runner,
        absl::optional<std::vector<std::string>> in_cors_exempt_header_list);
    ~StartInfo();

    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
    int32_t request_id;
    uint32_t options;

    network::ResourceRequest url_request;
    // |task_runner| is used to set up |client_receiver_|.
    scoped_refptr<base::SingleThreadTaskRunner> task_runner;
    absl::optional<std::vector<std::string>> cors_exempt_header_list;
  };
  // Holds any info needed to start or restart the request. Used when start is
  // deferred or when FollowRedirectForcingRestart() is called.
  std::unique_ptr<StartInfo> start_info_;

  struct ResponseInfo {
    explicit ResponseInfo(network::mojom::URLResponseHeadPtr in_response_head);
    ~ResponseInfo();

    network::mojom::URLResponseHeadPtr response_head;
  };
  // Set if response is deferred.
  std::unique_ptr<ResponseInfo> response_info_;
  mojo::ScopedDataPipeConsumerHandle body_;
  absl::optional<mojo_base::BigBuffer> cached_metadata_;

  struct RedirectInfo {
    RedirectInfo(const net::RedirectInfo& in_redirect_info,
                 network::mojom::URLResponseHeadPtr in_response_head);
    ~RedirectInfo();

    net::RedirectInfo redirect_info;
    network::mojom::URLResponseHeadPtr response_head;
  };
  // Set if redirect is deferred.
  std::unique_ptr<RedirectInfo> redirect_info_;

  struct PriorityInfo {
    PriorityInfo(net::RequestPriority in_priority,
                 int32_t in_intra_priority_value);

    net::RequestPriority priority;
    int32_t intra_priority_value;
  };
  // Set if request is deferred and SetPriority() is called.
  std::unique_ptr<PriorityInfo> priority_info_;

  // Set if a throttle changed the URL in WillStartRequest.
  GURL throttle_will_start_redirect_url_;

  // Set if a throttle changed the URL in WillRedirectRequest.
  // Only supported with the network service.
  GURL throttle_will_redirect_redirect_url_;

  // Set if the request should be made using the |original_url_|.
  bool throttle_will_start_original_url_ = false;
  // The first URL seen by the throttle.
  GURL original_url_;

  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  uint32_t inside_delegate_calls_ = 0;

  // The latest request URL from where we expect a response
  GURL response_url_;

  bool response_intercepted_ = false;

  std::vector<std::string> removed_headers_;
  net::HttpRequestHeaders modified_headers_;
  net::HttpRequestHeaders modified_cors_exempt_headers_;

  int pending_restart_flags_ = 0;
  bool has_pending_restart_ = false;

  base::WeakPtrFactory<ThrottlingURLLoader> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_THROTTLING_URL_LOADER_H_
