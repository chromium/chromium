// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_URL_LOADER_INTERCEPTOR_H_
#define CONTENT_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_URL_LOADER_INTERCEPTOR_H_

#include "content/browser/loader/subresource_proxying_url_loader.h"

namespace content {

// A loader interceptor for handling a topics subresource request, including
// fetch(<url>, {browsingTopics: true}).
//
// This loader interceptor works as follows:
//   1. Before making a network request (i.e. WillStartRequest()), if the
//      request is eligible for topics, calculates and adds the topics header.
//   2. For any redirect received (i.e. OnReceiveRedirect()), if the previous
//      request or redirect was eligible for topics, and if the response header
//      indicates an observation should be recorded, stores the observation.
//   3. For any followed redirect (i.e. WillFollowRedirect()),  if the redirect
//      is eligible for topics, calculates and adds/updates the topics header.
//   4. For the last response (i.e. OnReceiveResponse()),  if the previous
//      request or redirect was eligible for topics, and if the response header
//      indicates an observation should be recorded, stores the observation.
class CONTENT_EXPORT BrowsingTopicsURLLoaderInterceptor
    : public SubresourceProxyingURLLoader::Interceptor {
 public:
  BrowsingTopicsURLLoaderInterceptor(
      WeakDocumentPtr document,
      const network::ResourceRequest& resource_request);

  BrowsingTopicsURLLoaderInterceptor(
      const BrowsingTopicsURLLoaderInterceptor&) = delete;
  BrowsingTopicsURLLoaderInterceptor& operator=(
      const BrowsingTopicsURLLoaderInterceptor&) = delete;

  ~BrowsingTopicsURLLoaderInterceptor() override;

  // SubresourceProxyingURLLoader::Interceptor
  void WillStartRequest(net::HttpRequestHeaders& headers) override;
  void WillFollowRedirect(const std::optional<GURL>& new_url,
                          std::vector<std::string>& removed_headers,
                          net::HttpRequestHeaders& modified_headers) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr& head) override;
  void OnReceiveResponse(network::mojom::URLResponseHeadPtr& head) override;

 private:
  // Determines whether the ongoing request or redirect is eligible for topics,
  // and updates `topics_eligible_`. If the request is eligible for topics,
  // calculates and adds the topics header to `headers`. If `removed_headers` is
  // provided (implying we are processing a redirect request), appends the
  // topics header to `removed_headers`.
  void PopulateRequestOrRedirectHeaders(
      bool is_redirect,
      net::HttpRequestHeaders& headers,
      std::vector<std::string>* removed_headers);

  // If the previous request or redirect was eligible for topics, and if the
  // response header indicates that a topics observation should be recorded,
  // stores the observation.
  void ProcessRedirectOrResponseHeaders(
      const network::mojom::URLResponseHeadPtr& head);

  // Upon NavigationRequest::DidCommitNavigation(), `document_` will be set to
  // the document that this request is associated with. It will become null
  // whenever the document navigates away.
  WeakDocumentPtr document_;

  // The initial request state. This will be used to derive the opt-in
  // permissions policy features for each request/redirect.
  const raw_ref<const network::ResourceRequest> resource_request_;

  // The current request or redirect URL.
  GURL url_;

  // Whether the ongoing request or redirect is eligible for topics. Set to the
  // desired state when a request/redirect is made. Reset to false when the
  // corresponding response is received.
  bool topics_eligible_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_URL_LOADER_INTERCEPTOR_H_
