// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_BROKER_HOST_H_
#define MOJO_CORE_BROKER_HOST_H_

#include <stdint.h>

#include <vector>

#include "base/task/current_thread.h"
#include "mojo/core/channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace mojo {
namespace core {

// The BrokerHost is a channel to a broker client process, servicing synchronous
// IPCs issued by the client.
class BrokerHost : public Channel::Delegate,
                   public base::CurrentThread::DestructionObserver {
 public:
  explicit BrokerHost(PlatformChannelEndpoint endpoint);

  BrokerHost(const BrokerHost&) = delete;
  BrokerHost& operator=(const BrokerHost&) = delete;

 private:
  ~BrokerHost() override;

  // Channel::Delegate:
  void OnChannelMessage(const void* payload,
                        size_t payload_size,
                        std::vector<PlatformHandle> handles,
                        scoped_refptr<ipcz_driver::Envelope>) override;
  void OnChannelError(Channel::Error error) override;

  // base::CurrentThread::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override;

  void OnBufferRequest(uint32_t num_bytes);

  scoped_refptr<Channel> channel_;
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_BROKER_HOST_H_
