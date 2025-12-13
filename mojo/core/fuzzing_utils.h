// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_FUZZING_UTILS_H_
#define MOJO_CORE_FUZZING_UTILS_H_

#include "base/task/single_thread_task_executor.h"
#include "mojo/core/channel.h"
#include "mojo/core/entrypoints.h"
#include "mojo/core/ipcz_driver/envelope.h"

namespace mojo::core {

// A fake delegate for each Channel endpoint. By the time an incoming message
// reaches a Delegate, all interesting message parsing at the lowest protocol
// layer has already been done by the receiving Channel implementation, so this
// doesn't need to do any work.
class FakeChannelDelegate : public mojo::core::Channel::Delegate {
 public:
  explicit FakeChannelDelegate(bool is_ipcz_transport)
      : is_ipcz_transport_(is_ipcz_transport) {}
  ~FakeChannelDelegate() override = default;

  void OnChannelMessage(
      const void* payload,
      size_t payload_size,
      std::vector<mojo::PlatformHandle> handles,
      scoped_refptr<mojo::core::ipcz_driver::Envelope> envelope) override {}
  void OnChannelError(mojo::core::Channel::Error error) override {}

  bool IsIpczTransport() const override;

 private:
  const bool is_ipcz_transport_;
};

// Message deserialization may register handles in the global handle table. We
// need to initialize Core for that to be OK.
struct Environment {
  Environment() : main_thread_task_executor(base::MessagePumpType::IO) {
    mojo::core::InitializeCore();
  }

  base::SingleThreadTaskExecutor main_thread_task_executor;
};

}  // namespace mojo::core

#endif  // MOJO_CORE_FUZZING_UTILS_H_
