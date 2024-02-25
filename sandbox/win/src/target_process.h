// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_TARGET_PROCESS_H_
#define SANDBOX_WIN_SRC_TARGET_PROCESS_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/free_deleter.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/win/access_token.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/sid.h"
#include "base/win/windows_types.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

class Dispatcher;
class SharedMemIPCServer;
class ThreadPool;
class StartupInformationHelper;

// TargetProcess models a target instance (child process). Objects of this
// class are owned by the Policy used to create them.
class TargetProcess {
 public:
  TargetProcess() = delete;

  // The constructor takes ownership of `initial_token` and `lockdown_token`.
  TargetProcess(base::win::AccessToken initial_token,
                base::win::AccessToken lockdown_token,
                ThreadPool* thread_pool);

  TargetProcess(const TargetProcess&) = delete;
  TargetProcess& operator=(const TargetProcess&) = delete;

  ~TargetProcess();

  // Creates the new target process. The process is created suspended.
  ResultCode Create(const wchar_t* exe_path,
                    const wchar_t* command_line,
                    std::unique_ptr<StartupInformationHelper> startup_info,
                    base::win::ScopedProcessInformation* target_info,
                    DWORD* win_error);

  // Destroys the target process.
  void Terminate();

  // Creates the IPC objects such as the BrokerDispatcher and the
  // IPC server. The IPC server uses the services of the thread_pool.
  ResultCode Init(Dispatcher* ipc_dispatcher,
                  std::optional<base::span<const uint8_t>> policy,
                  std::optional<base::span<const uint8_t>> delegate_data,
                  uint32_t shared_IPC_size,
                  DWORD* win_error);

  // Returns the handle to the target process.
  HANDLE Process() const { return sandbox_process_info_.process_handle(); }

  // Returns the address of the target main exe. This is used by the
  // interceptions framework.
  HMODULE MainModule() const {
    return reinterpret_cast<HMODULE>(base_address_);
  }

  // Returns the name of the executable.
  const wchar_t* Name() const { return exe_name_.get(); }

  // Returns the process id.
  DWORD ProcessId() const { return sandbox_process_info_.process_id(); }

  // Returns the handle to the main thread.
  HANDLE MainThread() const { return sandbox_process_info_.thread_handle(); }

  // Transfers variable at `local_address` of `size` bytes from broker to
  // `target_address` of target.
  ResultCode TransferVariable(const char* name,
                              const void* local_address,
                              void* target_address,
                              size_t size);

  // Creates a mock TargetProcess used for testing interceptions.
  static std::unique_ptr<TargetProcess> MakeTargetProcessForTesting(
      HANDLE process,
      HMODULE base_address);

 private:
  FRIEND_TEST_ALL_PREFIXES(TargetProcessTest, FilterEnvironment);
  // Verify the target process looks the same as the broker process.
  ResultCode VerifySentinels();

  // Filters an environment to only include those that have an entry in
  // `to_keep`.
  static std::wstring FilterEnvironment(
      const wchar_t* env,
      const base::span<const std::wstring_view> to_keep);

  // Details of the target process.
  base::win::ScopedProcessInformation sandbox_process_info_;
  // The token associated with the process. It provides the core of the
  // sbox security.
  base::win::AccessToken lockdown_token_;
  // The token given to the initial thread so that the target process can
  // start. It has more powers than the lockdown_token.
  base::win::AccessToken initial_token_;
  // Kernel handle to the shared memory used by the IPC server.
  base::win::ScopedHandle shared_section_;
  // Reference to the IPC subsystem.
  std::unique_ptr<SharedMemIPCServer> ipc_server_;
  // Provides the threads used by the IPC. This class does not own this pointer.
  raw_ptr<ThreadPool> thread_pool_;
  // Base address of the main executable
  //
  // `base_address_` is not a raw_ptr<void>, because pointer to address in
  // another process could be confused as a pointer to PartitionMalloc memory,
  // causing ref-counting mismatch.  See also https://crbug.com/1173374.
  RAW_PTR_EXCLUSION void* base_address_;
  // Full name of the target executable.
  std::unique_ptr<wchar_t, base::FreeDeleter> exe_name_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_TARGET_PROCESS_H_
