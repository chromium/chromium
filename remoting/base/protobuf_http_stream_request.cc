// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/protobuf_http_stream_request.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/protobuf_http_stream_parser.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace remoting {

// static
constexpr base::TimeDelta
    ProtobufHttpStreamRequest::kStreamReadyTimeoutDuration;

ProtobufHttpStreamRequest::ProtobufHttpStreamRequest(
    std::unique_ptr<ProtobufHttpRequestConfig> config)
    : ProtobufHttpRequestBase(std::move(config)) {}

ProtobufHttpStreamRequest::~ProtobufHttpStreamRequest() = default;

void ProtobufHttpStreamRequest::SetStreamReadyCallback(
    base::OnceClosure callback) {
  stream_ready_callback_ = std::move(callback);
}

void ProtobufHttpStreamRequest::SetStreamClosedCallback(
    StreamClosedCallback callback) {
  stream_closed_callback_ = std::move(callback);
}

void ProtobufHttpStreamRequest::OnMessage(const std::string& message) {
  std::unique_ptr<google::protobuf::MessageLite> protobuf_message(
      default_message_->New());
  if (protobuf_message->ParseFromString(message)) {
    message_callback_.Run(std::move(protobuf_message));
  } else {
    LOG(ERROR) << "Failed to parse a stream message.";
  }
}

void ProtobufHttpStreamRequest::OnStreamClosed(
    const ProtobufHttpStatus& status) {
  DCHECK(stream_closed_callback_);
  DCHECK(invalidator_);

  // Move |invalidator_| out of |this| as the callback can potentially delete
  // |this|.
  auto invalidator = std::move(invalidator_);
  std::move(stream_closed_callback_).Run(status);
  std::move(invalidator).Run();
}

void ProtobufHttpStreamRequest::OnAuthFailed(const ProtobufHttpStatus& status) {
  // Can't call OnStreamClosed here since it invokes the |invalidator_|.
  std::move(stream_closed_callback_).Run(status);
}

void ProtobufHttpStreamRequest::StartRequestInternal(
    network::mojom::URLLoaderFactory* loader_factory) {
  DCHECK(default_message_);
  DCHECK(stream_ready_callback_);
  DCHECK(stream_closed_callback_);
  DCHECK(message_callback_);
  DCHECK(!stream_ready_timeout_timer_.IsRunning());

  stream_ready_timeout_timer_.Start(
      FROM_HERE, kStreamReadyTimeoutDuration, this,
      &ProtobufHttpStreamRequest::OnStreamReadyTimeout);

  // Safe to use unretained, as callbacks won't be called after |stream_parser_|
  // is deleted.
  stream_parser_ = std::make_unique<ProtobufHttpStreamParser>(
      base::BindRepeating(&ProtobufHttpStreamRequest::OnMessage,
                          base::Unretained(this)),
      base::BindRepeating(&ProtobufHttpStreamRequest::OnStreamClosed,
                          base::Unretained(this)));
  url_loader_->DownloadAsStream(loader_factory, this);
}

base::TimeDelta ProtobufHttpStreamRequest::GetRequestTimeoutDuration() const {
  return base::TimeDelta();
}

void ProtobufHttpStreamRequest::OnDataReceived(std::string_view string_view,
                                               base::OnceClosure resume) {
  if (stream_ready_timeout_timer_.IsRunning()) {
    stream_ready_timeout_timer_.Stop();
  }

  if (stream_ready_callback_) {
    std::move(stream_ready_callback_).Run();
  }

  DCHECK(stream_parser_);
  stream_parser_->Append(string_view);
  std::move(resume).Run();
}

void ProtobufHttpStreamRequest::OnComplete(bool success) {
  // |success| can be true even if the server returns 4xx or 5xx error.
  OnStreamClosed(GetUrlLoaderStatus());
}

void ProtobufHttpStreamRequest::OnRetry(base::OnceClosure start_retry) {
  NOTIMPLEMENTED();
}

void ProtobufHttpStreamRequest::OnStreamReadyTimeout() {
  OnStreamClosed(ProtobufHttpStatus(ProtobufHttpStatus::Code::DEADLINE_EXCEEDED,
                                    "Stream connection failed: timeout"));
}

}  // namespace remoting
