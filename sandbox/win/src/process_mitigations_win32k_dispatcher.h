// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_PROCESS_MITIGATIONS_WIN32K_DISPATCHER_H_
#define SANDBOX_SRC_PROCESS_MITIGATIONS_WIN32K_DISPATCHER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/sandbox_policy_base.h"

namespace sandbox {

// Class to maintain a reference to a OPM protected output handle.
class ProtectedVideoOutput
    : public base::RefCountedThreadSafe<ProtectedVideoOutput> {
 public:
  ProtectedVideoOutput(HANDLE handle) : handle_(handle) {}
  HANDLE handle() { return handle_; }

 private:
  friend class base::RefCountedThreadSafe<ProtectedVideoOutput>;
  ~ProtectedVideoOutput();

  HANDLE handle_;

  DISALLOW_COPY_AND_ASSIGN(ProtectedVideoOutput);
};

// This class sets up intercepts for the Win32K lockdown policy which is set
// on Windows 8 and beyond.
class ProcessMitigationsWin32KDispatcher : public Dispatcher {
 public:
  explicit ProcessMitigationsWin32KDispatcher(PolicyBase* policy_base);
  ~ProcessMitigationsWin32KDispatcher() override;

  // Dispatcher interface.
  bool SetupService(InterceptionManager* manager, IpcTag service) override;

  bool EnumDisplayMonitors(IPCInfo* ipc, CountedBuffer* buffer);
  bool GetMonitorInfo(IPCInfo* ipc, void* monitor, CountedBuffer* buffer);
  bool GetSuggestedOPMProtectedOutputArraySize(IPCInfo* ipc,
                                               std::wstring* device_name);
  bool CreateOPMProtectedOutputs(IPCInfo* ipc,
                                 std::wstring* device_name,
                                 CountedBuffer* protected_outputs);
  bool GetCertificateSize(IPCInfo* ipc,
                          std::wstring* device_name,
                          void* protected_output);
  bool GetCertificate(IPCInfo* ipc,
                      std::wstring* device_name,
                      void* protected_output,
                      void* shared_buffer_handle,
                      uint32_t shared_buffer_size);
  bool DestroyOPMProtectedOutput(IPCInfo* ipc, void* protected_output);
  bool GetOPMRandomNumber(IPCInfo* ipc,
                          void* protected_output,
                          CountedBuffer* random_number);
  bool SetOPMSigningKeyAndSequenceNumbers(IPCInfo* ipc,
                                          void* protected_output,
                                          CountedBuffer* parameters);
  bool ConfigureOPMProtectedOutput(IPCInfo* ipc,
                                   void* protected_output,
                                   void* shared_buffer_handle);
  bool GetOPMInformation(IPCInfo* ipc,
                         void* protected_output,
                         void* shared_buffer_handle);

 private:
  scoped_refptr<ProtectedVideoOutput> GetProtectedVideoOutput(
      HANDLE handle,
      bool destroy_output);

  PolicyBase* policy_base_;
  std::map<HANDLE, scoped_refptr<ProtectedVideoOutput>> protected_outputs_;
  base::Lock protected_outputs_lock_;

  DISALLOW_COPY_AND_ASSIGN(ProcessMitigationsWin32KDispatcher);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_PROCESS_MITIGATIONS_WIN32K_DISPATCHER_H_
