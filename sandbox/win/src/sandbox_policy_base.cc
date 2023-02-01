// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sandbox_policy_base.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/win/access_control_list.h"
#include "base/win/access_token.h"
#include "base/win/sid.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "sandbox/features.h"
#include "sandbox/win/src/acl.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/filesystem_policy.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/job.h"
#include "sandbox/win/src/named_pipe_policy.h"
#include "sandbox/win/src/policy_broker.h"
#include "sandbox/win/src/policy_engine_processor.h"
#include "sandbox/win/src/policy_low_level.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/process_mitigations_win32k_policy.h"
#include "sandbox/win/src/process_thread_policy.h"
#include "sandbox/win/src/restricted_token_utils.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_policy_diagnostic.h"
#include "sandbox/win/src/signed_policy.h"
#include "sandbox/win/src/target_process.h"
#include "sandbox/win/src/top_level_dispatcher.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace sandbox {
namespace {

// The standard windows size for one memory page.
constexpr size_t kOneMemPage = 4096;
// The IPC and Policy shared memory sizes.
constexpr size_t kIPCMemSize = kOneMemPage * 2;
constexpr size_t kPolMemSize = kOneMemPage * 6;

// Helper function to allocate space (on the heap) for policy.
sandbox::PolicyGlobal* MakeBrokerPolicyMemory() {
  const size_t kTotalPolicySz = kPolMemSize;
  sandbox::PolicyGlobal* policy =
      static_cast<sandbox::PolicyGlobal*>(::operator new(kTotalPolicySz));
  DCHECK(policy);
  memset(policy, 0, kTotalPolicySz);
  policy->data_size = kTotalPolicySz - sizeof(sandbox::PolicyGlobal);
  return policy;
}

bool IsInheritableHandle(HANDLE handle) {
  if (!handle)
    return false;
  if (handle == INVALID_HANDLE_VALUE)
    return false;
  // File handles (FILE_TYPE_DISK) and pipe handles are known to be
  // inheritable.  Console handles (FILE_TYPE_CHAR) are not
  // inheritable via PROC_THREAD_ATTRIBUTE_HANDLE_LIST.
  DWORD handle_type = GetFileType(handle);
  return handle_type == FILE_TYPE_DISK || handle_type == FILE_TYPE_PIPE;
}

bool ReplacePackageSidInDacl(HANDLE token,
                             const base::win::Sid& package_sid,
                             ACCESS_MASK access) {
  absl::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromHandle(
          token, base::win::SecurityObjectType::kKernel,
          DACL_SECURITY_INFORMATION);
  if (!sd) {
    return false;
  }

  if (!sd->SetDaclEntry(package_sid, base::win::SecurityAccessMode::kRevoke, 0,
                        0) ||
      !sd->SetDaclEntry(base::win::WellKnownSid::kAllApplicationPackages,
                        base::win::SecurityAccessMode::kGrant, access, 0)) {
    return false;
  }

  return sd->WriteToHandle(token, base::win::SecurityObjectType::kKernel,
                           DACL_SECURITY_INFORMATION);
}

}  // namespace

SANDBOX_INTERCEPT IntegrityLevel g_shared_delayed_integrity_level =
    INTEGRITY_LEVEL_LAST;
SANDBOX_INTERCEPT MitigationFlags g_shared_delayed_mitigations = 0;
SANDBOX_INTERCEPT MitigationFlags g_shared_startup_mitigations = 0;

ConfigBase::ConfigBase() noexcept
    :
#if DCHECK_IS_ON()
      creating_thread_id_(GetCurrentThreadId()),
#endif  // DCHECK_IS_ON()
      configured_(false),
      lockdown_level_(USER_LOCKDOWN),
      initial_level_(USER_LOCKDOWN),
      job_level_(JobLevel::kLockdown),
      integrity_level_(INTEGRITY_LEVEL_LAST),
      delayed_integrity_level_(INTEGRITY_LEVEL_LAST),
      mitigations_(0),
      delayed_mitigations_(0),
      add_restricting_random_sid_(false),
      lockdown_default_dacl_(false),
      allow_no_sandbox_job_(false),
      is_csrss_connected_(true),
      memory_limit_(0),
      ui_exceptions_(0),
      desktop_(Desktop::kDefault),
      filter_environment_(false),
      policy_maker_(nullptr),
      policy_(nullptr) {
}

