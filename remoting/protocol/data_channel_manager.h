// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_DATA_CHANNEL_MANAGER_H_
#define REMOTING_PROTOCOL_DATA_CHANNEL_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"

namespace remoting {
namespace protocol {

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
  void RegisterCreateHandlerCallback(const std::string& prefix,
                                     CreateHandlerCallback constructor);

  // Executes the registered callback to handle the new incoming data channel.
  // Returns true if a handler of the new data channel has been executed.
  bool OnIncomingDataChannel(const std::string& name,
                             std::unique_ptr<MessagePipe> pipe);

 private:
  std::vector<std::pair<std::string, CreateHandlerCallback>> constructors_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_DATA_CHANNEL_MANAGER_H_
