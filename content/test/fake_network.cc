// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fake_network.h"

#include "base/strings/string_util.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

namespace {

const char kDefaultHttpHeader[] = "HTTP/1.1 200 OK\n\n";
const char kDefaultHttpBody[] = "this body came from the network";
const char kDefaultHttpHeaderForJS[] =
    "HTTP/1.1 200 OK\n"
    "Content-Type: application/javascript\n\n";
const char kDefaultHttpBodyForJS[] = "/*this body came from the network*/";

}  // namespace

struct FakeNetwork::ResponseInfo {
  ResponseInfo() = default;
  ResponseInfo(const std::string& headers,
               const std::string& body,
               bool network_accessed,
               net::Error error_code)
      : headers(headers),
        body(body),
        network_accessed(network_accessed),
        error_code(error_code) {}
  ~ResponseInfo() = default;

  ResponseInfo(ResponseInfo&& info) = default;
  ResponseInfo& operator=(ResponseInfo&& info) = default;

  GURL url;
  std::string headers;
  std::string body;
  bool network_accessed = true;
  net::Error error_code = net::OK;
};

FakeNetwork::FakeNetwork() = default;

FakeNetwork::~FakeNetwork() = default;

const FakeNetwork::ResponseInfo& FakeNetwork::FindResponseInfo(
    const GURL& url) const {
  auto it = response_info_map_.find(url);
  if (it != response_info_map_.end())
    return it->second;

  if (user_defined_default_response_info_)
    return *user_defined_default_response_info_;

  static const base::NoDestructor<ResponseInfo> kDefaultResponseInfo(
      kDefaultHttpHeader, kDefaultHttpBody, /*network_accessed=*/true, net::OK);
  static const base::NoDestructor<ResponseInfo> kDefaultJsResponseInfo(
      kDefaultHttpHeaderForJS, kDefaultHttpBodyForJS, /*network_accessed=*/true,
      net::OK);
  bool is_js =
      base::EndsWith(url.path(), ".js", base::CompareCase::INSENSITIVE_ASCII);

  return is_js ? *kDefaultJsResponseInfo : *kDefaultResponseInfo;
}

void FakeNetwork::SetDefaultResponse(const std::string& headers,
                                     const std::string& body,
                                     bool network_accessed,
                                     net::Error error_code) {
  user_defined_default_response_info_ =
      std::make_unique<FakeNetwork::ResponseInfo>(headers, body,
                                                  network_accessed, error_code);
}

void FakeNetwork::SetResponse(const GURL& url,
                              const std::string& headers,
                              const std::string& body,
                              bool network_accessed,
                              net::Error error_code) {
  response_info_map_[url] =
      ResponseInfo(headers, body, network_accessed, error_code);
}

bool FakeNetwork::HandleRequest(URLLoaderInterceptor::RequestParams* params) {
  const network::ResourceRequest& url_request = params->url_request;
  const ResponseInfo& response_info = FindResponseInfo(url_request.url);

  net::HttpResponseInfo info;
  info.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(response_info.headers));
  network::ResourceResponseHead response;
  response.headers = info.headers;
  response.headers->GetMimeType(&response.mime_type);
  response.network_accessed = response_info.network_accessed;

  mojo::Remote<network::mojom::URLLoaderClient>& client = params->client;
  client->OnReceiveResponse(response);

  uint32_t bytes_written = response_info.body.size();
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  CHECK_EQ(MOJO_RESULT_OK,
           mojo::CreateDataPipe(nullptr, &producer_handle, &consumer_handle));
  producer_handle->WriteData(response_info.body.data(), &bytes_written,
                             MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
  client->OnStartLoadingResponseBody(std::move(consumer_handle));

  network::URLLoaderCompletionStatus status;
  status.error_code = response_info.error_code;
  client->OnComplete(status);
  return true;
}

}  // namespace content