bool ConfigBase::IsOnCreatingThread() const {
#if DCHECK_IS_ON()
  return GetCurrentThreadId() == creating_thread_id_;
#else  // DCHECK_IS_ON()
  NOTREACHED();
  return true;
#endif
}

bool ConfigBase::IsConfigured() const {
  return configured_;
}

bool ConfigBase::Freeze() {
  DCHECK(IsOnCreatingThread());
  DCHECK(!configured_);

  if (policy_) {
    if (!policy_maker_->Done())
      return false;
    // Policy maker is not needed once rules are compiled.
    policy_maker_.reset();
  }
  configured_ = true;
  return true;
}

PolicyGlobal* ConfigBase::policy() {
  DCHECK(configured_);
  return policy_;
}

std::vector<std::wstring>& ConfigBase::blocklisted_dlls() {
  DCHECK(configured_);
  return blocklisted_dlls_;
}

AppContainerBase* ConfigBase::app_container() {
  DCHECK(configured_);
  return app_container_.get();
}

ConfigBase::~ConfigBase() {
  // `policy_maker_` holds a raw_ptr on `policy_`, so we need to make sure it
  // gets destroyed first.
  policy_maker_.reset();
  policy_.ClearAndDelete();  // Allocated by MakeBrokerPolicyMemory.
}

ResultCode ConfigBase::AddRule(SubSystem subsystem,
                               Semantics semantics,
                               const wchar_t* pattern) {
  DCHECK(IsOnCreatingThread());
  DCHECK(!configured_);
  ResultCode result = AddRuleInternal(subsystem, semantics, pattern);
  LOG_IF(ERROR, result != SBOX_ALL_OK)
      << "Failed to add sandbox rule."
      << " error = " << result
      << ", subsystem = " << static_cast<int>(subsystem)
      << ", semantics = " << static_cast<int>(semantics) << ", pattern = '"
      << pattern << "'";
  return result;
}

ResultCode ConfigBase::AddRuleInternal(SubSystem subsystem,
                                       Semantics semantics,
                                       const wchar_t* pattern) {
  DCHECK(!configured_);
  if (!policy_) {
    policy_ = MakeBrokerPolicyMemory();
    policy_maker_ = std::make_unique<LowLevelPolicy>(policy_);
  }
  DCHECK(policy_maker_);

  switch (subsystem) {
    case SubSystem::kFiles: {
      if (!FileSystemPolicy::GenerateRules(pattern, semantics,
                                           policy_maker_.get())) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }
    case SubSystem::kNamedPipes: {
      if (!NamedPipePolicy::GenerateRules(pattern, semantics,
                                          policy_maker_.get())) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }
    case SubSystem::kWin32kLockdown: {
      DCHECK_EQ(MITIGATION_WIN32K_DISABLE,
                mitigations_ & MITIGATION_WIN32K_DISABLE)
          << "Enable MITIGATION_WIN32K_DISABLE before adding win32k policy "
             "rules.";
      if (!ProcessMitigationsWin32KLockdownPolicy::GenerateRules(
              pattern, semantics, policy_maker_.get())) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }
    case SubSystem::kSignedBinary: {
      // Signed intercept rules only supported on Windows 10 TH2 and above. This
      // must match the version checks in process_mitigations.cc for
      // consistency.
      if (base::win::GetVersion() >= base::win::Version::WIN10_TH2) {
        DCHECK_EQ(MITIGATION_FORCE_MS_SIGNED_BINS,
                  mitigations_ & MITIGATION_FORCE_MS_SIGNED_BINS)
            << "Enable MITIGATION_FORCE_MS_SIGNED_BINS before adding signed "
               "policy rules.";
        if (!SignedPolicy::GenerateRules(pattern, semantics,
                                         policy_maker_.get())) {
          NOTREACHED();
          return SBOX_ERROR_BAD_PARAMS;
        }
      }
      break;
    }
    case SubSystem::kProcess: {
      return SBOX_ERROR_UNSUPPORTED;
    }
  }

  return SBOX_ALL_OK;
}

void ConfigBase::AddDllToUnload(const wchar_t* dll_name) {
  blocklisted_dlls_.push_back(dll_name);
}

ResultCode ConfigBase::SetIntegrityLevel(IntegrityLevel integrity_level) {
  if (app_container_)
    return SBOX_ERROR_BAD_PARAMS;
  integrity_level_ = integrity_level;
  return SBOX_ALL_OK;
}

