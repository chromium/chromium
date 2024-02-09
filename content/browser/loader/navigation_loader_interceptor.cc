// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_loader_interceptor.h"

#include "content/browser/navigation_subresource_loader_params.h"

namespace content {

bool NavigationLoaderInterceptor::MaybeCreateLoaderForResponse(
    const network::URLLoaderCompletionStatus& status,
    const network::ResourceRequest& request,
    network::mojom::URLResponseHeadPtr* response,
    mojo::ScopedDataPipeConsumerHandle* response_body,
    mojo::PendingRemote<network::mojom::URLLoader>* loader,
    mojo::PendingReceiver<network::mojom::URLLoaderClient>* client_receiver,
    blink::ThrottlingURLLoader* url_loader,
    bool* skip_other_interceptors) {
  return false;
}

NavigationLoaderInterceptor::Result::Result(
    scoped_refptr<network::SharedURLLoaderFactory> single_request_factory,
    SubresourceLoaderParams subresource_loader_params,
    ResponseHeadUpdateParams response_head_update_params)
    : single_request_factory(std::move(single_request_factory)),
      subresource_loader_params(std::move(subresource_loader_params)),
      response_head_update_params(std::move(response_head_update_params)) {}
NavigationLoaderInterceptor::Result::Result(
    NavigationLoaderInterceptor::Result&&) = default;
NavigationLoaderInterceptor::Result&
NavigationLoaderInterceptor::Result::operator=(
    NavigationLoaderInterceptor::Result&&) = default;
NavigationLoaderInterceptor::Result::~Result() = default;

}  // namespace content
