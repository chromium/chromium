// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/corp_message_channel_strategy.h"

#include <utility>
#include <variant>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "remoting/base/http_status.h"
#include "remoting/base/internal_headers.h"
#include "remoting/base/scoped_protobuf_http_request.h"
#include "remoting/proto/messaging_service.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

using remoting::internal::ChannelActiveStruct;
using remoting::internal::ChannelOpenStruct;
using remoting::internal::HostOpenChannelResponseStruct;
using remoting::internal::PeerMessageStruct;

namespace remoting {

CorpMessageChannelStrategy::CorpMessageChannelStrategy() = default;
CorpMessageChannelStrategy::~CorpMessageChannelStrategy() = default;

void CorpMessageChannelStrategy::Initialize(
    const StreamOpener& stream_opener,
    const MessageCallback& on_incoming_msg) {
  DCHECK(stream_opener);
  DCHECK(on_incoming_msg);
  stream_opener_ = stream_opener;
  on_incoming_msg_ = on_incoming_msg;
  // TODO: joedow - The current message channel impl expects to have an
  // inactivity timeout available as soon as the channel is created because that
  // is how FTL messaging works. Since the Corp inactivity timeout is provided
  // by the server, we'll need to update the MessageChannel class to handle that
  // case. For now, set a generous initial timeout which will be overwritten
  // when the channel open message is received.
  inactivity_timeout_ = base::Seconds(60);
}

void CorpMessageChannelStrategy::OnReceiveMessagesResponse(
    std::unique_ptr<HostOpenChannelResponseStruct> response) {
  std::visit(absl::Overload(
                 [this](const ChannelOpenStruct& channel_open_message) {
                   VLOG(0) << "Received channel open";
                   inactivity_timeout_ =
                       channel_open_message.inactivity_timeout;
                 },
                 [this](const ChannelActiveStruct& channel_active_message) {
                   VLOG(0) << "Received channel active";
                   on_channel_active_.Run();
                 },
                 [this](const PeerMessageStruct& peer_message) {
                   VLOG(0) << "Received peer message";
                   on_incoming_msg_.Run(peer_message);
                 }),
             response->message);
}

std::unique_ptr<ScopedProtobufHttpRequest>
CorpMessageChannelStrategy::CreateChannel(
    base::OnceClosure on_channel_ready,
    ChannelClosedCallback on_channel_closed) {
  return stream_opener_.Run(
      std::move(on_channel_ready),
      base::BindRepeating(
          &CorpMessageChannelStrategy::OnReceiveMessagesResponse,
          weak_factory_.GetWeakPtr()),
      std::move(on_channel_closed));
}

base::TimeDelta CorpMessageChannelStrategy::GetInactivityTimeout() const {
  return *inactivity_timeout_;
}

void CorpMessageChannelStrategy::set_on_channel_active(
    base::RepeatingClosure on_channel_active) {
  on_channel_active_ = std::move(on_channel_active);
}

}  // namespace remoting
