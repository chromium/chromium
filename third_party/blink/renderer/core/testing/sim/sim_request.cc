// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/sim/sim_request.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/renderer/core/testing/sim/sim_network.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

SimRequest::SimRequest(String url, String mime_type)
    : url_(url),
      client_(nullptr),
      total_encoded_data_length_(0),
      is_ready_(false) {
  KURL full_url(url);
  WebURLResponse response(full_url);
  response.SetMIMEType(mime_type);
  response.SetHTTPStatusCode(200);
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(full_url,
                                                              response, "");
  SimNetwork::Current().AddRequest(*this);
}

SimRequest::~SimRequest() {
  DCHECK(!is_ready_);
}

void SimRequest::DidReceiveResponse(WebURLLoaderClient* client,
                                    const WebURLResponse& response) {
  client_ = client;
  response_ = response;
  is_ready_ = true;
}

void SimRequest::DidFail(const WebURLError& error) {
  error_ = error;
}

void SimRequest::Start() {
  SimNetwork::Current().ServePendingRequests();
  DCHECK(is_ready_);
  client_->DidReceiveResponse(response_);
}

void SimRequest::Write(const String& data) {
  DCHECK(is_ready_);
  DCHECK(!error_);
  total_encoded_data_length_ += data.length();
  client_->DidReceiveData(data.Utf8().data(), data.length());
}

void SimRequest::Write(const Vector<char>& data) {
  DCHECK(is_ready_);
  DCHECK(!error_);
  total_encoded_data_length_ += data.size();
  client_->DidReceiveData(data.data(), data.size());
}

void SimRequest::Finish() {
  DCHECK(is_ready_);
  if (error_) {
    client_->DidFail(*error_, total_encoded_data_length_,
                     total_encoded_data_length_, total_encoded_data_length_);
  } else {
    // TODO(esprehn): Is claiming a request time of 0 okay for tests?
    client_->DidFinishLoading(
        TimeTicks(), total_encoded_data_length_, total_encoded_data_length_,
        total_encoded_data_length_, false,
        std::vector<network::cors::PreflightTimingInfo>());
  }
  Reset();
}

void SimRequest::Complete(const String& data) {
  Start();
  if (!data.IsEmpty())
    Write(data);
  Finish();
}

void SimRequest::Complete(const Vector<char>& data) {
  Start();
  if (!data.IsEmpty())
    Write(data);
  Finish();
}

void SimRequest::Reset() {
  is_ready_ = false;
  client_ = nullptr;
  Platform::Current()->GetURLLoaderMockFactory()->UnregisterURL(KURL(url_));

  SimNetwork::Current().RemoveRequest(*this);
}

}  // namespace blink
