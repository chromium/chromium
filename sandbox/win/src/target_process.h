// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_TARGET_PROCESS_H_
#define SANDBOX_WIN_SRC_TARGET_PROCESS_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

class Dispatcher;
class SharedMemIPCServer;
class ThreadPool;

// TargetProcess models a target instance (child process). Objects of this
// class are owned by the Policy used to create them.
class TargetProcess {
 public:
  TargetProcess();
  TargetProcess(HANDLE process_handle);

  TargetProcess(const TargetProcess&) = delete;
  TargetProcess& operator=(const TargetProcess&) = delete;

  ~TargetProcess();

  // Destroys the target process.
  void Terminate();

  // Initialize the target process. Creates the IPC objects such as the
  // BrokerDispatcher and the IPC server. The IPC server uses the services of
  // the thread_pool.
  ResultCode Init(Dispatcher* ipc_dispatcher,
                  std::optional<base::span<const uint8_t>> policy,
                  std::optional<base::span<const uint8_t>> delegate_data,
                  uint32_t shared_IPC_size,
                  ThreadPool* thread_pool,
                  DWORD* win_error);

  // Returns the handle to the target process.
  HANDLE Process() const { return process_handle_.get(); }

  // Returns the process id.
  DWORD ProcessId() const { return process_id_; }

  // Transfers variable at `local_address` of `size` bytes from broker to
  // `target_address` of target.
  ResultCode TransferVariable(const void* local_address,
                              void* target_address,
                              size_t size);

 private:
  // Verify the target process looks the same as the broker process.
  ResultCode VerifySentinels();

  // Details of the target process.
  base::win::ScopedHandle process_handle_;
  DWORD process_id_;
  // Kernel handle to the shared memory used by the IPC server.
  base::win::ScopedHandle shared_section_;
  // Reference to the IPC subsystem.
  std::unique_ptr<SharedMemIPCServer> ipc_server_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_TARGET_PROCESS_H_
