// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/sim/sim_request.h"

#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/renderer/core/testing/sim/sim_network.h"
#include "third_party/blink/renderer/platform/loader/static_data_navigation_body_loader.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

SimRequestBase::SimRequestBase(String url,
                               String mime_type,
                               bool start_immediately,
                               Params params)
    : url_(url),
      redirect_url_(params.redirect_url),
      mime_type_(mime_type),
      start_immediately_(start_immediately),
      started_(false),
      client_(nullptr),
      total_encoded_data_length_(0),
      response_http_headers_(params.response_http_headers),
      response_http_status_(params.response_http_status) {
  SimNetwork::Current().AddRequest(*this);
}

SimRequestBase::~SimRequestBase() {
  DCHECK(!client_);
  DCHECK(!navigation_body_loader_);
}

void SimRequestBase::DidReceiveResponse(WebURLLoaderClient* client,
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
  DCHECK(redirect_url_.IsEmpty());  // client_ is nullptr on redirects
  DCHECK(client_);
  started_ = true;
  client_->DidReceiveResponse(response_);
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
  if (navigation_body_loader_)
    navigation_body_loader_->Write(data.data(), data.size());
  else
    client_->DidReceiveData(data.data(), data.size());
}

void SimRequestBase::Finish() {
  if (!started_)
    ServePending();
  DCHECK(started_);
  if (error_) {
    DCHECK(!navigation_body_loader_);
    client_->DidFail(*error_, total_encoded_data_length_,
                     total_encoded_data_length_, total_encoded_data_length_);
  } else {
    if (navigation_body_loader_) {
      navigation_body_loader_->Finish();
    } else {
      // TODO(esprehn): Is claiming a request time of 0 okay for tests?
      client_->DidFinishLoading(base::TimeTicks(), total_encoded_data_length_,
                                total_encoded_data_length_,
                                total_encoded_data_length_, false);
    }
  }
  Reset();
}

void SimRequestBase::Complete(const String& data) {
  if (!started_)
    ServePending();
  if (!started_)
    StartInternal();
  if (!data.IsEmpty())
    Write(data);
  Finish();
}

void SimRequestBase::Complete(const Vector<char>& data) {
  if (!started_)
    ServePending();
  if (!started_)
    StartInternal();
  if (!data.IsEmpty())
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

SimRequest::SimRequest(String url, String mime_type, Params params)
    : SimRequestBase(url, mime_type, true /* start_immediately */, params) {}

SimRequest::~SimRequest() = default;

SimSubresourceRequest::SimSubresourceRequest(String url,
                                             String mime_type,
                                             Params params)
    : SimRequestBase(url, mime_type, false /* start_immediately */, params) {}

SimSubresourceRequest::~SimSubresourceRequest() = default;

void SimSubresourceRequest::Start() {
  ServePending();
  StartInternal();
}

}  // namespace blink
