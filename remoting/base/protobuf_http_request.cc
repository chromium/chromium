// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/protobuf_http_request.h"

#include "remoting/base/protobuf_http_client_messages.pb.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace remoting {

namespace {
constexpr int kMaxResponseSizeBytes = 512 * 1024;  // 512 KB
}  // namespace

ProtobufHttpRequest::ProtobufHttpRequest(
    std::unique_ptr<ProtobufHttpRequestConfig> config)
    : ProtobufHttpRequestBase(std::move(config)) {}

ProtobufHttpRequest::~ProtobufHttpRequest() = default;

void ProtobufHttpRequest::SetTimeoutDuration(base::TimeDelta timeout_duration) {
  timeout_duration_ = timeout_duration;
}

void ProtobufHttpRequest::OnAuthFailed(const ProtobufHttpStatus& status) {
  RunResponseCallback(status);
}

void ProtobufHttpRequest::StartRequestInternal(
    network::mojom::URLLoaderFactory* loader_factory) {
  DCHECK(response_callback_);

  // Safe to use unretained as callback will not be called once |url_loader_| is
  // deleted.
  url_loader_->DownloadToString(
      loader_factory,
      base::BindOnce(&ProtobufHttpRequest::OnResponse, base::Unretained(this)),
      kMaxResponseSizeBytes);
}

base::TimeDelta ProtobufHttpRequest::GetRequestTimeoutDuration() const {
  return timeout_duration_;
}

void ProtobufHttpRequest::OnResponse(
    std::unique_ptr<std::string> response_body) {
  ProtobufHttpStatus url_loader_status = GetUrlLoaderStatus();
  // Move variables out of |this| as the callback can potentially delete |this|.
  auto invalidator = std::move(invalidator_);

  if (url_loader_status.ok()) {
    RunResponseCallback(ParseResponse(std::move(response_body)));
  } else {
    // Parse the status from the response.
    protobufhttpclient::Status api_status;
    if (response_body && api_status.ParseFromString(*response_body) &&
        api_status.code() > 0) {
      RunResponseCallback(ProtobufHttpStatus(api_status, *response_body));
    } else {
      // Fallback to just return the status from URL loader.
      RunResponseCallback(url_loader_status);
    }
  }
  // NOTE: Don't access member variables here, since |this| might have been
  // deleted by the callback.
  std::move(invalidator).Run();
}

ProtobufHttpStatus ProtobufHttpRequest::ParseResponse(
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    LOG(ERROR) << "Server returned no response body";
    return ProtobufHttpStatus(net::ERR_EMPTY_RESPONSE);
  }
  if (!response_message_->ParseFromString(*response_body)) {
    LOG(ERROR) << "Failed to parse response body";
    return ProtobufHttpStatus(net::ERR_INVALID_RESPONSE);
  }
  return ProtobufHttpStatus::OK();
}

void ProtobufHttpRequest::RunResponseCallback(
    const ProtobufHttpStatus& status) {
  // Drop unowned reference before invoking callback which destroys it.
  response_message_ = nullptr;
  std::move(response_callback_).Run(status);
}

}  // namespace remoting
