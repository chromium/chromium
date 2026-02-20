// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_BROKER_SERVICES_H_
#define SANDBOX_WIN_SRC_BROKER_SERVICES_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
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

class StartupInformationHelper;

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

  ~BrokerServicesBase() override;

  // BrokerServices interface.
  ResultCode Init(std::unique_ptr<BrokerServicesDelegate> delegate) override;
  ResultCode InitForTesting(
      std::unique_ptr<BrokerServicesDelegate> delegate,
      std::unique_ptr<BrokerServicesTargetTracker> target_tracker) override;
  ResultCode CreateAlternateDesktop(Desktop desktop) override;
  void DestroyDesktops() override;
  std::unique_ptr<TargetPolicy> CreatePolicy() override;
  std::unique_ptr<TargetPolicy> CreatePolicy(std::string_view key) override;

  void SpawnTargetAsync(const base::CommandLine& command_line,
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

  // Gets helper to log histograms from within the exe.
  BrokerServicesDelegate* GetMetricsDelegate();

  static void FreezeTargetConfigForTesting(TargetConfig* config);

 private:
  // Implements Init and InitForTesting.
  ResultCode InitInternal(
      std::unique_ptr<BrokerServicesDelegate> delegate,
      std::unique_ptr<BrokerServicesTargetTracker> target_tracker);

  // Ensures the desktop integrity suits any process we are launching.
  ResultCode UpdateDesktopIntegrity(Desktop desktop, IntegrityLevel integrity);

  using CreateTargetInfo =
      std::pair<std::unique_ptr<StartupInformationHelper>, TargetTokens>;

  // Creates the suspended target process and returns the new process handle in
  // the result.
  CreateTargetResult CreateTarget(base::CommandLine cmd_line,
                                  CreateTargetInfo target_info);

  // Helper for initializing the CreateTargetInfo for CreateTarget.
  base::expected<CreateTargetInfo, ResultCode> PreSpawnTarget(
      PolicyBase* policy_base);

  // Implementation for SpawnTarget and SpawnTargetAsync. The target creation
  // result is returned to `result_callback`.
  void SpawnTargetAsyncImpl(const base::CommandLine& command_line,
                            std::unique_ptr<PolicyBase> policy_base,
                            SpawnTargetCallback result_callback);

  // This function is a wrapper for FinishSpawnTargetImpl and gets called after
  // the target process is created. This function is responsible for running
  // `result_callback` to return the process information.
  void FinishSpawnTarget(std::unique_ptr<PolicyBase> policy_base,
                         SpawnTargetCallback result_callback,
                         CreateTargetResult target_result);

  // Finishes setup after the target process is created.
  ResultCode FinishSpawnTargetImpl(
      ResultCode initial_result,
      std::unique_ptr<PolicyBase> policy_base,
      const base::win::ScopedProcessInformation& process_info,
      DWORD& last_error);

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
