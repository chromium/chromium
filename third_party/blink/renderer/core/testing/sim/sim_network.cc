// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/sim/sim_network.h"

#include <memory>
#include <utility>
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_client.h"
#include "third_party/blink/renderer/platform/loader/static_data_navigation_body_loader.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

static SimNetwork* g_network = nullptr;

SimNetwork::SimNetwork() : current_request_(nullptr) {
  url_test_helpers::SetLoaderDelegate(this);
  DCHECK(!g_network);
  g_network = this;
}

SimNetwork::~SimNetwork() {
  url_test_helpers::SetLoaderDelegate(nullptr);
  // TODO(crbug.com/751425): We should use the mock functionality
  // via |SimTest::web_frame_client_| and/or |SimTest::web_view_helper_|.
  url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  g_network = nullptr;
}

SimNetwork& SimNetwork::Current() {
  DCHECK(g_network);
  return *g_network;
}

void SimNetwork::ServePendingRequests() {
  // TODO(crbug.com/751425): We should use the mock functionality
  // via |SimTest::web_frame_client_| and/or |SimTest::web_view_helper_|.
  url_test_helpers::ServeAsynchronousRequests();
}

void SimNetwork::DidReceiveResponse(URLLoaderClient* client,
                                    const WebURLResponse& response) {
  auto it = requests_.find(response.CurrentRequestUrl().GetString());
  if (it == requests_.end()) {
    client->DidReceiveResponse(response,
                               /*body=*/mojo::ScopedDataPipeConsumerHandle(),
                               /*cached_metadata=*/std::nullopt);
    return;
  }
  DCHECK(it->value);
  current_request_ = it->value;
  current_request_->DidReceiveResponse(client, response);
}

void SimNetwork::DidReceiveData(URLLoaderClient* client,
                                base::span<const char> data) {
  if (!current_request_)
    client->DidReceiveDataForTesting(data);
}

void SimNetwork::DidFail(URLLoaderClient* client,
                         const WebURLError& error,
                         int64_t total_encoded_data_length,
                         int64_t total_encoded_body_length,
                         int64_t total_decoded_body_length) {
  if (!current_request_) {
    client->DidFail(error, base::TimeTicks::Now(), total_encoded_data_length,
                    total_encoded_body_length, total_decoded_body_length);
    return;
  }
  current_request_->DidFail(error);
}

void SimNetwork::DidFinishLoading(URLLoaderClient* client,
                                  base::TimeTicks finish_time,
                                  int64_t total_encoded_data_length,
                                  int64_t total_encoded_body_length,
                                  int64_t total_decoded_body_length) {
  if (!current_request_) {
    client->DidFinishLoading(finish_time, total_encoded_data_length,
                             total_encoded_body_length,
                             total_decoded_body_length);
    return;
  }
  current_request_ = nullptr;
}

void SimNetwork::AddRequest(SimRequestBase& request) {
  DCHECK(!requests_.Contains(request.url_.GetString()));
  requests_.insert(request.url_.GetString(), &request);
  WebURLResponse response(request.url_);
  response.SetMimeType(request.mime_type_);
  response.AddHttpHeaderField("Content-Type", request.mime_type_);

  if (request.redirect_url_.empty()) {
    response.SetHttpStatusCode(request.response_http_status_);
  } else {
    response.SetHttpStatusCode(302);
    response.AddHttpHeaderField("Location", request.redirect_url_);
  }

  for (const auto& http_header : request.response_http_headers_)
    response.AddHttpHeaderField(http_header.key, http_header.value);

  // TODO(crbug.com/751425): We should use the mock functionality
  // via |SimTest::web_frame_client_| and/or |SimTest::web_view_helper_|.
  url_test_helpers::RegisterMockedURLLoadWithCustomResponse(request.url_, "",
                                                            response);
}

void SimNetwork::RemoveRequest(SimRequestBase& request) {
  requests_.erase(request.url_);
  // TODO(crbug.com/751425): We should use the mock functionality
  // via |SimTest::web_frame_client_| and/or |SimTest::web_view_helper_|.
  url_test_helpers::RegisterMockedURLUnregister(request.url_);
}

bool SimNetwork::FillNavigationParamsResponse(WebNavigationParams* params) {
  auto it = requests_.find(params->url.GetString());
  SimRequestBase* request = it->value;
  params->response = WebURLResponse(params->url);
  params->response.SetMimeType(request->mime_type_);
  params->response.AddHttpHeaderField("Content-Type", request->mime_type_);
  params->response.SetHttpStatusCode(request->response_http_status_);
  for (const auto& http_header : request->response_http_headers_)
    params->response.AddHttpHeaderField(http_header.key, http_header.value);

  auto body_loader = std::make_unique<StaticDataNavigationBodyLoader>();
  request->UsedForNavigation(body_loader.get());
  params->body_loader = std::move(body_loader);
  params->referrer = request->referrer_;
  params->requestor_origin = request->requestor_origin_;
  return true;
}

}  // namespace blink