IntegrityLevel ConfigBase::GetIntegrityLevel() const {
  return integrity_level_;
}

void ConfigBase::SetDelayedIntegrityLevel(IntegrityLevel integrity_level) {
  delayed_integrity_level_ = integrity_level;
}

ResultCode ConfigBase::SetLowBox(const wchar_t* sid) {
  if (!features::IsAppContainerSandboxSupported())
    return SBOX_ERROR_UNSUPPORTED;

  DCHECK(sid);
  if (app_container_)
    return SBOX_ERROR_BAD_PARAMS;

  app_container_ = AppContainerBase::CreateLowbox(sid);
  if (!app_container_)
    return SBOX_ERROR_INVALID_LOWBOX_SID;

  return SBOX_ALL_OK;
}

ResultCode ConfigBase::SetProcessMitigations(MitigationFlags flags) {
  // Prior to Win10 RS5 CreateProcess fails when AppContainer and mitigation
  // flags are enabled. Return an error on downlevel platforms if trying to
  // set new mitigations.
  if (app_container_ &&
      base::win::GetVersion() < base::win::Version::WIN10_RS5) {
    return SBOX_ERROR_BAD_PARAMS;
  }
  if (!CanSetProcessMitigationsPreStartup(flags))
    return SBOX_ERROR_BAD_PARAMS;
  mitigations_ = flags;
  return SBOX_ALL_OK;
}

MitigationFlags ConfigBase::GetProcessMitigations() {
  return mitigations_;
}

ResultCode ConfigBase::SetDelayedProcessMitigations(MitigationFlags flags) {
  if (!CanSetProcessMitigationsPostStartup(flags))
    return SBOX_ERROR_BAD_PARAMS;
  delayed_mitigations_ = flags;
  return SBOX_ALL_OK;
}

MitigationFlags ConfigBase::GetDelayedProcessMitigations() const {
  return delayed_mitigations_;
}

void ConfigBase::AddRestrictingRandomSid() {
  add_restricting_random_sid_ = true;
}

void ConfigBase::SetLockdownDefaultDacl() {
  lockdown_default_dacl_ = true;
}

ResultCode ConfigBase::AddAppContainerProfile(const wchar_t* package_name,
                                              bool create_profile) {
  if (!features::IsAppContainerSandboxSupported())
    return SBOX_ERROR_UNSUPPORTED;

  DCHECK(!configured_);
  DCHECK(package_name);
  if (app_container_ || integrity_level_ != INTEGRITY_LEVEL_LAST) {
    return SBOX_ERROR_BAD_PARAMS;
  }

  if (create_profile) {
    app_container_ = AppContainerBase::CreateProfile(
        package_name, L"Chrome Sandbox", L"Profile for Chrome Sandbox");
  } else {
    app_container_ = AppContainerBase::Open(package_name);
  }
  if (!app_container_)
    return SBOX_ERROR_CREATE_APPCONTAINER;

  // A bug exists in CreateProcess where enabling an AppContainer profile and
  // passing a set of mitigation flags will generate ERROR_INVALID_PARAMETER.
  // Apply best efforts here and convert set mitigations to delayed mitigations.
  // This bug looks to have been fixed in Win10 RS5, so exit early if possible.
  if (base::win::GetVersion() >= base::win::Version::WIN10_RS5)
    return SBOX_ALL_OK;

  delayed_mitigations_ =
      mitigations_ & GetAllowedPostStartupProcessMitigations();
  DCHECK(delayed_mitigations_ ==
         (mitigations_ & ~(MITIGATION_SEHOP |
                           MITIGATION_RESTRICT_INDIRECT_BRANCH_PREDICTION)));
  mitigations_ = 0;
  return SBOX_ALL_OK;
}

scoped_refptr<AppContainer> ConfigBase::GetAppContainer() {
  return app_container_;
}

ResultCode ConfigBase::SetTokenLevel(TokenLevel initial, TokenLevel lockdown) {
  // Note: TokenLevel enum values increase as lockdown decreases.
  if (initial < lockdown) {
    return SBOX_ERROR_BAD_PARAMS;
  }
  initial_level_ = initial;
  lockdown_level_ = lockdown;
  return SBOX_ALL_OK;
}

TokenLevel ConfigBase::GetInitialTokenLevel() const {
  return initial_level_;
}

TokenLevel ConfigBase::GetLockdownTokenLevel() const {
  return lockdown_level_;
}

