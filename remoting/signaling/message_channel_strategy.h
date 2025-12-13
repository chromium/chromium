// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_MESSAGE_CHANNEL_STRATEGY_H_
#define REMOTING_SIGNALING_MESSAGE_CHANNEL_STRATEGY_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"

namespace remoting {

class HttpStatus;
class ScopedProtobufHttpRequest;

// An interface for handling use-case specific logic for MessageChannel.
class MessageChannelStrategy {
 public:
  using ChannelClosedCallback = base::OnceCallback<void(const HttpStatus&)>;

  virtual ~MessageChannelStrategy() = default;

  // Called by the message channel to provide a callback which should be run
  // each time a channel active message is received.
  virtual void set_on_channel_active(
      base::RepeatingClosure on_channel_active) = 0;

  // Creates the specific channel implementation for the stream.
  virtual std::unique_ptr<ScopedProtobufHttpRequest> CreateChannel(
      base::OnceClosure on_channel_ready,
      ChannelClosedCallback on_channel_closed) = 0;

  // Returns the inactivity timeout for the stream.
  virtual base::TimeDelta GetInactivityTimeout() const = 0;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_MESSAGE_CHANNEL_STRATEGY_H_
