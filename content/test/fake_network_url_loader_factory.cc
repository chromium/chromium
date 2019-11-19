// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fake_network_url_loader_factory.h"

#include "base/strings/string_util.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

namespace {

const char kDefaultHeader[] = "HTTP/1.1 200 OK\n\n";
const char kDefaultBody[] = "this body came from the network";
const char kDefaultHeaderForJS[] =
    "HTTP/1.1 200 OK\n"
    "Content-Type: application/javascript\n\n";
const char kDefaultBodyForJS[] = "/*this body came from the network*/";

}  // namespace

FakeNetworkURLLoaderFactory::ResponseInfo::ResponseInfo(
    const std::string& headers,
    const std::string& body,
    bool network_accessed,
    net::Error error_code)
    : headers(headers),
      body(body),
      network_accessed(network_accessed),
      error_code(error_code) {}

FakeNetworkURLLoaderFactory::ResponseInfo::ResponseInfo() = default;
FakeNetworkURLLoaderFactory::ResponseInfo::ResponseInfo(
    FakeNetworkURLLoaderFactory::ResponseInfo&& info) = default;
FakeNetworkURLLoaderFactory::ResponseInfo::~ResponseInfo() = default;
FakeNetworkURLLoaderFactory::ResponseInfo&
FakeNetworkURLLoaderFactory::ResponseInfo::operator=(
    FakeNetworkURLLoaderFactory::ResponseInfo&& info) = default;

FakeNetworkURLLoaderFactory::FakeNetworkURLLoaderFactory() = default;

FakeNetworkURLLoaderFactory::FakeNetworkURLLoaderFactory(
    const std::string& headers,
    const std::string& body,
    bool network_accessed,
    net::Error error_code)
    : user_defined_default_response_info_(
          std::make_unique<FakeNetworkURLLoaderFactory::ResponseInfo>(
              headers,
              body,
              network_accessed,
              error_code)) {}

FakeNetworkURLLoaderFactory::~FakeNetworkURLLoaderFactory() = default;

void FakeNetworkURLLoaderFactory::SetResponse(const GURL& url,
                                              const std::string& headers,
                                              const std::string& body,
                                              bool network_accessed,
                                              net::Error error_code) {
  response_info_map_[url] =
      ResponseInfo(headers, body, network_accessed, error_code);
}

const FakeNetworkURLLoaderFactory::ResponseInfo&
FakeNetworkURLLoaderFactory::FindResponseInfo(const GURL& url) const {
  auto it = response_info_map_.find(url);
  if (it != response_info_map_.end())
    return it->second;

  if (user_defined_default_response_info_)
    return *user_defined_default_response_info_;

  static const base::NoDestructor<ResponseInfo> kDefaultResponseInfo(
      kDefaultHeader, kDefaultBody, /*network_accessed=*/true, net::OK);
  static const base::NoDestructor<ResponseInfo> kDefaultJsResponseInfo(
      kDefaultHeaderForJS, kDefaultBodyForJS, /*network_accessed=*/true,
      net::OK);
  bool is_js =
      base::EndsWith(url.path(), ".js", base::CompareCase::INSENSITIVE_ASCII);

  return is_js ? *kDefaultJsResponseInfo : *kDefaultResponseInfo;
}

void FakeNetworkURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  const ResponseInfo& response_info = FindResponseInfo(url_request.url);
  mojo::Remote<network::mojom::URLLoaderClient> client_remote(
      std::move(client));

  net::HttpResponseInfo info;
  info.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(response_info.headers));
  network::ResourceResponseHead response;
  response.headers = info.headers;
  response.headers->GetMimeType(&response.mime_type);
  response.network_accessed = response_info.network_accessed;
  client_remote->OnReceiveResponse(response);

  uint32_t bytes_written = response_info.body.size();
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  CHECK_EQ(MOJO_RESULT_OK,
           mojo::CreateDataPipe(nullptr, &producer_handle, &consumer_handle));
  producer_handle->WriteData(response_info.body.data(), &bytes_written,
                             MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
  client_remote->OnStartLoadingResponseBody(std::move(consumer_handle));

  network::URLLoaderCompletionStatus status;
  status.error_code = response_info.error_code;
  client_remote->OnComplete(status);
}

void FakeNetworkURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace content
