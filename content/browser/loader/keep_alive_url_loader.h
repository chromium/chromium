// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_KEEP_ALIVE_URL_LOADER_H_
#define CONTENT_BROWSER_LOADER_KEEP_ALIVE_URL_LOADER_H_

#include <stdint.h>

#include "base/functional/callback.h"
#include "base/types/pass_key.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace content {

class KeepAliveURLLoaderService;
class PolicyContainerHost;

// A URLLoader for loading a fetch keepalive request via the browser process,
// including both `fetch(..., {keepalive: true})` and `navigator.sendBeacon()`
// requests.
//
// To load a keepalive request initiated by a renderer, this loader performs the
// following logic:
// 1. Forwards all request loading actions received from a remote of
//    `mojom::URLLoader` in a renderer to a receiver of `mojom::URLLoader` in
//    the network service connected by `loader_`.
// 2. Receives request loading results from the network service, i.e. the remote
//    of `loader_receiver_`. The URLLoaderClient overrides will be triggered to
//    process results:
//    A. For redirect, perform security checks and ask the network service to
//       follow all subsequent redirects.
//    B. For non-redirect,
//       a. If the renderer is still alive, i.e. `forwarding_client_` is
//          connected, ask it to process the results instead.
//       b. If the renderer is dead, drop the results.
//
// Instances of this class must only be constructed and run within the browser
// process, such that the lifetime of the corresponding requests can be
// maintained by the browser instead of by a renderer.
//
// Design Doc:
// https://docs.google.com/document/d/1ZzxMMBvpqn8VZBZKnb7Go8TWjnrGcXuLS_USwVVRUvY
class CONTENT_EXPORT KeepAliveURLLoader
    : public network::mojom::URLLoader,
      public network::mojom::URLLoaderClient {
 public:
  // A callback type to delete this loader immediately on triggered.
  using OnDeleteCallback = base::OnceCallback<void(void)>;

  // Must only be constructed by a `KeepAliveURLLoaderService`.
  // `resource_request` must be a keepalive request from a renderer.
  // `forwarding_client` should handle request loading results from the network
  // service if it is still connected.
  // `delete_callback` is a callback to delete this object.
  // `policy_container_host` must not be null.
  KeepAliveURLLoader(
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
      scoped_refptr<PolicyContainerHost> policy_container_host,
      base::PassKey<KeepAliveURLLoaderService>);
  ~KeepAliveURLLoader() override;

  // Not copyable.
  KeepAliveURLLoader(const KeepAliveURLLoader&) = delete;
  KeepAliveURLLoader& operator=(const KeepAliveURLLoader&) = delete;

  // Sets the callback to be invoked on errors which require closing the pipe.
  // Running `on_delete_callback` will immediately delete `this`.
  //
  // Not an argument to constructor because the Mojo ReceiverId needs to be
  // bound to the callback, but can only be obtained after creating `this`.
  // Must be called immediately after creating a KeepAliveLoader.
  void set_on_delete_callback(OnDeleteCallback on_delete_callback);

  // For testing only:
  // TODO(crbug.com/1427366): Figure out alt to not rely on this in test.
  class TestObserver : public base::RefCountedThreadSafe<TestObserver> {
   public:
    virtual void OnReceiveRedirectForwarded(KeepAliveURLLoader* loader) = 0;
    virtual void OnReceiveRedirectProcessed(KeepAliveURLLoader* loader) = 0;
    virtual void OnReceiveResponseForwarded(KeepAliveURLLoader* loader) = 0;
    virtual void OnReceiveResponseProcessed(KeepAliveURLLoader* loader) = 0;
    virtual void OnCompleteForwarded(
        KeepAliveURLLoader* loader,
        const network::URLLoaderCompletionStatus& completion_status) = 0;
    virtual void OnCompleteProcessed(
        KeepAliveURLLoader* loader,
        const network::URLLoaderCompletionStatus& completion_status) = 0;

   protected:
    virtual ~TestObserver() = default;
    friend class base::RefCountedThreadSafe<TestObserver>;
  };
  void SetObserverForTesting(scoped_refptr<TestObserver> observer);

 private:
  // Receives actions from renderer.
  // `network::mojom::URLLoader` overrides:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // Receives actions from network service.
  // `network::mojom::URLLoaderClient` overrides:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      absl::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(
      const network::URLLoaderCompletionStatus& completion_status) override;

  // Returns net::OK to allow following the redirect. Otherwise, returns
  // corresponding error code.
  net::Error WillFollowRedirect(const net::RedirectInfo& redirect_info) const;
  void OnNetworkConnectionError();
  void OnRendererConnectionError();
  void DeleteSelf();

  // The ID to identify the request being loaded by this loader.
  const int32_t request_id_;

  // The request to be loaded by this loader.
  // Set in the constructor and updated when redirected.
  network::ResourceRequest resource_request_;

  // Connection with the network service:
  // Connects to the receiver network::URLLoader implemented in the network
  // service that performs actual request loading.
  mojo::Remote<network::mojom::URLLoader> loader_;
  // Connection with the network service:
  // Receives the result of the request loaded by `loader_` from the network
  // service.
  mojo::Receiver<network::mojom::URLLoaderClient> loader_receiver_{this};

  // Connection with a renderer:
  // Connects to the receiver URLLoaderClient implemented in the renderer.
  // It is the client that this loader may forward the URLLoader response from
  // the network service, i.e. message received by `loader_receiver_`, to.
  // It may be disconnected if the renderer is dead. In such case, subsequent
  // URLLoader response may be handled in browser.
  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  // A callback to delete this loader object and clean up resource.
  OnDeleteCallback on_delete_callback_;

  // Whether `OnReceiveResponse()` has been called.
  bool has_received_response_ = false;

  // A refptr to keep the `PolicyContainerHost` from the RenderFrameHost that
  // initiates this loader alive until `this` is destroyed.
  // It is never null.
  scoped_refptr<PolicyContainerHost> policy_container_host_;

  // Records the initial request URL to help veryfing redirect request.
  const GURL initial_url_;
  // Records the latest URL to help veryfing redirect request.
  GURL last_url_;

  // For testing only:
  // Not owned.
  scoped_refptr<TestObserver> observer_for_testing_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_KEEP_ALIVE_URL_LOADER_H_