ResultCode ConfigBase::SetJobLevel(JobLevel job_level, uint32_t ui_exceptions) {
  if (memory_limit_ && job_level == JobLevel::kNone) {
    return SBOX_ERROR_BAD_PARAMS;
  }
  job_level_ = job_level;
  ui_exceptions_ = ui_exceptions;
  return SBOX_ALL_OK;
}

JobLevel ConfigBase::GetJobLevel() const {
  return job_level_;
}

void ConfigBase::SetJobMemoryLimit(size_t memory_limit) {
  memory_limit_ = memory_limit;
}

void ConfigBase::SetAllowNoSandboxJob() {
  allow_no_sandbox_job_ = true;
}

bool ConfigBase::GetAllowNoSandboxJob() {
  return allow_no_sandbox_job_;
}

ResultCode ConfigBase::AddKernelObjectToClose(const wchar_t* handle_type,
                                              const wchar_t* handle_name) {
  DCHECK(!configured_);
  if (!handle_closer_)
    handle_closer_ = std::make_unique<HandleCloser>();
  return handle_closer_->AddHandle(handle_type, handle_name);
}

ResultCode ConfigBase::SetDisconnectCsrss() {
// Does not work on 32-bit, and the ASAN runtime falls over with the
// CreateThread EAT patch used when this is enabled.
// See https://crbug.com/783296#c27.
#if defined(_WIN64) && !defined(ADDRESS_SANITIZER)
  is_csrss_connected_ = false;
  return AddKernelObjectToClose(L"ALPC Port", nullptr);
#else
  return SBOX_ALL_OK;
#endif  // !defined(_WIN64) || defined(ADDRESS_SANITIZER)
}

void ConfigBase::SetDesktop(Desktop desktop) {
  desktop_ = desktop;
}

void ConfigBase::SetFilterEnvironment(bool filter) {
  filter_environment_ = filter;
}

bool ConfigBase::GetEnvironmentFiltered() {
  return filter_environment_;
}

PolicyBase::PolicyBase(base::StringPiece tag)
    : tag_(tag),
      config_(),
      config_ptr_(nullptr),
      stdout_handle_(INVALID_HANDLE_VALUE),
      stderr_handle_(INVALID_HANDLE_VALUE),
      job_() {
  dispatcher_ = std::make_unique<TopLevelDispatcher>(this);
}

PolicyBase::~PolicyBase() {}

TargetConfig* PolicyBase::GetConfig() {
  return config();
}

ConfigBase* PolicyBase::config() {
  if (config_ptr_) {
    DCHECK(!config_);
    // Should have a tag if we are sharing backing configuration.
    DCHECK(!tag_.empty());
    return config_ptr_;
  }
  if (!config_) {
    DCHECK(tag_.empty());
    config_ = std::make_unique<ConfigBase>();
  }
  return config_.get();
}

bool PolicyBase::SetConfig(TargetConfig* config) {
  // Cannot call this method if we already own our memory.
  DCHECK(!config_);
  // Cannot call this method twice.
  DCHECK(!config_ptr_);
  // Must provide valid shared data region.
  DCHECK(config);
  // Should have a tag.
  DCHECK(!tag_.empty());
  config_ptr_ = static_cast<ConfigBase*>(config);
  return true;
}

ResultCode PolicyBase::SetStdoutHandle(HANDLE handle) {
  if (!IsInheritableHandle(handle))
    return SBOX_ERROR_BAD_PARAMS;
  stdout_handle_ = handle;
  return SBOX_ALL_OK;
}

ResultCode PolicyBase::SetStderrHandle(HANDLE handle) {
  if (!IsInheritableHandle(handle))
    return SBOX_ERROR_BAD_PARAMS;
  stderr_handle_ = handle;
  return SBOX_ALL_OK;
}

