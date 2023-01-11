// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_URL_LOADER_H_
#define CONTENT_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_URL_LOADER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/weak_document_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace content {

// A URLLoader for handling a topics request, including
// fetch(<url>, {browsingTopics: true}).
//
// This loader works as follows:
//   1. Before making a network request (i.e. BrowsingTopicsURLLoader()), if the
//      request is eligible for topics, calculates and adds the topics header.
//      Starts the request with `loader_`.
//   2. For any redirect received (i.e. OnReceiveRedirect()), if the previous
//      request or redirect was eligible for topics, and if the response header
//      indicates an observation should be recorded, stores the observation.
//      Forwards the original response back to `forwarding_client_`.
//   3. For any followed redirect (i.e. FollowRedirect()),  if the redirect is
//      eligible for topics, calculates and adds/updates the topics header.
//      Forwards the updated redirect to `loader_`.
//   4. For the last response (i.e. OnReceiveResponse()),  if the previous
//      request or redirect was eligible for topics, and if the response header
//      indicates an observation should be recorded, stores the observation.
//      Forwards the original response (e.g. hands off fetching the body) back
//      to `forwarding_client_`.
class CONTENT_EXPORT BrowsingTopicsURLLoader
    : public network::mojom::URLLoader,
      public network::mojom::URLLoaderClient {
 public:
  BrowsingTopicsURLLoader(
      WeakDocumentPtr document,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory);

  BrowsingTopicsURLLoader(const BrowsingTopicsURLLoader&) = delete;
  BrowsingTopicsURLLoader& operator=(const BrowsingTopicsURLLoader&) = delete;

  ~BrowsingTopicsURLLoader() override;

 private:
  // network::mojom::URLLoader overrides:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // network::mojom::URLLoaderClient overrides:
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
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  void OnNetworkConnectionError();

  // Upon NavigationRequest::DidCommitNavigation(), `document_` will be set to
  // the document that this `BrowsingTopicsURLLoader` is associated with. It
  // will become null whenever the document navigates away.
  WeakDocumentPtr document_;

  // The current request or redirect URL.
  GURL url_;

  // The initial request state. This will be used to derive the opt-in
  // permissions policy features for each request/redirect.
  network::ResourceRequest request_;

  // Whether the ongoing request or redirect is eligible for topics. Set to the
  // desired state when a request/redirect is made. Reset to false when the
  // corresponding response is received.
  bool topics_eligible_ = false;

  // For the actual request.
  mojo::Remote<network::mojom::URLLoader> loader_;

  // The client to forward the response to.
  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_URL_LOADER_H_
