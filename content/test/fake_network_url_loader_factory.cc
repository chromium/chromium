// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fake_network_url_loader_factory.h"

#include "base/strings/string_util.h"
#include "content/public/browser/child_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {

FakeNetworkURLLoaderFactory::FakeNetworkURLLoaderFactory() = default;

FakeNetworkURLLoaderFactory::FakeNetworkURLLoaderFactory(
    const std::string& headers,
    const std::string& body,
    bool network_accessed,
    net::Error error_code) {
  fake_network_.SetDefaultResponse(headers, body, network_accessed, error_code);
}

FakeNetworkURLLoaderFactory::~FakeNetworkURLLoaderFactory() = default;

void FakeNetworkURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  URLLoaderInterceptor::RequestParams params;
  params.process_id = ChildProcessHost::kInvalidUniqueID;  // unused
  params.request_id = request_id;
  params.options = options;
  params.url_request = url_request;
  params.client.Bind(std::move(client));
  params.traffic_annotation = traffic_annotation;

  fake_network_.HandleRequest(&params);
}

void FakeNetworkURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace content
