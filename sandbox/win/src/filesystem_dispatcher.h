// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_FILESYSTEM_DISPATCHER_H_
#define SANDBOX_WIN_SRC_FILESYSTEM_DISPATCHER_H_

#include <stdint.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/sandbox_policy_base.h"

namespace sandbox {

// This class handles file system-related IPC calls.
class FilesystemDispatcher : public Dispatcher {
 public:
  explicit FilesystemDispatcher(PolicyBase* policy_base);

  FilesystemDispatcher(const FilesystemDispatcher&) = delete;
  FilesystemDispatcher& operator=(const FilesystemDispatcher&) = delete;

  ~FilesystemDispatcher() override {}

  // Dispatcher interface.
  bool SetupService(InterceptionManager* manager, IpcTag service) override;

 private:
  // Processes IPC requests coming from calls to NtCreateFile in the target.
  bool NtCreateFile(IPCInfo* ipc,
                    std::wstring* name,
                    uint32_t attributes,
                    uint32_t desired_access,
                    uint32_t file_attributes,
                    uint32_t share_access,
                    uint32_t create_disposition,
                    uint32_t create_options);

  // Processes IPC requests coming from calls to NtOpenFile in the target.
  bool NtOpenFile(IPCInfo* ipc,
                  std::wstring* name,
                  uint32_t attributes,
                  uint32_t desired_access,
                  uint32_t share_access,
                  uint32_t create_options);

  // Processes IPC requests coming from calls to NtQueryAttributesFile in the
  // target.
  bool NtQueryAttributesFile(IPCInfo* ipc,
                             std::wstring* name,
                             uint32_t attributes,
                             CountedBuffer* info);

  // Processes IPC requests coming from calls to NtQueryFullAttributesFile in
  // the target.
  bool NtQueryFullAttributesFile(IPCInfo* ipc,
                                 std::wstring* name,
                                 uint32_t attributes,
                                 CountedBuffer* info);

  // Processes IPC requests coming from calls to NtSetInformationFile with the
  // rename information class.
  bool NtSetInformationFile(IPCInfo* ipc,
                            HANDLE handle,
                            CountedBuffer* status,
                            CountedBuffer* info,
                            uint32_t length,
                            uint32_t info_class);

  // Evaluate the sandbox policy for the file system call.
  EvalResult EvalPolicy(IpcTag ipc_tag,
                        const std::wstring& name,
                        uint32_t desired_access = 0,
                        bool open_only = true);

  raw_ptr<PolicyBase> policy_base_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_FILESYSTEM_DISPATCHER_H_
