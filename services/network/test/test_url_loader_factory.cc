// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_url_loader_factory.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_utils.h"

namespace network {

TestURLLoaderFactory::PendingRequest::PendingRequest() = default;
TestURLLoaderFactory::PendingRequest::~PendingRequest() = default;

TestURLLoaderFactory::PendingRequest::PendingRequest(PendingRequest&& other) =
    default;
TestURLLoaderFactory::PendingRequest& TestURLLoaderFactory::PendingRequest::
operator=(PendingRequest&& other) = default;

TestURLLoaderFactory::Response::Response() = default;
TestURLLoaderFactory::Response::~Response() = default;
TestURLLoaderFactory::Response::Response(Response&&) = default;
TestURLLoaderFactory::Response& TestURLLoaderFactory::Response::operator=(
    Response&&) = default;

TestURLLoaderFactory::TestURLLoaderFactory()
    : weak_wrapper_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              this)) {}

TestURLLoaderFactory::~TestURLLoaderFactory() {
  weak_wrapper_->Detach();
}

void TestURLLoaderFactory::AddResponse(const GURL& url,
                                       mojom::URLResponseHeadPtr head,
                                       const std::string& content,
                                       const URLLoaderCompletionStatus& status,
                                       Redirects redirects,
                                       ResponseProduceFlags flags) {
  Response response;
  response.url = url;
  response.redirects = std::move(redirects);
  response.head = std::move(head);
  response.content = content;
  response.status = status;
  response.status.decoded_body_length = content.size();
  response.flags = flags;
  responses_[url] = std::move(response);

  for (auto it = pending_requests_.begin(); it != pending_requests_.end();) {
    if (CreateLoaderAndStartInternal(it->request.url, it->client.get())) {
      it = pending_requests_.erase(it);
    } else {
      ++it;
    }
  }
}

void TestURLLoaderFactory::AddResponse(const std::string& url,
                                       const std::string& content,
                                       net::HttpStatusCode http_status) {
  mojom::URLResponseHeadPtr head = CreateURLResponseHead(http_status);
  head->mime_type = "text/html";
  URLLoaderCompletionStatus status;
  AddResponse(GURL(url), std::move(head), content, status);
}

bool TestURLLoaderFactory::IsPending(const std::string& url,
                                     const ResourceRequest** request_out) {
  base::RunLoop().RunUntilIdle();
  for (const auto& candidate : pending_requests_) {
    if (candidate.request.url == url) {
      if (request_out)
        *request_out = &candidate.request;
      return candidate.client.is_connected();
    }
  }
  return false;
}

int TestURLLoaderFactory::NumPending() {
  int pending = 0;
  base::RunLoop().RunUntilIdle();
  for (const auto& candidate : pending_requests_) {
    if (candidate.client.is_connected())
      ++pending;
  }
  return pending;
}

TestURLLoaderFactory::PendingRequest* TestURLLoaderFactory::GetPendingRequest(
    size_t index) {
  if (index >= pending_requests_.size())
    return nullptr;
  auto* request = &(pending_requests_[index]);
  DCHECK(request);
  return request;
}

void TestURLLoaderFactory::ClearResponses() {
  responses_.clear();
}

void TestURLLoaderFactory::SetInterceptor(const Interceptor& interceptor) {
  interceptor_ = interceptor;
}

void TestURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<mojom::URLLoader> receiver,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& url_request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (interceptor_)
    interceptor_.Run(url_request);

  mojo::Remote<mojom::URLLoaderClient> client_remote(std::move(client));
  if (CreateLoaderAndStartInternal(url_request.url, client_remote.get()))
    return;

  PendingRequest pending_request;
  pending_request.client = std::move(client_remote);
  pending_request.request = url_request;
  pending_request.options = options;
  pending_requests_.push_back(std::move(pending_request));
}

