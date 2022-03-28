// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sandbox_policy_base.h"

#include <sddl.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/logging.h"
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
#include "sandbox/win/src/sandbox_utils.h"
#include "sandbox/win/src/security_capabilities.h"
#include "sandbox/win/src/signed_policy.h"
#include "sandbox/win/src/target_process.h"
#include "sandbox/win/src/top_level_dispatcher.h"
#include "sandbox/win/src/window.h"

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

}  // namespace

namespace sandbox {

SANDBOX_INTERCEPT IntegrityLevel g_shared_delayed_integrity_level;
SANDBOX_INTERCEPT MitigationFlags g_shared_delayed_mitigations;

// Initializes static members. alternate_desktop_handle_ is a desktop on
// alternate_winstation_handle_, alternate_desktop_local_winstation_handle_ is a
// desktop on the same winstation as the parent process.
HWINSTA PolicyBase::alternate_winstation_handle_ = nullptr;
HDESK PolicyBase::alternate_desktop_handle_ = nullptr;
HDESK PolicyBase::alternate_desktop_local_winstation_handle_ = nullptr;
IntegrityLevel PolicyBase::alternate_desktop_integrity_level_label_ =
    INTEGRITY_LEVEL_SYSTEM;
IntegrityLevel
    PolicyBase::alternate_desktop_local_winstation_integrity_level_label_ =
        INTEGRITY_LEVEL_SYSTEM;

PolicyBase::PolicyBase()
    : lockdown_level_(USER_LOCKDOWN),
      initial_level_(USER_LOCKDOWN),
      job_level_(JobLevel::kLockdown),
      ui_exceptions_(0),
      memory_limit_(0),
      use_alternate_desktop_(false),
      use_alternate_winstation_(false),
      relaxed_interceptions_(true),
      stdout_handle_(INVALID_HANDLE_VALUE),
      stderr_handle_(INVALID_HANDLE_VALUE),
      integrity_level_(INTEGRITY_LEVEL_LAST),
      delayed_integrity_level_(INTEGRITY_LEVEL_LAST),
      mitigations_(0),
      delayed_mitigations_(0),
      is_csrss_connected_(true),
      policy_maker_(nullptr),
      policy_(nullptr),
      lockdown_default_dacl_(false),
      add_restricting_random_sid_(false),
      effective_token_(nullptr),
      allow_no_sandbox_job_(false),
      job_() {
  dispatcher_ = std::make_unique<TopLevelDispatcher>(this);
}

PolicyBase::~PolicyBase() {
  delete policy_maker_;
  delete policy_;
}

ResultCode PolicyBase::SetTokenLevel(TokenLevel initial, TokenLevel lockdown) {
  if (initial < lockdown) {
    return SBOX_ERROR_BAD_PARAMS;
  }
  initial_level_ = initial;
  lockdown_level_ = lockdown;
  return SBOX_ALL_OK;
}

TokenLevel PolicyBase::GetInitialTokenLevel() const {
  return initial_level_;
}

TokenLevel PolicyBase::GetLockdownTokenLevel() const {
  return lockdown_level_;
}

ResultCode PolicyBase::SetJobLevel(JobLevel job_level, uint32_t ui_exceptions) {
  // Cannot set this after the job has been initialized.
  if (job_.IsValid())
    return SBOX_ERROR_BAD_PARAMS;
  if (memory_limit_ && job_level == JobLevel::kNone) {
    return SBOX_ERROR_BAD_PARAMS;
  }
  job_level_ = job_level;
  ui_exceptions_ = ui_exceptions;
  return SBOX_ALL_OK;
}

JobLevel PolicyBase::GetJobLevel() const {
  return job_level_;
}

ResultCode PolicyBase::SetJobMemoryLimit(size_t memory_limit) {
  memory_limit_ = memory_limit;
  return SBOX_ALL_OK;
}

ResultCode PolicyBase::SetAlternateDesktop(bool alternate_winstation) {
  use_alternate_desktop_ = true;
  use_alternate_winstation_ = alternate_winstation;
  return CreateAlternateDesktop(alternate_winstation);
}

std::wstring PolicyBase::GetAlternateDesktop() const {
  // No alternate desktop or winstation. Return an empty string.
  if (!use_alternate_desktop_ && !use_alternate_winstation_) {
    return std::wstring();
  }

  if (use_alternate_winstation_) {
    // The desktop and winstation should have been created by now.
    // If we hit this scenario, it means that the user ignored the failure
    // during SetAlternateDesktop, so we ignore it here too.
    if (!alternate_desktop_handle_ || !alternate_winstation_handle_)
      return std::wstring();

    return GetFullDesktopName(alternate_winstation_handle_,
                              alternate_desktop_handle_);
  }

  if (!alternate_desktop_local_winstation_handle_)
    return std::wstring();

  return GetFullDesktopName(nullptr,
                            alternate_desktop_local_winstation_handle_);
}

ResultCode PolicyBase::CreateAlternateDesktop(bool alternate_winstation) {
  if (alternate_winstation) {
    // Check if it's already created.
    if (alternate_winstation_handle_ && alternate_desktop_handle_)
      return SBOX_ALL_OK;

    DCHECK(!alternate_winstation_handle_);
    // Create the window station.
    ResultCode result = CreateAltWindowStation(&alternate_winstation_handle_);
    if (SBOX_ALL_OK != result)
      return result;

    // Verify that everything is fine.
    if (!alternate_winstation_handle_ ||
        base::win::GetWindowObjectName(alternate_winstation_handle_).empty())
      return SBOX_ERROR_CANNOT_CREATE_DESKTOP;

    // Create the destkop.
    result = CreateAltDesktop(alternate_winstation_handle_,
                              &alternate_desktop_handle_);
    if (SBOX_ALL_OK != result)
      return result;

    // Verify that everything is fine.
    if (!alternate_desktop_handle_ ||
        base::win::GetWindowObjectName(alternate_desktop_handle_).empty()) {
      return SBOX_ERROR_CANNOT_CREATE_DESKTOP;
    }
  } else {
    // Check if it already exists.
    if (alternate_desktop_local_winstation_handle_)
      return SBOX_ALL_OK;

    // Create the destkop.
    ResultCode result =
        CreateAltDesktop(nullptr, &alternate_desktop_local_winstation_handle_);
    if (SBOX_ALL_OK != result)
      return result;

    // Verify that everything is fine.
    if (!alternate_desktop_local_winstation_handle_ ||
        base::win::GetWindowObjectName(
            alternate_desktop_local_winstation_handle_)
            .empty()) {
      return SBOX_ERROR_CANNOT_CREATE_DESKTOP;
    }
  }

  return SBOX_ALL_OK;
}

void PolicyBase::DestroyAlternateDesktop() {
  if (use_alternate_winstation_) {
    if (alternate_desktop_handle_) {
      ::CloseDesktop(alternate_desktop_handle_);
      alternate_desktop_handle_ = nullptr;
    }

    if (alternate_winstation_handle_) {
      ::CloseWindowStation(alternate_winstation_handle_);
      alternate_winstation_handle_ = nullptr;
    }
  } else {
    if (alternate_desktop_local_winstation_handle_) {
      ::CloseDesktop(alternate_desktop_local_winstation_handle_);
      alternate_desktop_local_winstation_handle_ = nullptr;
    }
  }
}

ResultCode PolicyBase::SetIntegrityLevel(IntegrityLevel integrity_level) {
  if (app_container_)
    return SBOX_ERROR_BAD_PARAMS;
  integrity_level_ = integrity_level;
  return SBOX_ALL_OK;
}

IntegrityLevel PolicyBase::GetIntegrityLevel() const {
  return integrity_level_;
}

ResultCode PolicyBase::SetDelayedIntegrityLevel(
    IntegrityLevel integrity_level) {
  delayed_integrity_level_ = integrity_level;
  return SBOX_ALL_OK;
}

ResultCode PolicyBase::SetLowBox(const wchar_t* sid) {
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

ResultCode PolicyBase::SetProcessMitigations(MitigationFlags flags) {
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

MitigationFlags PolicyBase::GetProcessMitigations() {
  return mitigations_;
}

ResultCode PolicyBase::SetDelayedProcessMitigations(MitigationFlags flags) {
  if (!CanSetProcessMitigationsPostStartup(flags))
    return SBOX_ERROR_BAD_PARAMS;
  delayed_mitigations_ = flags;
  return SBOX_ALL_OK;
}

MitigationFlags PolicyBase::GetDelayedProcessMitigations() const {
  return delayed_mitigations_;
}

void PolicyBase::SetStrictInterceptions() {
  relaxed_interceptions_ = false;
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

ResultCode PolicyBase::AddRule(SubSystem subsystem,
                               Semantics semantics,
                               const wchar_t* pattern) {
  ResultCode result = AddRuleInternal(subsystem, semantics, pattern);
  LOG_IF(ERROR, result != SBOX_ALL_OK)
      << "Failed to add sandbox rule."
      << " error = " << result << ", subsystem = " << subsystem
      << ", semantics = " << semantics << ", pattern = '" << pattern << "'";
  return result;
}

ResultCode PolicyBase::AddDllToUnload(const wchar_t* dll_name) {
  blocklisted_dlls_.push_back(dll_name);
  return SBOX_ALL_OK;
}

ResultCode PolicyBase::AddKernelObjectToClose(const wchar_t* handle_type,
                                              const wchar_t* handle_name) {
  return handle_closer_.AddHandle(handle_type, handle_name);
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

void PolicyBase::SetLockdownDefaultDacl() {
  lockdown_default_dacl_ = true;
}

void PolicyBase::AddRestrictingRandomSid() {
  add_restricting_random_sid_ = true;
}

const base::HandlesToInheritVector& PolicyBase::GetHandlesBeingShared() {
  return handles_to_share_;
}

ResultCode PolicyBase::InitJob() {
  if (job_.IsValid())
    return SBOX_ERROR_BAD_PARAMS;

  if (job_level_ == JobLevel::kNone)
    return SBOX_ALL_OK;

  // Create the Windows job object.
  DWORD result = job_.Init(job_level_, nullptr, ui_exceptions_, memory_limit_);
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

  if (job_level_ >= JobLevel::kInteractive)
    return SBOX_ALL_OK;

  if (ERROR_SUCCESS != job_.SetActiveProcessLimit(0))
    return SBOX_ERROR_CANNOT_UPDATE_JOB_PROCESS_LIMIT;

  return SBOX_ALL_OK;
}

ResultCode PolicyBase::MakeTokens(base::win::ScopedHandle* initial,
                                  base::win::ScopedHandle* lockdown,
                                  base::win::ScopedHandle* lowbox) {
  absl::optional<base::win::Sid> random_sid =
      add_restricting_random_sid_ ? base::win::Sid::GenerateRandomSid()
                                  : absl::nullopt;
  if (add_restricting_random_sid_ && !random_sid)
    return SBOX_ERROR_CANNOT_CREATE_RESTRICTED_TOKEN;

  // Create the 'naked' token. This will be the permanent token associated
  // with the process and therefore with any thread that is not impersonating.
  DWORD result = CreateRestrictedToken(
      effective_token_, lockdown_level_, integrity_level_, PRIMARY,
      lockdown_default_dacl_, random_sid, lockdown);
  if (ERROR_SUCCESS != result)
    return SBOX_ERROR_CANNOT_CREATE_RESTRICTED_TOKEN;

  // If we're launching on the alternate desktop we need to make sure the
  // integrity label on the object is no higher than the sandboxed process's
  // integrity level. So, we lower the label on the desktop process if it's
  // not already low enough for our process.
  if (use_alternate_desktop_ && integrity_level_ != INTEGRITY_LEVEL_LAST) {
    // Integrity label enum is reversed (higher level is a lower value).
    static_assert(INTEGRITY_LEVEL_SYSTEM < INTEGRITY_LEVEL_UNTRUSTED,
                  "Integrity level ordering reversed.");
    HDESK desktop_handle = nullptr;
    IntegrityLevel desktop_integrity_level_label;
    if (use_alternate_winstation_) {
      desktop_handle = alternate_desktop_handle_;
      desktop_integrity_level_label = alternate_desktop_integrity_level_label_;
    } else {
      desktop_handle = alternate_desktop_local_winstation_handle_;
      desktop_integrity_level_label =
          alternate_desktop_local_winstation_integrity_level_label_;
    }
    // If the desktop_handle hasn't been created for any reason, skip this.
    if (desktop_handle && desktop_integrity_level_label < integrity_level_) {
      result = SetObjectIntegrityLabel(
          desktop_handle, SecurityObjectType::kWindow, 0, integrity_level_);
      if (ERROR_SUCCESS != result)
        return SBOX_ERROR_CANNOT_SET_DESKTOP_INTEGRITY;

      if (use_alternate_winstation_) {
        alternate_desktop_integrity_level_label_ = integrity_level_;
      } else {
        alternate_desktop_local_winstation_integrity_level_label_ =
            integrity_level_;
      }
    }
  }

  if (app_container_ &&
      app_container_->GetAppContainerType() == AppContainerType::kLowbox) {
    ResultCode result_code = app_container_->BuildLowBoxToken(lowbox, lockdown);

    if (result_code != SBOX_ALL_OK)
      return result_code;
  }

  // Create the 'better' token. We use this token as the one that the main
  // thread uses when booting up the process. It should contain most of
  // what we need (before reaching main( ))
  result = CreateRestrictedToken(effective_token_, initial_level_,
                                 integrity_level_, IMPERSONATION,
                                 lockdown_default_dacl_, random_sid, initial);
  if (ERROR_SUCCESS != result)
    return SBOX_ERROR_CANNOT_CREATE_RESTRICTED_IMP_TOKEN;

  return SBOX_ALL_OK;
}

ResultCode PolicyBase::ApplyToTarget(std::unique_ptr<TargetProcess> target) {
  if (target_)
    return SBOX_ERROR_UNEXPECTED_CALL;

  if (policy_) {
    if (!policy_maker_->Done())
      return SBOX_ERROR_NO_SPACE;
  }

  if (!ApplyProcessMitigationsToSuspendedProcess(target->Process(),
                                                 mitigations_)) {
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
  ret = target->Init(dispatcher_.get(), policy_, kIPCMemSize, kPolMemSize,
                     &win_error);

  if (ret != SBOX_ALL_OK)
    return ret;

  g_shared_delayed_integrity_level = delayed_integrity_level_;
  ret = target->TransferVariable("g_shared_delayed_integrity_level",
                                 &g_shared_delayed_integrity_level,
                                 sizeof(g_shared_delayed_integrity_level));
  g_shared_delayed_integrity_level = INTEGRITY_LEVEL_LAST;
  if (SBOX_ALL_OK != ret)
    return ret;

  // Add in delayed mitigations and pseudo-mitigations enforced at startup.
  g_shared_delayed_mitigations =
      delayed_mitigations_ | FilterPostStartupProcessMitigations(mitigations_);
  if (!CanSetProcessMitigationsPostStartup(g_shared_delayed_mitigations))
    return SBOX_ERROR_BAD_PARAMS;

  ret = target->TransferVariable("g_shared_delayed_mitigations",
                                 &g_shared_delayed_mitigations,
                                 sizeof(g_shared_delayed_mitigations));
  g_shared_delayed_mitigations = 0;
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

ResultCode PolicyBase::SetDisconnectCsrss() {
// Does not work on 32-bit, and the ASAN runtime falls over with the
// CreateThread EAT patch used when this is enabled.
// See https://crbug.com/783296#c27.
#if defined(_WIN64) && !defined(ADDRESS_SANITIZER)
  if (base::win::GetVersion() >= base::win::Version::WIN10) {
    is_csrss_connected_ = false;
    return AddKernelObjectToClose(L"ALPC Port", nullptr);
  }
#endif  // !defined(_WIN64)
  return SBOX_ALL_OK;
}

EvalResult PolicyBase::EvalPolicy(IpcTag service,
                                  CountedParameterSetBase* params) {
  if (policy_) {
    if (!policy_->entry[static_cast<size_t>(service)]) {
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
    PolicyProcessor pol_evaluator(policy_->entry[static_cast<size_t>(service)]);
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

ResultCode PolicyBase::AddAppContainerProfile(const wchar_t* package_name,
                                              bool create_profile) {
  if (!features::IsAppContainerSandboxSupported())
    return SBOX_ERROR_UNSUPPORTED;

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

scoped_refptr<AppContainer> PolicyBase::GetAppContainer() {
  return app_container_;
}

void PolicyBase::SetEffectiveToken(HANDLE token) {
  CHECK(token);
  effective_token_ = token;
}

ResultCode PolicyBase::SetupAllInterceptions(TargetProcess& target) {
  InterceptionManager manager(target, relaxed_interceptions_);

  if (policy_) {
    for (size_t i = 0; i < kMaxIpcTag; i++) {
      if (policy_->entry[i] &&
          !dispatcher_->SetupService(&manager, static_cast<IpcTag>(i)))
        return SBOX_ERROR_SETUP_INTERCEPTION_SERVICE;
    }
  }

  for (const std::wstring& dll : blocklisted_dlls_)
    manager.AddToUnloadModules(dll.c_str());

  if (!SetupBasicInterceptions(&manager, is_csrss_connected_))
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
  return handle_closer_.InitializeTargetHandles(target);
}

ResultCode PolicyBase::AddRuleInternal(SubSystem subsystem,
                                       Semantics semantics,
                                       const wchar_t* pattern) {
  if (!policy_) {
    policy_ = MakeBrokerPolicyMemory();
    DCHECK(policy_);
    policy_maker_ = new LowLevelPolicy(policy_);
    DCHECK(policy_maker_);
  }

  switch (subsystem) {
    case SUBSYS_FILES: {
      if (!FileSystemPolicy::GenerateRules(pattern, semantics, policy_maker_)) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }
    case SUBSYS_NAMED_PIPES: {
      if (!NamedPipePolicy::GenerateRules(pattern, semantics, policy_maker_)) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }
    case SUBSYS_WIN32K_LOCKDOWN: {
      // Win32k intercept rules only supported on Windows 8 and above. This must
      // match the version checks in process_mitigations.cc for consistency.
      if (base::win::GetVersion() >= base::win::Version::WIN8) {
        DCHECK_EQ(MITIGATION_WIN32K_DISABLE,
                  mitigations_ & MITIGATION_WIN32K_DISABLE)
            << "Enable MITIGATION_WIN32K_DISABLE before adding win32k policy "
               "rules.";
        if (!ProcessMitigationsWin32KLockdownPolicy::GenerateRules(
                pattern, semantics, policy_maker_)) {
          NOTREACHED();
          return SBOX_ERROR_BAD_PARAMS;
        }
      }
      break;
    }
    case SUBSYS_SIGNED_BINARY: {
      // Signed intercept rules only supported on Windows 10 TH2 and above. This
      // must match the version checks in process_mitigations.cc for
      // consistency.
      if (base::win::GetVersion() >= base::win::Version::WIN10_TH2) {
        DCHECK_EQ(MITIGATION_FORCE_MS_SIGNED_BINS,
                  mitigations_ & MITIGATION_FORCE_MS_SIGNED_BINS)
            << "Enable MITIGATION_FORCE_MS_SIGNED_BINS before adding signed "
               "policy rules.";
        if (!SignedPolicy::GenerateRules(pattern, semantics, policy_maker_)) {
          NOTREACHED();
          return SBOX_ERROR_BAD_PARAMS;
        }
      }
      break;
    }
    case SUBSYS_SOCKET: {
      // Only one semantic is supported for this subsystem; to allow socket
      // brokering.
      DCHECK_EQ(SOCKET_ALLOW_BROKER, semantics);
      // A very simple policy that just allows socket brokering if present.
      PolicyRule socket_policy(ASK_BROKER);
      policy_maker_->AddRule(IpcTag::WS2SOCKET, &socket_policy);
      break;
    }

    default: { return SBOX_ERROR_UNSUPPORTED; }
  }

  return SBOX_ALL_OK;
}

void PolicyBase::SetAllowNoSandboxJob() {
  allow_no_sandbox_job_ = true;
}

bool PolicyBase::GetAllowNoSandboxJob() {
  return allow_no_sandbox_job_;
}

}  // namespace sandbox
