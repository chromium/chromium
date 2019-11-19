// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_url_request.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ppapi/cpp/url_response_info.h"

// Read buffer we allocate per read when reading response from
// URLLoader.
static const int kReadSize = 1024;

namespace remoting {

PepperUrlRequest::PepperUrlRequest(
    pp::InstanceHandle pp_instance,
    UrlRequest::Type type,
    const std::string& url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : request_info_(pp_instance),
      url_loader_(pp_instance),
      url_(url),
      callback_factory_(this) {
  request_info_.SetURL(url);
  switch (type) {
    case Type::GET:
      request_info_.SetMethod("GET");
      break;
    case Type::POST:
      request_info_.SetMethod("POST");
      break;
  }
}

PepperUrlRequest::~PepperUrlRequest() {}

void PepperUrlRequest::AddHeader(const std::string& value) {
  headers_ += value + "\n\r";
}

void PepperUrlRequest::SetPostData(const std::string& content_type,
                                   const std::string& data) {
  AddHeader("Content-Type: " + content_type);
  request_info_.AppendDataToBody(data.data(), data.length());
}

void PepperUrlRequest::Start(const OnResultCallback& on_result_callback) {
  on_result_callback_ = on_result_callback;

  request_info_.SetHeaders(headers_);
  int result = url_loader_.Open(
      request_info_,
      callback_factory_.NewCallback(&PepperUrlRequest::OnUrlOpened));
  DCHECK_EQ(result, PP_OK_COMPLETIONPENDING);
}

void PepperUrlRequest::OnUrlOpened(int32_t result) {
  if (result == PP_ERROR_ABORTED) {
    return;
  }

  if (result < 0) {
    LOG(WARNING) << "pp::URLLoader for " << url_ << " failed: " << result;
    std::move(on_result_callback_).Run(Result::Failed());
    return;
  }

  ReadResponseBody();
}

void PepperUrlRequest::ReadResponseBody() {
  int pos = response_.size();
  response_.resize(pos + kReadSize);
  int result = url_loader_.ReadResponseBody(
      &response_[pos], kReadSize,
      callback_factory_.NewCallback(&PepperUrlRequest::OnResponseBodyRead));
  DCHECK_EQ(result, PP_OK_COMPLETIONPENDING);
}

void PepperUrlRequest::OnResponseBodyRead(int32_t result) {
  if (result == PP_ERROR_ABORTED)
    return;

  if (result < 0) {
    LOG(WARNING) << "Failed to read HTTP response body when fetching "
                 << url_ << ", error: " << result;
    std::move(on_result_callback_).Run(Result::Failed());
    return;
  }

  // Resize the buffer in case we've read less than was requested.
  CHECK_LE(result, kReadSize);
  CHECK_GE(static_cast<int>(response_.size()), kReadSize);
  response_.resize(response_.size() - kReadSize + result);

  // Try to read again if there is more data to read.
  if (result > 0) {
    ReadResponseBody();
    return;
  }

  std::move(on_result_callback_)
      .Run(Result(url_loader_.GetResponseInfo().GetStatusCode(), response_));
}

PepperUrlRequestFactory::PepperUrlRequestFactory(pp::InstanceHandle pp_instance)
    : pp_instance_(pp_instance) {}
PepperUrlRequestFactory::~PepperUrlRequestFactory() {}

std::unique_ptr<UrlRequest> PepperUrlRequestFactory::CreateUrlRequest(
    UrlRequest::Type type,
    const std::string& url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  return std::make_unique<PepperUrlRequest>(pp_instance_, type, url,
                                            traffic_annotation);
}

}  // namespace remoting