void TestURLLoaderFactory::Clone(
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
TestURLLoaderFactory::GetSafeWeakWrapper() {
  return weak_wrapper_;
}

bool TestURLLoaderFactory::CreateLoaderAndStartInternal(
    const GURL& url,
    mojom::URLLoaderClient* client) {
  auto it = responses_.find(url);
  if (it == responses_.end())
    return false;

  Redirects redirects;
  for (auto& redirect : it->second.redirects) {
    redirects.push_back(
        std::make_pair(redirect.first, redirect.second.Clone()));
  }
  SimulateResponse(client, std::move(redirects), it->second.head.Clone(),
                   it->second.content, it->second.status, it->second.flags);
  return true;
}

bool TestURLLoaderFactory::SimulateResponseForPendingRequest(
    const GURL& url,
    const network::URLLoaderCompletionStatus& completion_status,
    mojom::URLResponseHeadPtr response_head,
    const std::string& content,
    ResponseMatchFlags flags) {
  if (pending_requests_.empty())
    return false;

  const bool url_match_prefix = flags & kUrlMatchPrefix;
  const bool reverse = flags & kMostRecentMatch;

  // Give any cancellations a chance to happen...
  base::RunLoop().RunUntilIdle();

  bool found_request = false;
  network::TestURLLoaderFactory::PendingRequest request;
  for (int i = (reverse ? static_cast<int>(pending_requests_.size()) - 1 : 0);
       reverse ? i >= 0 : i < static_cast<int>(pending_requests_.size());
       reverse ? --i : ++i) {
    // Skip already cancelled.
    if (!pending_requests_[i].client.is_connected())
      continue;

    if (pending_requests_[i].request.url == url ||
        (url_match_prefix &&
         base::StartsWith(pending_requests_[i].request.url.spec(), url.spec(),
                          base::CompareCase::INSENSITIVE_ASCII))) {
      request = std::move(pending_requests_[i]);
      pending_requests_.erase(pending_requests_.begin() + i);
      found_request = true;
      break;
    }
  }
  if (!found_request)
    return false;

  // |decoded_body_length| must be set to the right size or the SimpleURLLoader
  // will fail.
  network::URLLoaderCompletionStatus status(completion_status);
  status.decoded_body_length = content.size();

  SimulateResponse(request.client.get(), TestURLLoaderFactory::Redirects(),
                   std::move(response_head), content, status, kResponseDefault);
  base::RunLoop().RunUntilIdle();

  return true;
}

bool TestURLLoaderFactory::SimulateResponseForPendingRequest(
    const std::string& url,
    const std::string& content,
    net::HttpStatusCode http_status,
    ResponseMatchFlags flags) {
  mojom::URLResponseHeadPtr head = CreateURLResponseHead(http_status);
  head->mime_type = "text/html";
  URLLoaderCompletionStatus status;
  status.decoded_body_length = content.size();
  return SimulateResponseForPendingRequest(GURL(url), status, std::move(head),
                                           content, flags);
}

void TestURLLoaderFactory::SimulateResponseWithoutRemovingFromPendingList(
    PendingRequest* request,
    mojom::URLResponseHeadPtr head,
    std::string content,
    const URLLoaderCompletionStatus& completion_status) {
  URLLoaderCompletionStatus status(completion_status);
  status.decoded_body_length = content.size();
  SimulateResponse(request->client.get(), TestURLLoaderFactory::Redirects(),
                   std::move(head), content, status, kResponseDefault);
  base::RunLoop().RunUntilIdle();
}

void TestURLLoaderFactory::SimulateResponseWithoutRemovingFromPendingList(
    PendingRequest* request,
    std::string content) {
  URLLoaderCompletionStatus completion_status(net::OK);
  mojom::URLResponseHeadPtr head = CreateURLResponseHead(net::HTTP_OK);
  SimulateResponseWithoutRemovingFromPendingList(request, std::move(head),
                                                 content, completion_status);
}

// static
void TestURLLoaderFactory::SimulateResponse(
    mojom::URLLoaderClient* client,
    TestURLLoaderFactory::Redirects redirects,
    mojom::URLResponseHeadPtr head,
    std::string content,
    URLLoaderCompletionStatus status,
    ResponseProduceFlags response_flags) {
  for (const auto& redirect : redirects)
    client->OnReceiveRedirect(redirect.first, redirect.second.Clone());

  if (response_flags & kResponseOnlyRedirectsNoDestination)
    return;

  if ((response_flags & kSendHeadersOnNetworkError) ||
      status.error_code == net::OK) {
    client->OnReceiveResponse(std::move(head));
  }

  if (status.error_code == net::OK) {
    mojo::DataPipe data_pipe(content.size());
    uint32_t bytes_written = content.size();
    CHECK_EQ(MOJO_RESULT_OK, data_pipe.producer_handle->WriteData(
                                 content.data(), &bytes_written,
                                 MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));
    client->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));
  }
  client->OnComplete(status);
}

}  // namespace network
