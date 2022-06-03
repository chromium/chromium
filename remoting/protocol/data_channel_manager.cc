// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/data_channel_manager.h"

#include <utility>

#include "base/check.h"
#include "remoting/protocol/message_pipe.h"

namespace remoting {
namespace protocol {

DataChannelManager::DataChannelManager() = default;
DataChannelManager::~DataChannelManager() = default;

void DataChannelManager::RegisterCreateHandlerCallback(
    const std::string& prefix,
    CreateHandlerCallback constructor) {
  DCHECK(!prefix.empty());
  DCHECK(constructor);
  constructors_.push_back(std::make_pair(prefix, std::move(constructor)));
}

bool DataChannelManager::OnIncomingDataChannel(
    const std::string& name,
    std::unique_ptr<MessagePipe> pipe) {
  for (auto& constructor : constructors_) {
    if (name.find(constructor.first) == 0) {
      constructor.second.Run(name, std::move(pipe));
      return true;
    }
  }
  return false;
}

}  // namespace protocol
}  // namespace remoting