void PolicyBase::AddHandleToShare(HANDLE handle) {
  CHECK(handle);
  CHECK_NE(handle, INVALID_HANDLE_VALUE);

  // Ensure the handle can be inherited.
  bool result =
      SetHandleInformation(handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
  PCHECK(result);

  handles_to_share_.push_back(handle);
}

const base::HandlesToInheritVector& PolicyBase::GetHandlesBeingShared() {
  return handles_to_share_;
}

ResultCode PolicyBase::InitJob() {
  if (job_.IsValid())
    return SBOX_ERROR_BAD_PARAMS;

  if (config()->GetJobLevel() == JobLevel::kNone)
    return SBOX_ALL_OK;

  // Create the Windows job object.
  DWORD result = job_.Init(config()->GetJobLevel(), config()->ui_exceptions(),
                           config()->memory_limit());
  if (ERROR_SUCCESS != result)
    return SBOX_ERROR_CANNOT_INIT_JOB;

  return SBOX_ALL_OK;
}

HANDLE PolicyBase::GetJobHandle() {
  return job_.GetHandle();
}

bool PolicyBase::HasJob() {
  return job_.IsValid();
}

ResultCode PolicyBase::DropActiveProcessLimit() {
  if (!job_.IsValid())
    return SBOX_ERROR_BAD_PARAMS;

  if (config()->GetJobLevel() >= JobLevel::kInteractive)
    return SBOX_ALL_OK;

  if (ERROR_SUCCESS != job_.SetActiveProcessLimit(0))
    return SBOX_ERROR_CANNOT_UPDATE_JOB_PROCESS_LIMIT;

  return SBOX_ALL_OK;
}

ResultCode PolicyBase::MakeTokens(
    absl::optional<base::win::AccessToken>& initial,
    absl::optional<base::win::AccessToken>& lockdown) {
  absl::optional<base::win::Sid> random_sid;
  if (config()->add_restricting_random_sid()) {
    random_sid = base::win::Sid::GenerateRandomSid();
  }

  IntegrityLevel integrity_level = config()->integrity_level();
  bool lockdown_default_dacl = config()->lockdown_default_dacl();
  // Create the 'naked' token. This will be the permanent token associated
  // with the process and therefore with any thread that is not impersonating.
  absl::optional<base::win::AccessToken> primary = CreateRestrictedToken(
      config()->GetLockdownTokenLevel(), integrity_level, TokenType::kPrimary,
      lockdown_default_dacl, random_sid);
  if (!primary) {
    return SBOX_ERROR_CANNOT_CREATE_RESTRICTED_TOKEN;
  }

  AppContainerBase* app_container = config()->app_container();
  if (app_container &&
      app_container->GetAppContainerType() == AppContainerType::kLowbox) {
    // Build the lowbox lockdown (primary) token.
    primary = app_container->BuildPrimaryToken(*primary);
    if (!primary) {
      return SBOX_ERROR_CANNOT_CREATE_LOWBOX_TOKEN;
    }

    if (!ReplacePackageSidInDacl(primary->get(), app_container->GetPackageSid(),
                                 TOKEN_ALL_ACCESS)) {
      return SBOX_ERROR_CANNOT_MODIFY_LOWBOX_TOKEN_DACL;
    }
  }

  lockdown = std::move(*primary);

  // Create the 'better' token. We use this token as the one that the main
  // thread uses when booting up the process. It should contain most of
  // what we need (before reaching main( ))
  absl::optional<base::win::AccessToken> impersonation = CreateRestrictedToken(
      config()->GetInitialTokenLevel(), integrity_level,
      TokenType::kImpersonation, lockdown_default_dacl, random_sid);
  if (!impersonation) {
    return SBOX_ERROR_CANNOT_CREATE_RESTRICTED_IMP_TOKEN;
  }

  if (app_container) {
    impersonation = app_container->BuildImpersonationToken(*impersonation);
    if (!impersonation) {
      return SBOX_ERROR_CANNOT_CREATE_LOWBOX_IMPERSONATION_TOKEN;
    }
  }

  initial = std::move(*impersonation);

  return SBOX_ALL_OK;
}

ResultCode PolicyBase::ApplyToTarget(std::unique_ptr<TargetProcess> target) {
  if (target_)
    return SBOX_ERROR_UNEXPECTED_CALL;
  // Policy rules are compiled when the underlying ConfigBase is frozen.
  DCHECK(config()->IsConfigured());

  if (!ApplyProcessMitigationsToSuspendedProcess(
          target->Process(), config()->GetProcessMitigations())) {
    return SBOX_ERROR_APPLY_ASLR_MITIGATIONS;
  }

  ResultCode ret = SetupAllInterceptions(*target);

  if (ret != SBOX_ALL_OK)
    return ret;

  if (!SetupHandleCloser(*target))
    return SBOX_ERROR_SETUP_HANDLE_CLOSER;

  DWORD win_error = ERROR_SUCCESS;
  // Initialize the sandbox infrastructure for the target.
  // TODO(wfh) do something with win_error code here.
  ret = target->Init(dispatcher_.get(), config()->policy(), kIPCMemSize,
                     kPolMemSize, &win_error);

  if (ret != SBOX_ALL_OK)
    return ret;

  g_shared_delayed_integrity_level = config()->delayed_integrity_level();
  ret = target->TransferVariable("g_shared_delayed_integrity_level",
                                 &g_shared_delayed_integrity_level,
                                 sizeof(g_shared_delayed_integrity_level));
  g_shared_delayed_integrity_level = INTEGRITY_LEVEL_LAST;
  if (SBOX_ALL_OK != ret)
    return ret;

  // Add in delayed mitigations and pseudo-mitigations enforced at startup.
  g_shared_delayed_mitigations =
      config()->GetDelayedProcessMitigations() |
      FilterPostStartupProcessMitigations(config()->GetProcessMitigations());
  if (!CanSetProcessMitigationsPostStartup(g_shared_delayed_mitigations))
    return SBOX_ERROR_BAD_PARAMS;

  ret = target->TransferVariable("g_shared_delayed_mitigations",
                                 &g_shared_delayed_mitigations,
                                 sizeof(g_shared_delayed_mitigations));
  g_shared_delayed_mitigations = 0;
  if (SBOX_ALL_OK != ret)
    return ret;

  g_shared_startup_mitigations = config()->GetProcessMitigations();
  ret = target->TransferVariable("g_shared_startup_mitigations",
                                 &g_shared_startup_mitigations,
                                 sizeof(g_shared_startup_mitigations));
  g_shared_startup_mitigations = 0;
  if (SBOX_ALL_OK != ret)
    return ret;

  target_ = std::move(target);
  return SBOX_ALL_OK;
}

// Can only be called if a job was associated with this policy.
bool PolicyBase::OnJobEmpty() {
  target_.reset();
  return true;
}

bool PolicyBase::OnProcessFinished(DWORD process_id) {
  if (target_->ProcessId() == process_id)
    target_.reset();
  return true;
}

EvalResult PolicyBase::EvalPolicy(IpcTag service,
                                  CountedParameterSetBase* params) {
  PolicyGlobal* policy = config()->policy();
  if (policy) {
    if (!policy->entry[static_cast<size_t>(service)]) {
      // There is no policy for this particular service. This is not a big
      // deal.
      return DENY_ACCESS;
    }
    for (size_t i = 0; i < params->count; i++) {
      if (!params->parameters[i].IsValid()) {
        NOTREACHED();
        return SIGNAL_ALARM;
      }
    }
    PolicyProcessor pol_evaluator(policy->entry[static_cast<size_t>(service)]);
    PolicyResult result =
        pol_evaluator.Evaluate(kShortEval, params->parameters, params->count);
    if (POLICY_MATCH == result)
      return pol_evaluator.GetAction();

    DCHECK(POLICY_ERROR != result);
  }

  return DENY_ACCESS;
}

HANDLE PolicyBase::GetStdoutHandle() {
  return stdout_handle_;
}

HANDLE PolicyBase::GetStderrHandle() {
  return stderr_handle_;
}

ResultCode PolicyBase::SetupAllInterceptions(TargetProcess& target) {
  InterceptionManager manager(target);
  PolicyGlobal* policy = config()->policy();
  if (policy) {
    for (size_t i = 0; i < kMaxIpcTag; i++) {
      if (policy->entry[i] &&
          !dispatcher_->SetupService(&manager, static_cast<IpcTag>(i)))
        return SBOX_ERROR_SETUP_INTERCEPTION_SERVICE;
    }
  }

  for (const std::wstring& dll : config()->blocklisted_dlls())
    manager.AddToUnloadModules(dll.c_str());

  if (!SetupBasicInterceptions(&manager, config()->is_csrss_connected()))
    return SBOX_ERROR_SETUP_BASIC_INTERCEPTIONS;

  ResultCode rc = manager.InitializeInterceptions();
  if (rc != SBOX_ALL_OK)
    return rc;

  // Finally, setup imports on the target so the interceptions can work.
  if (!SetupNtdllImports(target))
    return SBOX_ERROR_SETUP_NTDLL_IMPORTS;

  return SBOX_ALL_OK;
}

bool PolicyBase::SetupHandleCloser(TargetProcess& target) {
  auto* handle_closer = config()->handle_closer();
  if (!handle_closer)
    return true;
  return handle_closer->InitializeTargetHandles(target);
}

}  // namespace sandbox
