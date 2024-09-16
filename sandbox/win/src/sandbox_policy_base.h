// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SANDBOX_POLICY_BASE_H_
#define SANDBOX_WIN_SRC_SANDBOX_POLICY_BASE_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <optional>
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/memory/raw_ptr.h"
#include "base/process/launch.h"
#include "base/synchronization/lock.h"
#include "base/win/access_token.h"
#include "base/win/windows_types.h"
#include "sandbox/win/src/app_container_base.h"
#include "sandbox/win/src/handle_closer.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/job.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_engine_params.h"
#include "sandbox/win/src/sandbox_policy.h"

namespace sandbox {

class BrokerServicesBase;
class Dispatcher;
class LowLevelPolicy;
class PolicyDiagnostic;
class TargetProcess;
struct PolicyGlobal;

// The members of this class are shared between multiple sandbox::PolicyBase
// objects and must be safe for access from multiple threads once created.
// When shared members will not be destroyed until BrokerServicesBase is
// destroyed at process shutdown.
class ConfigBase final : public TargetConfig {
 public:
  ConfigBase() noexcept;
  ~ConfigBase() override;

  ConfigBase(const ConfigBase&) = delete;
  ConfigBase& operator=(const ConfigBase&) = delete;

  bool IsConfigured() const override;

  ResultCode SetTokenLevel(TokenLevel initial, TokenLevel lockdown) override;
  TokenLevel GetInitialTokenLevel() const override;
  TokenLevel GetLockdownTokenLevel() const override;
  ResultCode SetJobLevel(JobLevel job_level, uint32_t ui_exceptions) override;
  JobLevel GetJobLevel() const override;
  void SetJobMemoryLimit(size_t memory_limit) override;
  ResultCode AllowFileAccess(FileSemantics semantics,
                             const wchar_t* pattern) override;
  ResultCode AllowExtraDlls(const wchar_t* pattern) override;
  ResultCode SetFakeGdiInit() override;
  void AddDllToUnload(const wchar_t* dll_name) override;
  ResultCode SetIntegrityLevel(IntegrityLevel integrity_level) override;
  IntegrityLevel GetIntegrityLevel() const override;
  void SetDelayedIntegrityLevel(IntegrityLevel integrity_level) override;
  ResultCode SetLowBox(const wchar_t* sid) override;
  ResultCode SetProcessMitigations(MitigationFlags flags) override;
  MitigationFlags GetProcessMitigations() override;
  ResultCode SetDelayedProcessMitigations(MitigationFlags flags) override;
  MitigationFlags GetDelayedProcessMitigations() const override;
  void AddRestrictingRandomSid() override;
  void SetLockdownDefaultDacl() override;
  ResultCode AddAppContainerProfile(const wchar_t* package_name) override;
  AppContainer* GetAppContainer() override;
  void AddKernelObjectToClose(HandleToClose handle_info) override;
  void SetDisconnectCsrss() override;
  void SetDesktop(Desktop desktop) override;
  void SetFilterEnvironment(bool filter) override;
  bool GetEnvironmentFiltered() override;
  void SetZeroAppShim() override;

 private:
  // Can call Freeze() and is_csrss_connected().
  friend class BrokerServicesBase;
  // Can examine private fields.
  friend class PolicyDiagnostic;
  // Can call private accessors.
  friend class PolicyBase;
  // Can ask for the low-level policy.
  friend class TopLevelDispatcher;

  // Promise that no further changes will be made to the configuration, and
  // this object can be reused by multiple policies.
  bool Freeze();

  // Use in DCHECK only - returns `true` in non-DCHECK builds.
  bool IsOnCreatingThread() const;

  // Lazily populates the policy_ and policy_maker_ members for internal rules.
  // Can only be called before the object is fully configured.
  LowLevelPolicy* PolicyMaker();

  // Some IPCs are only configured if a matching policy has been set, this
  // method allows TopLevelDispatcher to determine if a policy exists for a
  // given service. Only call after calling Freeze().
  bool NeedsIpc(IpcTag service) const;

#if DCHECK_IS_ON()
  // Used to sequence-check in DCHECK builds.
  uint32_t creating_thread_id_;
#endif  // DCHECK_IS_ON()

  // Once true the configuration is frozen and can be applied to later policies.
  bool configured_ = false;

  // Should only be called once the object is configured.
  PolicyGlobal* policy();
  std::optional<base::span<const uint8_t>> policy_span();
  std::vector<std::wstring>& blocklisted_dlls();
  AppContainerBase* app_container();
  IntegrityLevel integrity_level() { return integrity_level_; }
  IntegrityLevel delayed_integrity_level() { return delayed_integrity_level_; }
  bool add_restricting_random_sid() { return add_restricting_random_sid_; }
  bool lockdown_default_dacl() { return lockdown_default_dacl_; }
  bool is_csrss_connected() { return is_csrss_connected_; }
  size_t memory_limit() { return memory_limit_; }
  uint32_t ui_exceptions() { return ui_exceptions_; }
  Desktop desktop() { return desktop_; }
  const HandleCloserConfig& handle_closer() { return handle_closer_; }
  bool zero_appshim() { return zero_appshim_; }

