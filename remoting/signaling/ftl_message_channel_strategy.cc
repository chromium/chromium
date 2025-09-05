// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_message_channel_strategy.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "remoting/base/http_status.h"
#include "remoting/base/scoped_protobuf_http_request.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"

namespace remoting {

namespace {
constexpr base::TimeDelta kPongTimeout = base::Seconds(15);
}  // namespace

FtlMessageChannelStrategy::FtlMessageChannelStrategy() = default;
FtlMessageChannelStrategy::~FtlMessageChannelStrategy() = default;

void FtlMessageChannelStrategy::Initialize(
    const StreamOpener& stream_opener,
    const MessageCallback& on_incoming_msg) {
  DCHECK(stream_opener);
  DCHECK(on_incoming_msg);
  stream_opener_ = stream_opener;
  on_incoming_msg_ = on_incoming_msg;
}

void FtlMessageChannelStrategy::OnReceiveMessagesResponse(
    std::unique_ptr<ftl::ReceiveMessagesResponse> response) {
  switch (response->body_case()) {
    case ftl::ReceiveMessagesResponse::BodyCase::kInboxMessage: {
      VLOG(1) << "Received message";
      on_incoming_msg_.Run(response->inbox_message());
      break;
    }
    case ftl::ReceiveMessagesResponse::BodyCase::kPong:
      VLOG(1) << "Received pong";
      on_channel_active_.Run();
      break;
    case ftl::ReceiveMessagesResponse::BodyCase::kStartOfBatch:
      VLOG(1) << "Received start of batch";
      break;
    case ftl::ReceiveMessagesResponse::BodyCase::kEndOfBatch:
      VLOG(1) << "Received end of batch";
      break;
    default:
      LOG(WARNING) << "Received unknown message type: "
                   << response->body_case();
      break;
  }
}

std::unique_ptr<ScopedProtobufHttpRequest>
FtlMessageChannelStrategy::CreateChannel(
    base::OnceClosure on_channel_ready,
    ChannelClosedCallback on_channel_closed) {
  return stream_opener_.Run(
      std::move(on_channel_ready),
      base::BindRepeating(&FtlMessageChannelStrategy::OnReceiveMessagesResponse,
                          weak_factory_.GetWeakPtr()),
      std::move(on_channel_closed));
}

base::TimeDelta FtlMessageChannelStrategy::GetInactivityTimeout() const {
  return kPongTimeout;
}

void FtlMessageChannelStrategy::set_on_channel_active(
    base::RepeatingClosure on_channel_active) {
  on_channel_active_ = std::move(on_channel_active);
}

}  // namespace remoting
