// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_CORP_MESSAGE_CHANNEL_STRATEGY_H_
#define REMOTING_SIGNALING_CORP_MESSAGE_CHANNEL_STRATEGY_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "remoting/signaling/message_channel_strategy.h"

namespace remoting {

class ScopedProtobufHttpRequest;

namespace internal {
struct HostOpenChannelResponseStruct;
struct PeerMessageStruct;
}  // namespace internal

class CorpMessageChannelStrategy : public MessageChannelStrategy {
 public:
  using MessageReceivedCallback = base::RepeatingCallback<void(
      std::unique_ptr<internal::HostOpenChannelResponseStruct>)>;
  using StreamOpener =
      base::RepeatingCallback<std::unique_ptr<ScopedProtobufHttpRequest>(
          base::OnceClosure on_channel_ready,
          const MessageReceivedCallback& on_incoming_msg,
          ChannelClosedCallback on_channel_closed)>;
  using MessageCallback =
      base::RepeatingCallback<void(const internal::PeerMessageStruct& message)>;

  CorpMessageChannelStrategy();

  CorpMessageChannelStrategy(const CorpMessageChannelStrategy&) = delete;
  CorpMessageChannelStrategy& operator=(const CorpMessageChannelStrategy&) =
      delete;

  ~CorpMessageChannelStrategy() override;

  void Initialize(const StreamOpener& stream_opener,
                  const MessageCallback& on_incoming_msg);

  // MessageChannelStrategy implementation.
  std::unique_ptr<ScopedProtobufHttpRequest> CreateChannel(
      base::OnceClosure on_channel_ready,
      ChannelClosedCallback on_channel_closed) override;
  base::TimeDelta GetInactivityTimeout() const override;
  void set_on_channel_active(base::RepeatingClosure on_channel_active) override;

 private:
  void OnReceiveMessagesResponse(
      std::unique_ptr<internal::HostOpenChannelResponseStruct> response);

  StreamOpener stream_opener_;
  MessageCallback on_incoming_msg_;
  base::RepeatingClosure on_channel_active_;
  std::optional<base::TimeDelta> inactivity_timeout_;

  base::WeakPtrFactory<CorpMessageChannelStrategy> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_CORP_MESSAGE_CHANNEL_STRATEGY_H_
