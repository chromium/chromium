// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_BROKER_SERVICES_H_
#define SANDBOX_WIN_SRC_BROKER_SERVICES_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "sandbox/win/src/alternate_desktop.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/sharedmem_ipc_server.h"
#include "sandbox/win/src/threadpool.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

// BrokerServicesBase ---------------------------------------------------------
// Broker implementation version 0
//
// This is an implementation of the interface BrokerServices and
// of the associated TargetProcess interface. In this implementation
// TargetProcess is a friend of BrokerServices where the later manages a
// collection of the former.
class BrokerServicesBase final : public BrokerServices,
                                 public SingletonBase<BrokerServicesBase> {
 public:
  BrokerServicesBase();

  BrokerServicesBase(const BrokerServicesBase&) = delete;
  BrokerServicesBase& operator=(const BrokerServicesBase&) = delete;

  ~BrokerServicesBase();

  // BrokerServices interface.
  ResultCode Init(std::unique_ptr<BrokerServicesDelegate> delegate) override;
  ResultCode InitForTesting(
      std::unique_ptr<BrokerServicesDelegate> delegate,
      std::unique_ptr<BrokerServicesTargetTracker> target_tracker) override;
  ResultCode CreateAlternateDesktop(Desktop desktop) override;
  void DestroyDesktops() override;
  std::unique_ptr<TargetPolicy> CreatePolicy() override;
  std::unique_ptr<TargetPolicy> CreatePolicy(std::string_view key) override;

  ResultCode SpawnTarget(const wchar_t* exe_path,
                         const wchar_t* command_line,
                         std::unique_ptr<TargetPolicy> policy,
                         DWORD* last_error,
                         PROCESS_INFORMATION* target) override;
  void SpawnTargetAsync(const wchar_t* exe_path,
                        const wchar_t* command_line,
                        std::unique_ptr<TargetPolicy> policy,
                        SpawnTargetCallback result_callback) override;
  ResultCode GetPolicyDiagnostics(
      std::unique_ptr<PolicyDiagnosticsReceiver> receiver) override;
  void SetStartingMitigations(MitigationFlags starting_mitigations) override;
  bool RatchetDownSecurityMitigations(
      MitigationFlags additional_flags) override;
  std::wstring GetDesktopName(Desktop desktop) override;

  void SetBrokerServicesDelegateForTesting(
      std::unique_ptr<BrokerServicesDelegate> delegate);

  static void FreezeTargetConfigForTesting(TargetConfig* config);

 private:
  // Implements Init and InitForTesting.
  ResultCode InitInternal(
      std::unique_ptr<BrokerServicesDelegate> delegate,
      std::unique_ptr<BrokerServicesTargetTracker> target_tracker);

  // Ensures the desktop integrity suits any process we are launching.
  ResultCode UpdateDesktopIntegrity(Desktop desktop, IntegrityLevel integrity);

  // Creates the suspended target process and returns the new process handle in
  // the result. In parallel launch mode, this function runs on the thread pool.
  CreateTargetResult CreateTarget(
      TargetProcess* target,
      const std::wstring& exe_path,
      const std::wstring& command_line,
      std::unique_ptr<StartupInformationHelper> startup_info);

  // Helper for initializing `startup_info` and `target` for CreateTarget.
  ResultCode PreSpawnTarget(const wchar_t* exe_path,
                            PolicyBase* policy_base,
                            StartupInformationHelper* startup_info,
                            std::unique_ptr<TargetProcess>& target);

  // Implementation for SpawnTarget and SpawnTargetAsync.
  // Parallel launching will be used if `allow_parallel_launch` is true and
  // BrokerServicesDelegate::EnableParallelLaunch() returns true.
  // The target creation result is returned to `result_callback`.
  void SpawnTargetAsyncImpl(const wchar_t* exe_path,
                            const wchar_t* command_line,
                            std::unique_ptr<PolicyBase> policy_base,
                            SpawnTargetCallback result_callback,
                            bool allow_parallel_launch);

  // This function is a wrapper for FinishSpawnTargetImpl and gets called after
  // the target process is created. This function is responsible for running
  // `result_callback` to return the process information.
  void FinishSpawnTarget(std::unique_ptr<PolicyBase> policy_base,
                         std::unique_ptr<TargetProcess> target,
                         SpawnTargetCallback result_callback,
                         CreateTargetResult target_result);

  // Finishes setup after the target process is created.
  ResultCode FinishSpawnTargetImpl(
      ResultCode initial_result,
      std::unique_ptr<PolicyBase> policy_base,
      std::unique_ptr<TargetProcess> target,
      base::win::ScopedProcessInformation* process_info,
      DWORD* last_error);

  // The completion port used by the job objects to communicate events to
  // the worker thread.
  base::win::ScopedHandle job_port_;

  // Handle to the worker thread that reacts to job notifications.
  base::win::ScopedHandle job_thread_;

  // Provides a pool of threads that are used to wait on the IPC calls.
  // Owned by TargetEventsThread which is alive until our destructor.
  raw_ptr<ThreadPool, DanglingUntriaged> thread_pool_ = nullptr;

  // Handles for the alternate winstation (desktop==kAlternateWinstation).
  std::unique_ptr<AlternateDesktop> alt_winstation_;
  // Handles for the same winstation as the parent (desktop==kAlternateDesktop).
  std::unique_ptr<AlternateDesktop> alt_desktop_;

  // Cache of configs backing policies. Entries are retained until shutdown and
  // used to prime policies created by CreatePolicy() with the same `tag`.
  base::flat_map<std::string, std::unique_ptr<TargetConfig>> config_cache_;

  // Provides configuration for using parallel or synchronous process launching.
  std::unique_ptr<BrokerServicesDelegate> broker_services_delegate_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_BROKER_SERVICES_H_