  TokenLevel lockdown_level_;
  TokenLevel initial_level_;
  JobLevel job_level_;
  IntegrityLevel integrity_level_;
  IntegrityLevel delayed_integrity_level_;
  MitigationFlags mitigations_;
  MitigationFlags delayed_mitigations_;
  bool add_restricting_random_sid_;
  bool lockdown_default_dacl_;
  bool is_csrss_connected_;
  size_t memory_limit_;
  uint32_t ui_exceptions_;
  Desktop desktop_;
  bool filter_environment_;
  bool zero_appshim_;
  HandleCloserConfig handle_closer_;

  // Object in charge of generating the low level policy. Will be reset() when
  // Freeze() is called.
  std::unique_ptr<LowLevelPolicy> policy_maker_;
  // Memory structure that stores the low level policy rules for proxied calls.
  raw_ptr<PolicyGlobal> policy_;
  // The list of dlls to unload in the target process.
  std::vector<std::wstring> blocklisted_dlls_;
  // AppContainer to be applied to the target process.
  std::unique_ptr<AppContainerBase> app_container_;
};

class PolicyBase final : public TargetPolicy {
 public:
  PolicyBase(std::string_view key);
  ~PolicyBase() override;

  PolicyBase(const PolicyBase&) = delete;
  PolicyBase& operator=(const PolicyBase&) = delete;

  // TargetPolicy:
  TargetConfig* GetConfig() override;
  ResultCode SetStdoutHandle(HANDLE handle) override;
  ResultCode SetStderrHandle(HANDLE handle) override;
  void AddHandleToShare(HANDLE handle) override;
  void AddDelegateData(base::span<const uint8_t> data) override;

  // Creates a Job object with the level specified in a previous call to
  // SetJobLevel().
  ResultCode InitJob();

  // Returns the handle for this policy's job, or nullptr if the job is
  // not initialized.
  HANDLE GetJobHandle();

  // Returns true if a job is associated with this policy.
  bool HasJob();

  // Updates the active process limit on the policy's job to zero.
  // Has no effect if the job is allowed to spawn processes.
  ResultCode DropActiveProcessLimit();

  // Creates the two tokens with the levels specified in a previous call to
  // SetTokenLevel().
  ResultCode MakeTokens(std::optional<base::win::AccessToken>& initial,
                        std::optional<base::win::AccessToken>& lockdown);

  // Applies the sandbox to |target| and takes ownership. Internally a
  // call to TargetProcess::Init() is issued.
  ResultCode ApplyToTarget(std::unique_ptr<TargetProcess> target);

  EvalResult EvalPolicy(IpcTag service, CountedParameterSetBase* params);

  HANDLE GetStdoutHandle();
  HANDLE GetStderrHandle();

  // Returns the list of handles being shared with the target process.
  const base::HandlesToInheritVector& GetHandlesBeingShared();

 private:
  // BrokerServicesBase is allowed to set shared backing fields for TargetConfig.
  friend class sandbox::BrokerServicesBase;
  // Allow PolicyDiagnostic to snapshot PolicyBase for diagnostics.
  friend class PolicyDiagnostic;
  // Allow TopLevelDispatcher to know which IPC policy rules are necessary.
  friend class TopLevelDispatcher;

  // Sets up interceptions for a new target. This policy must own |target|.
  ResultCode SetupAllInterceptions(TargetProcess& target);

  // Sets up the handle closer for a new target. This policy must own |target|.
  bool SetupHandleCloser(TargetProcess& target);

  // TargetConfig will really be a ConfigBase.
  bool SetConfig(TargetConfig* config);

  // Gets possibly shared data or allocates if it did not already exist.
  ConfigBase* config();
  // Tag provided when this policy was created - mainly for debugging.
  std::string tag_;
  // Backing data if this object was created with an empty tag_.
  std::unique_ptr<ConfigBase> config_;
  // Shared backing data if this object will share fields with other policies.
  raw_ptr<ConfigBase> config_ptr_;

  // Remaining members are unique to this instance and will be configured every
  // time.

  // Returns nullopt if no data has been set, or a view into the data.
  std::optional<base::span<const uint8_t>> delegate_data_span();

  // The user-defined global policy settings.
  HANDLE stdout_handle_;
  HANDLE stderr_handle_;
  // An opaque blob of data the delegate uses to prime any pre-sandbox hooks.
  std::unique_ptr<const std::vector<uint8_t>> delegate_data_;

  std::unique_ptr<Dispatcher> dispatcher_;

  // Contains the list of handles being shared with the target process.
  // This list contains handles other than the stderr/stdout handles which are
  // shared with the target at times.
  base::HandlesToInheritVector handles_to_share_;
  Job job_;

  // The policy takes ownership of a target as it is applied to it.
  std::unique_ptr<TargetProcess> target_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SANDBOX_POLICY_BASE_H_
