// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_TARGET_PROCESS_H_
#define SANDBOX_WIN_SRC_TARGET_PROCESS_H_

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/free_deleter.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/sandbox_types.h"

namespace base {
namespace win {

class StartupInformation;

}  // namespace win
}  // namespace base

namespace sandbox {

class SharedMemIPCServer;
class Sid;
class ThreadProvider;

// TargetProcess models a target instance (child process). Objects of this
// class are owned by the Policy used to create them.
class TargetProcess {
 public:
  // The constructor takes ownership of |initial_token| and |lockdown_token|
  TargetProcess(base::win::ScopedHandle initial_token,
                base::win::ScopedHandle lockdown_token,
                HANDLE job,
                ThreadProvider* thread_pool,
                const std::vector<Sid>& impersonation_capabilities);
  ~TargetProcess();

  // TODO(cpu): Currently there does not seem to be a reason to implement
  // reference counting for this class since is internal, but kept the
  // the same interface so the interception framework does not need to be
  // touched at this point.
  void AddRef() {}
  void Release() {}

  // Creates the new target process. The process is created suspended.
  ResultCode Create(const wchar_t* exe_path,
                    const wchar_t* command_line,
                    bool inherit_handles,
                    const base::win::StartupInformation& startup_info,
                    base::win::ScopedProcessInformation* target_info,
                    DWORD* win_error);

  // Assign a new lowbox token to the process post creation. The process
  // must still be in its initial suspended state, however this still
  // might fail in the presence of third-party software.
  ResultCode AssignLowBoxToken(const base::win::ScopedHandle& token);

  // Destroys the target process.
  void Terminate();

  // Creates the IPC objects such as the BrokerDispatcher and the
  // IPC server. The IPC server uses the services of the thread_pool.
  ResultCode Init(Dispatcher* ipc_dispatcher,
                  void* policy,
                  uint32_t shared_IPC_size,
                  uint32_t shared_policy_size,
                  DWORD* win_error);

  // Returns the handle to the target process.
  HANDLE Process() const { return sandbox_process_info_.process_handle(); }

  // Returns the handle to the job object that the target process belongs to.
  HANDLE Job() const { return job_; }

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

  // Transfers variable at |address| of |size| bytes from broker to target.
  ResultCode TransferVariable(const char* name, void* address, size_t size);

 private:
  // Details of the target process.
  base::win::ScopedProcessInformation sandbox_process_info_;
  // The token associated with the process. It provides the core of the
  // sbox security.
  base::win::ScopedHandle lockdown_token_;
  // The token given to the initial thread so that the target process can
  // start. It has more powers than the lockdown_token.
  base::win::ScopedHandle initial_token_;
  // Kernel handle to the shared memory used by the IPC server.
  base::win::ScopedHandle shared_section_;
  // Job object containing the target process.
  HANDLE job_;
  // Reference to the IPC subsystem.
  std::unique_ptr<SharedMemIPCServer> ipc_server_;
  // Provides the threads used by the IPC. This class does not own this pointer.
  ThreadProvider* thread_pool_;
  // Base address of the main executable
  void* base_address_;
  // Full name of the target executable.
  std::unique_ptr<wchar_t, base::FreeDeleter> exe_name_;
  /// List of capability sids for use when impersonating in an AC process.
  std::vector<Sid> impersonation_capabilities_;

  // Function used for testing.
  friend TargetProcess* MakeTestTargetProcess(HANDLE process,
                                              HMODULE base_address);

  DISALLOW_IMPLICIT_CONSTRUCTORS(TargetProcess);
};

// Creates a mock TargetProcess used for testing interceptions.
// TODO(cpu): It seems that this method is not going to be used anymore.
TargetProcess* MakeTestTargetProcess(HANDLE process, HMODULE base_address);

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_TARGET_PROCESS_H_
