// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/sim/sim_request.h"

#include "third_party/blink/renderer/core/testing/sim/sim_network.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_client.h"
#include "third_party/blink/renderer/platform/loader/static_data_navigation_body_loader.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

SimRequestBase::SimRequestBase(KURL url,
                               String mime_type,
                               bool start_immediately,
                               Params params)
    : url_(std::move(url)),
      redirect_url_(params.redirect_url),
      mime_type_(std::move(mime_type)),
      referrer_(params.referrer),
      requestor_origin_(params.requestor_origin),
      start_immediately_(start_immediately),
      response_http_headers_(params.response_http_headers),
      response_http_status_(params.response_http_status) {
  SimNetwork::Current().AddRequest(*this);
}

SimRequestBase::~SimRequestBase() {
  DCHECK(!client_);
  DCHECK(!navigation_body_loader_);
}

void SimRequestBase::DidReceiveResponse(URLLoaderClient* client,
                                        const WebURLResponse& response) {
  DCHECK(!navigation_body_loader_);
  client_ = client;
  response_ = response;
  started_ = false;
  if (start_immediately_)
    StartInternal();
}

void SimRequestBase::DidFail(const WebURLError& error) {
  error_ = error;
}

void SimRequestBase::UsedForNavigation(
    StaticDataNavigationBodyLoader* navigation_body_loader) {
  DCHECK(start_immediately_);
  DCHECK(!client_);
  DCHECK(!started_);
  navigation_body_loader_ = navigation_body_loader;
  started_ = true;
}

void SimRequestBase::StartInternal() {
  DCHECK(!started_);
  DCHECK(redirect_url_.empty());  // client_ is nullptr on redirects
  DCHECK(client_);
  started_ = true;
  client_->DidReceiveResponse(response_,
                              /*body=*/mojo::ScopedDataPipeConsumerHandle(),
                              /*cached_metadata=*/std::nullopt);
}

void SimRequestBase::Write(const String& data) {
  WriteInternal(StringUTF8Adaptor(data));
}

void SimRequestBase::Write(const Vector<char>& data) {
  WriteInternal(data);
}

void SimRequestBase::WriteInternal(base::span<const char> data) {
  if (!started_)
    ServePending();
  DCHECK(started_);
  DCHECK(!error_);
  total_encoded_data_length_ += data.size();
  if (navigation_body_loader_) {
    navigation_body_loader_->Write(data);
  } else {
    client_->DidReceiveDataForTesting(data);
  }
}

void SimRequestBase::Finish(bool body_loader_finished) {
  if (!started_)
    ServePending();
  DCHECK(started_);
  if (error_) {
    DCHECK(!navigation_body_loader_);
    client_->DidFail(*error_, base::TimeTicks::Now(),
                     total_encoded_data_length_, total_encoded_data_length_,
                     total_encoded_data_length_);
  } else {
    if (navigation_body_loader_) {
      if (!body_loader_finished)
        navigation_body_loader_->Finish();
    } else {
      client_->DidFinishLoading(
          base::TimeTicks::Now(), total_encoded_data_length_,
          total_encoded_data_length_, total_encoded_data_length_);
    }
  }
  Reset();
}

void SimRequestBase::Complete(const String& data) {
  if (!started_)
    ServePending();
  if (!started_)
    StartInternal();
  if (!data.empty())
    Write(data);
  Finish();
}

void SimRequestBase::Complete(const Vector<char>& data) {
  if (!started_)
    ServePending();
  if (!started_)
    StartInternal();
  if (!data.empty())
    Write(data);
  Finish();
}

void SimRequestBase::Reset() {
  started_ = false;
  client_ = nullptr;
  navigation_body_loader_ = nullptr;
  SimNetwork::Current().RemoveRequest(*this);
}

void SimRequestBase::ServePending() {
  SimNetwork::Current().ServePendingRequests();
}

SimRequest::SimRequest(KURL url, String mime_type, Params params)
    : SimRequestBase(std::move(url),
                     std::move(mime_type),
                     /* start_immediately=*/true,
                     params) {}

SimRequest::SimRequest(String url, String mime_type, Params params)
    : SimRequest(KURL(url), std::move(mime_type), params) {}

SimRequest::~SimRequest() = default;

SimSubresourceRequest::SimSubresourceRequest(KURL url,
                                             String mime_type,
                                             Params params)
    : SimRequestBase(std::move(url),
                     std::move(mime_type),
                     /* start_immediately=*/false,
                     params) {}

SimSubresourceRequest::SimSubresourceRequest(String url,
                                             String mime_type,
                                             Params params)
    : SimSubresourceRequest(KURL(url), std::move(mime_type), params) {}

SimSubresourceRequest::~SimSubresourceRequest() = default;

void SimSubresourceRequest::Start() {
  ServePending();
  StartInternal();
}

}  // namespace blink
