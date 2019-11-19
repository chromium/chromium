// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/remote_security_key_main.h"

#include <memory>
#include <string>
#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/logging.h"
#include "remoting/host/security_key/security_key_ipc_client.h"
#include "remoting/host/security_key/security_key_message_handler.h"

#if defined(OS_WIN)
#include <aclapi.h>
#include <windows.h>

#include "base/win/scoped_handle.h"
#endif  // defined(OS_WIN)

#if defined(OS_WIN)
namespace {

bool AddAccessRightForWellKnownSid(WELL_KNOWN_SID_TYPE type, DWORD new_right) {
  // Open a handle for the current process, read the current DACL, update it,
  // and write it back.  This will add |new_right| to the current process.
  base::win::ScopedHandle process_handle(OpenProcess(READ_CONTROL | WRITE_DAC,
                                                     /*bInheritHandle=*/FALSE,
                                                     GetCurrentProcessId()));
  if (!process_handle.IsValid()) {
    PLOG(ERROR) << "OpenProcess() failed!";
    return false;
  }

  // TODO(joedow): Add a custom deleter to handle objects which are freed via
  // LocalFree().  Tracked by crbug.com/622913
  PSECURITY_DESCRIPTOR descriptor = nullptr;
  // |old_dacl| is a pointer into the opaque |descriptor| struct, don't free it.
  PACL old_dacl = nullptr;
  PACL new_dacl = nullptr;

  if (GetSecurityInfo(process_handle.Get(),
                      SE_KERNEL_OBJECT,
                      DACL_SECURITY_INFORMATION,
                      /*ppsidOwner=*/nullptr,
                      /*ppsidGroup=*/nullptr,
                      &old_dacl,
                      /*ppSacl=*/nullptr,
                      &descriptor) != ERROR_SUCCESS) {
    PLOG(ERROR) << "GetSecurityInfo() failed!";
    return false;
  }

  BYTE buffer[SECURITY_MAX_SID_SIZE] = {0};
  DWORD buffer_size = SECURITY_MAX_SID_SIZE;
  if (!CreateWellKnownSid(type, /*DomainSid=*/nullptr, buffer, &buffer_size)) {
    PLOG(ERROR) << "CreateWellKnownSid() failed!";
    LocalFree(descriptor);
    return false;
  }

  SID* sid = reinterpret_cast<SID*>(buffer);
  EXPLICIT_ACCESS new_access = {0};
  new_access.grfAccessMode = GRANT_ACCESS;
  new_access.grfAccessPermissions = new_right;
  new_access.grfInheritance = NO_INHERITANCE;

  new_access.Trustee.pMultipleTrustee = nullptr;
  new_access.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
  new_access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
  new_access.Trustee.ptstrName = reinterpret_cast<LPWSTR>(sid);

  if (SetEntriesInAcl(1, &new_access, old_dacl, &new_dacl) != ERROR_SUCCESS) {
    PLOG(ERROR) << "SetEntriesInAcl() failed!";
    LocalFree(descriptor);
    return false;
  }

  bool right_added = true;
  if (SetSecurityInfo(process_handle.Get(),
                      SE_KERNEL_OBJECT,
                      DACL_SECURITY_INFORMATION,
                      /*ppsidOwner=*/nullptr,
                      /*ppsidGroup=*/nullptr,
                      new_dacl,
                      /*ppSacl=*/nullptr) != ERROR_SUCCESS) {
    PLOG(ERROR) << "SetSecurityInfo() failed!";
    right_added = false;
  }

  LocalFree(new_dacl);
  LocalFree(descriptor);

  return right_added;
}

}  // namespace
#endif  // defined(OS_WIN)

namespace remoting {

int StartRemoteSecurityKey() {
#if defined(OS_WIN)
  if (!AddAccessRightForWellKnownSid(WinLocalServiceSid,
                                     PROCESS_QUERY_LIMITED_INFORMATION)) {
    return kInitializationFailed;
  }

  // GetStdHandle() returns pseudo-handles for stdin and stdout even if
  // the hosting executable specifies "Windows" subsystem. However the returned
  // handles are invalid in that case unless standard input and output are
  // redirected to a pipe or file.
  base::File read_file(GetStdHandle(STD_INPUT_HANDLE));
  base::File write_file(GetStdHandle(STD_OUTPUT_HANDLE));

  // After the message handler starts, the security key message reader
  // will keep doing blocking read operations on the input named pipe.
  // If any other thread tries to perform any operation on STDIN, it will also
  // block because the input named pipe is synchronous (non-overlapped).
  // It is pretty common for a DLL to query the device info (GetFileType) of
  // the STD* handles at startup. So any LoadLibrary request can potentially
  // be blocked. To prevent that from happening we close STDIN and STDOUT
  // handles as soon as we retrieve the corresponding file handles.
  SetStdHandle(STD_INPUT_HANDLE, nullptr);
  SetStdHandle(STD_OUTPUT_HANDLE, nullptr);
#elif defined(OS_POSIX)
  // The files are automatically closed.
  base::File read_file(STDIN_FILENO);
  base::File write_file(STDOUT_FILENO);
#else
#error Not implemented.
#endif

  mojo::core::Init();
  mojo::core::ScopedIPCSupport ipc_support(
      base::ThreadTaskRunnerHandle::Get(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

  base::RunLoop run_loop;

  std::unique_ptr<SecurityKeyIpcClient> ipc_client(new SecurityKeyIpcClient());

  SecurityKeyMessageHandler message_handler;
  message_handler.Start(std::move(read_file), std::move(write_file),
                        std::move(ipc_client), run_loop.QuitClosure());

  run_loop.Run();

  return kSuccessExitCode;
}

int RemoteSecurityKeyMain(int argc, char** argv) {
  // This object instance is required by Chrome classes (such as MessageLoop).
  base::AtExitManager exit_manager;
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);

  base::CommandLine::Init(argc, argv);
  remoting::InitHostLogging();

  return StartRemoteSecurityKey();
}

}  // namespace remoting
