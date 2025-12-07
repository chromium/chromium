// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_FACTORY_H_
#define IPC_IPC_CHANNEL_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "ipc/ipc_channel.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace IPC {

class Listener;

// Encapsulates how a Channel is created. A ChannelFactory can be
// passed to the constructor of ChannelProxy or SyncChannel to tell them
// how to create underlying channel.
class COMPONENT_EXPORT(IPC) ChannelFactory {
 public:
  // Creates a factory for "native" channel emulation.
  static std::unique_ptr<ChannelFactory> Create(
      const mojo::MessagePipeHandle& handle,
      Channel::Mode mode,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner);

  // Create a factory for Mojo channels.
  static std::unique_ptr<ChannelFactory> CreateServerFactory(
      mojo::ScopedMessagePipeHandle handle,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner);

  static std::unique_ptr<ChannelFactory> CreateClientFactory(
      mojo::ScopedMessagePipeHandle handle,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner);

  virtual ~ChannelFactory() { }
  virtual std::unique_ptr<Channel> BuildChannel(Listener* listener) = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetIPCTaskRunner() = 0;
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_FACTORY_H_
