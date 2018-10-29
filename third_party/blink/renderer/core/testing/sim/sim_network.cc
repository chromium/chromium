// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/sim/sim_network.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"

namespace blink {

static SimNetwork* g_network = nullptr;

SimNetwork::SimNetwork() : current_request_(nullptr) {
  Platform::Current()->GetURLLoaderMockFactory()->SetLoaderDelegate(this);
  DCHECK(!g_network);
  g_network = this;
}

SimNetwork::~SimNetwork() {
  Platform::Current()->GetURLLoaderMockFactory()->SetLoaderDelegate(nullptr);
  Platform::Current()
      ->GetURLLoaderMockFactory()
      ->UnregisterAllURLsAndClearMemoryCache();
  g_network = nullptr;
}

SimNetwork& SimNetwork::Current() {
  DCHECK(g_network);
  return *g_network;
}

void SimNetwork::ServePendingRequests() {
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
}

void SimNetwork::DidReceiveResponse(WebURLLoaderClient* client,
                                    const WebURLResponse& response) {
  auto it = requests_.find(response.Url().GetString());
  if (it == requests_.end()) {
    client->DidReceiveResponse(response);
    return;
  }
  DCHECK(it->value);
  current_request_ = it->value;
  current_request_->DidReceiveResponse(client, response);
}

void SimNetwork::DidReceiveData(WebURLLoaderClient* client,
                                const char* data,
                                int data_length) {
  if (!current_request_)
    client->DidReceiveData(data, data_length);
}

void SimNetwork::DidFail(WebURLLoaderClient* client,
                         const WebURLError& error,
                         int64_t total_encoded_data_length,
                         int64_t total_encoded_body_length,
                         int64_t total_decoded_body_length) {
  if (!current_request_) {
    client->DidFail(error, total_encoded_data_length, total_encoded_body_length,
                    total_decoded_body_length);
    return;
  }
  current_request_->DidFail(error);
}

void SimNetwork::DidFinishLoading(WebURLLoaderClient* client,
                                  TimeTicks finish_time,
                                  int64_t total_encoded_data_length,
                                  int64_t total_encoded_body_length,
                                  int64_t total_decoded_body_length) {
  if (!current_request_) {
    client->DidFinishLoading(finish_time, total_encoded_data_length,
                             total_encoded_body_length,
                             total_decoded_body_length, false,
                             std::vector<network::cors::PreflightTimingInfo>());
    return;
  }
  current_request_ = nullptr;
}

void SimNetwork::AddRequest(SimRequest& request) {
  requests_.insert(request.Url(), &request);
}

void SimNetwork::RemoveRequest(SimRequest& request) {
  requests_.erase(request.Url());
}

}  // namespace blink
