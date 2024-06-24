// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_url_loader_factory.h"

#include <string_view>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/test_future.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_utils.h"

namespace network {

TestURLLoaderFactory::TestURLLoader::FollowRedirectParams::
    FollowRedirectParams() = default;
TestURLLoaderFactory::TestURLLoader::FollowRedirectParams::
    ~FollowRedirectParams() = default;

TestURLLoaderFactory::TestURLLoader::FollowRedirectParams::FollowRedirectParams(
    FollowRedirectParams&& other) = default;

TestURLLoaderFactory::TestURLLoader::FollowRedirectParams&
TestURLLoaderFactory::TestURLLoader::FollowRedirectParams::operator=(
    FollowRedirectParams&& other) = default;

TestURLLoaderFactory::TestURLLoader::TestURLLoader(
    mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver)
    : receiver_(this, std::move(url_loader_receiver)) {}

TestURLLoaderFactory::TestURLLoader::~TestURLLoader() = default;

void TestURLLoaderFactory::TestURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
  FollowRedirectParams params;
  params.removed_headers = removed_headers;
  params.modified_headers = modified_headers;
  params.modified_cors_exempt_headers = modified_cors_exempt_headers;
  params.new_url = new_url;

  follow_redirect_params_.emplace_back(std::move(params));
}

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

TestURLLoaderFactory::TestURLLoaderFactory(bool observe_loader_requests)
    : weak_wrapper_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              this)),
      observe_loader_requests_(observe_loader_requests) {}

TestURLLoaderFactory::~TestURLLoaderFactory() {
  weak_wrapper_->Detach();
}

void TestURLLoaderFactory::AddResponse(const GURL& url,
                                       mojom::URLResponseHeadPtr head,
                                       std::string_view content,
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

void TestURLLoaderFactory::AddResponse(std::string_view url,
                                       std::string_view content,
                                       net::HttpStatusCode http_status) {
  mojom::URLResponseHeadPtr head = CreateURLResponseHead(http_status);
  head->mime_type = "text/html";
  URLLoaderCompletionStatus status;
  AddResponse(GURL(url), std::move(head), content, status);
}

bool TestURLLoaderFactory::IsPending(std::string_view url,
                                     const ResourceRequest** request_out) {
  base::RunLoop().RunUntilIdle();
  for (const auto& candidate : pending_requests_) {
    if (candidate.request.url == url) {
      if (request_out)
        *request_out = &candidate.request;
      if (candidate.client.is_connected())
        return true;
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
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& url_request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  ++total_requests_;

  if (interceptor_)
    interceptor_.Run(url_request);

  mojo::Remote<mojom::URLLoaderClient> client_remote(std::move(client));
  if (CreateLoaderAndStartInternal(url_request.url, client_remote.get()))
    return;

  PendingRequest pending_request;

  if (observe_loader_requests_) {
    pending_request.test_url_loader =
        std::make_unique<TestURLLoader>(std::move(receiver));
  }

  pending_request.client = std::move(client_remote);
  pending_request.request_id = request_id;
  pending_request.options = options;
  pending_request.request = url_request;
  pending_request.traffic_annotation = traffic_annotation;
  pending_requests_.push_back(std::move(pending_request));
  if (on_new_pending_request_) {
    std::move(on_new_pending_request_).Run();
  }
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
    redirects.emplace_back(redirect.first, redirect.second.Clone());
  }
  SimulateResponse(client, std::move(redirects), it->second.head.Clone(),
                   it->second.content, it->second.status, it->second.flags);
  return true;
}

std::optional<network::TestURLLoaderFactory::PendingRequest>
TestURLLoaderFactory::FindPendingRequest(const GURL& url,
                                         ResponseMatchFlags flags) {
  const bool url_match_prefix = flags & kUrlMatchPrefix;
  const bool reverse = flags & kMostRecentMatch;
  const bool wait_for_request = flags & kWaitForRequest;

  // Give any cancellations a chance to happen...
  base::RunLoop().RunUntilIdle();

  network::TestURLLoaderFactory::PendingRequest request;
  while (true) {
    bool found_request = false;
    for (int i = (reverse ? static_cast<int>(pending_requests_.size()) - 1 : 0);
         reverse ? i >= 0 : i < static_cast<int>(pending_requests_.size());
         reverse ? --i : ++i) {
      // Skip already cancelled.
      if (!pending_requests_[i].client.is_connected()) {
        continue;
      }

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
    if (found_request) {
      return request;
    }
    if (wait_for_request) {
      base::test::TestFuture<void> future;
      on_new_pending_request_ = future.GetCallback();
      if (!future.Wait()) {
        // Timed out.
        return std::nullopt;
      }
    } else {
      return std::nullopt;
    }
  }
}

bool TestURLLoaderFactory::SimulateResponseForPendingRequest(
    const GURL& url,
    const network::URLLoaderCompletionStatus& completion_status,
    mojom::URLResponseHeadPtr response_head,
    std::string_view content,
    ResponseMatchFlags flags) {
  auto request = FindPendingRequest(url, flags);
  if (!request) {
    return false;
  }

  // |decoded_body_length| must be set to the right size or the SimpleURLLoader
  // will fail.
  network::URLLoaderCompletionStatus status(completion_status);
  status.decoded_body_length = content.size();

  SimulateResponse(request->client.get(), TestURLLoaderFactory::Redirects(),
                   std::move(response_head), content, status, kResponseDefault);
  // Attempt to wait for the response to be handled. If any part of the handling
  // is queued elsewhere (for example on another thread) this may return before
  // it is finished.
  base::RunLoop().RunUntilIdle();

  return true;
}

bool TestURLLoaderFactory::SimulateResponseForPendingRequest(
    std::string_view url,
    std::string_view content,
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
    std::string_view content,
    const URLLoaderCompletionStatus& completion_status) {
  URLLoaderCompletionStatus status(completion_status);
  status.decoded_body_length = content.size();
  SimulateResponse(request->client.get(), TestURLLoaderFactory::Redirects(),
                   std::move(head), content, status, kResponseDefault);
  // Attempt to wait for the response to be handled. If any part of the handling
  // is queued elsewhere (for example on another thread) this may return before
  // it is finished.
  base::RunLoop().RunUntilIdle();
}

void TestURLLoaderFactory::SimulateResponseWithoutRemovingFromPendingList(
    PendingRequest* request,
    std::string_view content) {
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
    std::string_view content,
    URLLoaderCompletionStatus status,
    ResponseProduceFlags response_flags) {
  for (const auto& redirect : redirects)
    client->OnReceiveRedirect(redirect.first, redirect.second.Clone());

  if (response_flags & kResponseOnlyRedirectsNoDestination)
    return;

  mojo::ScopedDataPipeConsumerHandle body;

  if (status.error_code == net::OK) {
    mojo::ScopedDataPipeProducerHandle producer_handle;
    CHECK_EQ(mojo::CreateDataPipe(content.size(), producer_handle, body),
             MOJO_RESULT_OK);
    CHECK_EQ(MOJO_RESULT_OK,
             producer_handle->WriteAllData(base::as_byte_span(content)));
  }

  if ((response_flags & kSendHeadersOnNetworkError) ||
      status.error_code == net::OK) {
    client->OnReceiveResponse(std::move(head), std::move(body), std::nullopt);
  }

  client->OnComplete(status);
}

}  // namespace network
