// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_DATA_CHANNEL_MANAGER_H_
#define REMOTING_PROTOCOL_DATA_CHANNEL_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"

namespace remoting::protocol {

class MessagePipe;

// DataChannelManager helps to manage optional data channels. Consumers can
// register a function to handle data from a named data channel.
class DataChannelManager final {
 public:
  using CreateHandlerCallback =
      base::RepeatingCallback<void(const std::string& name,
                                   std::unique_ptr<MessagePipe> pipe)>;

  DataChannelManager();
  ~DataChannelManager();

  // Registers a factory function to handle a new incoming data channel with a
  // name matching |prefix|. Both |constructor| and |prefix| cannot be empty.
  // Must be called before OnRegistrationComplete() is called.
  void RegisterCreateHandlerCallback(const std::string& prefix,
                                     CreateHandlerCallback constructor);

  // Handles all pending data channels and drops those without a matching
  // handler. After this method is called, future incoming data channels without
  // a matching handler will be silently dropped.
  // Must be called exactly once.
  void OnRegistrationComplete();

  // Executes the registered callback to handle the new incoming data channel.
  // If no matching handler is found, and OnRegistrationComplete() has not been
  // called, the data channel will be held and handled later, otherwise the data
  // channel will be silently dropped.
  void OnIncomingDataChannel(const std::string& name,
                             std::unique_ptr<MessagePipe> pipe);

 private:
  std::vector<std::pair<std::string, CreateHandlerCallback>> constructors_;
  std::vector<std::pair<std::string, std::unique_ptr<MessagePipe>>>
      pending_data_channels_;
  bool is_registration_complete_ = false;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_DATA_CHANNEL_MANAGER_H_
