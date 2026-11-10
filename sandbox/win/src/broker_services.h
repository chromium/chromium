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
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
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
//
// Launching, tagged policy creation, and shutdown share a sequence. Startup
// initialization and desktop creation may run on another thread before
// launching. Diagnostics and IPC metrics can be requested off the launch
// sequence. After Shutdown(), CRT teardown may destroy the broker on another
// thread.
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
  void Shutdown() override;
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

  // No launches or IPC callbacks may be using the delegate when it is replaced.
  void SetBrokerServicesDelegateForTesting(
      std::unique_ptr<BrokerServicesDelegate> delegate);

  // Gets helper to log histograms from sandbox IPC workers within the exe.
  BrokerServicesDelegate* GetMetricsDelegate();

  static void FreezeTargetConfigForTesting(TargetConfig* config);

 private:
  // Implements Init and InitForTesting.
  ResultCode InitInternal(
      std::unique_ptr<BrokerServicesDelegate> delegate,
      std::unique_ptr<BrokerServicesTargetTracker> target_tracker);

  // Ensures the desktop integrity suits any process we are launching.
  ResultCode UpdateDesktopIntegrity(Desktop desktop, IntegrityLevel integrity)
      VALID_CONTEXT_REQUIRED(launch_sequence_checker_);

  using CreateTargetInfo =
      std::pair<std::unique_ptr<StartupInformationHelper>, TargetTokens>;

  // Creates the suspended target process and returns the new process handle in
  // the result. Must not access broker state: this task can outlive the broker
  // during shutdown.
  static CreateTargetResult CreateTarget(base::CommandLine cmd_line,
                                         CreateTargetInfo target_info);

  // Helper for initializing the CreateTargetInfo for CreateTarget.
  base::expected<CreateTargetInfo, ResultCode> PreSpawnTarget(
      PolicyBase* policy_base) VALID_CONTEXT_REQUIRED(launch_sequence_checker_);

  // Implementation for SpawnTarget and SpawnTargetAsync. The target creation
  // result is returned to `result_callback`.
  void SpawnTargetAsyncImpl(const base::CommandLine& command_line,
                            std::unique_ptr<PolicyBase> policy_base,
                            SpawnTargetCallback result_callback)
      VALID_CONTEXT_REQUIRED(launch_sequence_checker_);

  // Finishes target setup and invokes the result callback.
  void FinishSpawnTarget(std::unique_ptr<PolicyBase> policy_base,
                         SpawnTargetCallback result_callback,
                         CreateTargetResult target_result);

  // Finishes setup after the target process is created.
  ResultCode FinishSpawnTargetImpl(
      ResultCode initial_result,
      std::unique_ptr<PolicyBase> policy_base,
      const base::win::ScopedProcessInformation& process_info,
      DWORD& last_error) VALID_CONTEXT_REQUIRED(launch_sequence_checker_);

  // Startup does not bind this checker. Launch-sequence work must finish before
  // destruction, which may run off sequence.
  SEQUENCE_CHECKER(launch_sequence_checker_);

  // Both job handles are assigned in InitInternal(), not the constructor, so
  // Init()/InitForTesting() can report creation failures. They cannot be const
  // with this two-phase initialization.
  //
  // The completion port used by the job objects to communicate events to
  // the worker thread. Initialized before launching and diagnostics, then
  // unchanged until destruction.
  base::win::ScopedHandle job_port_;

  // Handle to the worker thread that reacts to job notifications. Initialized
  // before launching and diagnostics, then unchanged until destruction.
  base::win::ScopedHandle job_thread_;

  // Provides a pool of threads that are used to wait on the IPC calls.
  // Published by InitInternal() before launching and owned by
  // TargetEventsThread, which is alive until our destructor.
  raw_ptr<ThreadPool, DanglingUntriaged> thread_pool_ = nullptr;

  // Desktops are created before launching and their integrity is updated on the
  // launch sequence. Tests may destroy/recreate them while launches are
  // quiescent. Handles for the alternate winstation
  // (desktop==kAlternateWinstation).
  std::unique_ptr<AlternateDesktop> alt_winstation_;
  // Handles for the same winstation as the parent (desktop==kAlternateDesktop).
  std::unique_ptr<AlternateDesktop> alt_desktop_;

  // Configurations shared by policies created with the same tag. Each policy
  // retains its configuration independently so it can outlive the broker.
  base::flat_map<std::string, scoped_refptr<ConfigBase>> config_cache_
      GUARDED_BY_CONTEXT(launch_sequence_checker_);

  // Published during initialization, then used by the launcher and IPC workers.
  // Test-only replacement requires quiescing users of the delegate.
  std::unique_ptr<BrokerServicesDelegate> broker_services_delegate_;

  bool is_shutdown_ GUARDED_BY_CONTEXT(launch_sequence_checker_) = false;

  // Shutdown() dooms the factory on the launch sequence before CRT teardown
  // destroys the broker, potentially on a different thread.
  base::WeakPtrFactory<BrokerServicesBase> weak_factory_
      GUARDED_BY_CONTEXT(launch_sequence_checker_){this};
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_BROKER_SERVICES_H_
