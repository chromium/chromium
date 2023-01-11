// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_MESSAGE_CHANNEL_FACTORY_H_
#define REMOTING_PROTOCOL_MESSAGE_CHANNEL_FACTORY_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"

namespace remoting::protocol {

class MessagePipe;

class MessageChannelFactory {
 public:
  typedef base::OnceCallback<void(std::unique_ptr<MessagePipe>)>
      ChannelCreatedCallback;

  virtual ~MessageChannelFactory() {}

  // Creates new channels and calls the |callback| when then new channel is
  // created and connected. Callback may be called synchronously, before the
  // call returns. If channel creation fails the callback is never called. All
  // channels must be destroyed, and CancelChannelCreation() called for any
  // pending channels, before the factory is destroyed.
  virtual void CreateChannel(const std::string& name,
                             ChannelCreatedCallback callback) = 0;

  // Cancels a pending CreateChannel() operation for the named channel. If the
  // channel creation already completed then canceling it has no effect. When
  // shutting down this method must be called for each channel pending creation.
  virtual void CancelChannelCreation(const std::string& name) = 0;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_MESSAGE_CHANNEL_FACTORY_H_
