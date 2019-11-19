// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_FACTORY_H_
#define IPC_IPC_CHANNEL_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "ipc/ipc_channel.h"

namespace mojo {
namespace internal {
class MessageQuotaChecker;
}  // namespace internal
}  // namespace mojo

namespace IPC {

// Encapsulates how a Channel is created. A ChannelFactory can be
// passed to the constructor of ChannelProxy or SyncChannel to tell them
// how to create underlying channel.
class COMPONENT_EXPORT(IPC) ChannelFactory {
 public:
  // Creates a factory for "native" channel built through
  // IPC::Channel::Create().
  static std::unique_ptr<ChannelFactory> Create(
      const ChannelHandle& handle,
      Channel::Mode mode,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner);

  virtual ~ChannelFactory() { }
  virtual std::unique_ptr<Channel> BuildChannel(Listener* listener) = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetIPCTaskRunner() = 0;
  virtual scoped_refptr<mojo::internal::MessageQuotaChecker>
  GetQuotaChecker() = 0;
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_FACTORY_H_
