// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_MESSAGE_CHANNEL_STRATEGY_H_
#define REMOTING_SIGNALING_FTL_MESSAGE_CHANNEL_STRATEGY_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "remoting/signaling/message_channel_strategy.h"

namespace remoting {

class ScopedProtobufHttpRequest;

namespace ftl {
class ReceiveMessagesResponse;
class InboxMessage;
}  // namespace ftl

class FtlMessageChannelStrategy : public MessageChannelStrategy {
 public:
  using MessageReceivedCallback = base::RepeatingCallback<void(
      std::unique_ptr<ftl::ReceiveMessagesResponse>)>;
  using StreamOpener =
      base::RepeatingCallback<std::unique_ptr<ScopedProtobufHttpRequest>(
          base::OnceClosure on_channel_ready,
          const MessageReceivedCallback& on_incoming_msg,
          ChannelClosedCallback on_channel_closed)>;
  using MessageCallback =
      base::RepeatingCallback<void(const ftl::InboxMessage& message)>;

  FtlMessageChannelStrategy();

  FtlMessageChannelStrategy(const FtlMessageChannelStrategy&) = delete;
  FtlMessageChannelStrategy& operator=(const FtlMessageChannelStrategy&) =
      delete;

  ~FtlMessageChannelStrategy() override;

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
      std::unique_ptr<ftl::ReceiveMessagesResponse> response);

  StreamOpener stream_opener_;
  MessageCallback on_incoming_msg_;
  base::RepeatingClosure on_channel_active_;

  base::WeakPtrFactory<FtlMessageChannelStrategy> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_MESSAGE_CHANNEL_STRATEGY_H_
