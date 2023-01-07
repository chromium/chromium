// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/data_channel_manager.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "remoting/protocol/message_pipe.h"

namespace remoting::protocol {

DataChannelManager::DataChannelManager() = default;
DataChannelManager::~DataChannelManager() = default;

void DataChannelManager::RegisterCreateHandlerCallback(
    const std::string& prefix,
    CreateHandlerCallback constructor) {
  DCHECK(!prefix.empty());
  DCHECK(constructor);
  DCHECK(!is_registration_complete_);
  constructors_.emplace_back(prefix, std::move(constructor));
}

void DataChannelManager::OnRegistrationComplete() {
  DCHECK(!is_registration_complete_);
  is_registration_complete_ = true;
  for (auto& pending_channel : pending_data_channels_) {
    OnIncomingDataChannel(pending_channel.first,
                          std::move(pending_channel.second));
  }
  pending_data_channels_.clear();
}

void DataChannelManager::OnIncomingDataChannel(
    const std::string& name,
    std::unique_ptr<MessagePipe> pipe) {
  for (auto& constructor : constructors_) {
    if (name.find(constructor.first) == 0) {
      constructor.second.Run(name, std::move(pipe));
      return;
    }
  }
  if (!is_registration_complete_) {
    VLOG(1) << "Handler for " << name << " has not been registered. "
            << "Will try again later.";
    pending_data_channels_.emplace_back(name, std::move(pipe));
  } else {
    LOG(WARNING) << "Channel " << name
                 << " ignored due to no matching handler.";
  }
}

}  // namespace remoting::protocol
