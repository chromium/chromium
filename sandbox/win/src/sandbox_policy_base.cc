// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sandbox_policy_base.h"

#include <sddl.h>
#include <stddef.h>
#include <stdint.h>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/acl.h"
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
#include "sandbox/win/src/registry_policy.h"
#include "sandbox/win/src/restricted_token_utils.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_utils.h"
#include "sandbox/win/src/security_capabilities.h"
#include "sandbox/win/src/signed_policy.h"
#include "sandbox/win/src/sync_policy.h"
#include "sandbox/win/src/target_process.h"
#include "sandbox/win/src/top_level_dispatcher.h"
#include "sandbox/win/src/window.h"

namespace {

// The standard windows size for one memory page.
constexpr size_t kOneMemPage = 4096;
// The IPC and Policy shared memory sizes.
constexpr size_t kIPCMemSize = kOneMemPage * 2;
constexpr size_t kPolMemSize = kOneMemPage * 14;

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
    : ref_count(1),
      lockdown_level_(USER_LOCKDOWN),
      initial_level_(USER_LOCKDOWN),
      job_level_(JOB_LOCKDOWN),
      ui_exceptions_(0),
      memory_limit_(0),
      use_alternate_desktop_(false),
      use_alternate_winstation_(false),
      file_system_init_(false),
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
      lowbox_sid_(nullptr),
      lockdown_default_dacl_(false),
      enable_opm_redirection_(false),
      effective_token_(nullptr) {
  ::InitializeCriticalSection(&lock_);
  dispatcher_.reset(new TopLevelDispatcher(this));
}

PolicyBase::~PolicyBase() {
  TargetSet::iterator it;
  for (it = targets_.begin(); it != targets_.end(); ++it) {
    TargetProcess* target = (*it);
    delete target;
  }
  delete policy_maker_;
  delete policy_;

  if (lowbox_sid_)
    ::LocalFree(lowbox_sid_);

  ::DeleteCriticalSection(&lock_);
}

void PolicyBase::AddRef() {
  ::InterlockedIncrement(&ref_count);
}

void PolicyBase::Release() {
  if (0 == ::InterlockedDecrement(&ref_count))
    delete this;
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
  if (memory_limit_ && job_level == JOB_NONE) {
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
  if (app_container_profile_)
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
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return SBOX_ERROR_UNSUPPORTED;

  DCHECK(sid);
  if (lowbox_sid_ || app_container_profile_)
    return SBOX_ERROR_BAD_PARAMS;

  if (!ConvertStringSidToSid(sid, &lowbox_sid_))
    return SBOX_ERROR_INVALID_LOWBOX_SID;

  return SBOX_ALL_OK;
}

ResultCode PolicyBase::SetProcessMitigations(MitigationFlags flags) {
  // Prior to Win10 RS5 CreateProcess fails when AppContainer and mitigation
  // flags are enabled. Return an error on downlevel platforms if trying to
  // set new mitigations.
  if (app_container_profile_ &&
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

const base::HandlesToInheritVector& PolicyBase::GetHandlesBeingShared() {
  return handles_to_share_;
}

ResultCode PolicyBase::MakeJobObject(base::win::ScopedHandle* job) {
  if (job_level_ == JOB_NONE) {
    job->Close();
    return SBOX_ALL_OK;
  }

  // Create the windows job object.
  Job job_obj;
  DWORD result =
      job_obj.Init(job_level_, nullptr, ui_exceptions_, memory_limit_);
  if (ERROR_SUCCESS != result)
    return SBOX_ERROR_CANNOT_INIT_JOB;

  *job = job_obj.Take();
  return SBOX_ALL_OK;
}

ResultCode PolicyBase::MakeTokens(base::win::ScopedHandle* initial,
                                  base::win::ScopedHandle* lockdown,
                                  base::win::ScopedHandle* lowbox) {
  // Create the 'naked' token. This will be the permanent token associated
  // with the process and therefore with any thread that is not impersonating.
  DWORD result =
      CreateRestrictedToken(effective_token_, lockdown_level_, integrity_level_,
                            PRIMARY, lockdown_default_dacl_, lockdown);
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
      result =
          SetObjectIntegrityLabel(desktop_handle, SE_WINDOW_OBJECT, L"",
                                  GetIntegrityLevelString(integrity_level_));
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

  if (lowbox_sid_) {
    if (!lowbox_directory_.IsValid()) {
      result =
          CreateLowBoxObjectDirectory(lowbox_sid_, true, &lowbox_directory_);
      DCHECK(result == ERROR_SUCCESS);
    }

    // The order of handles isn't important in the CreateLowBoxToken call.
    // The kernel will maintain a reference to the object directory handle.
    HANDLE saved_handles[1] = {lowbox_directory_.Get()};
    DWORD saved_handles_count = lowbox_directory_.IsValid() ? 1 : 0;

    Sid package_sid(lowbox_sid_);
    SecurityCapabilities caps(package_sid);
    if (CreateLowBoxToken(lockdown->Get(), PRIMARY, &caps, saved_handles,
                          saved_handles_count, lowbox) != ERROR_SUCCESS) {
      return SBOX_ERROR_CANNOT_CREATE_LOWBOX_TOKEN;
    }

    if (!ReplacePackageSidInDacl(lowbox->Get(), SE_KERNEL_OBJECT, package_sid,
                                 TOKEN_ALL_ACCESS)) {
      return SBOX_ERROR_CANNOT_MODIFY_LOWBOX_TOKEN_DACL;
    }
  }

  // Create the 'better' token. We use this token as the one that the main
  // thread uses when booting up the process. It should contain most of
  // what we need (before reaching main( ))
  result =
      CreateRestrictedToken(effective_token_, initial_level_, integrity_level_,
                            IMPERSONATION, lockdown_default_dacl_, initial);
  if (ERROR_SUCCESS != result)
    return SBOX_ERROR_CANNOT_CREATE_RESTRICTED_IMP_TOKEN;

  return SBOX_ALL_OK;
}

PSID PolicyBase::GetLowBoxSid() const {
  return lowbox_sid_;
}

ResultCode PolicyBase::AddTarget(TargetProcess* target) {
  if (policy_)
    policy_maker_->Done();

  if (!ApplyProcessMitigationsToSuspendedProcess(target->Process(),
                                                 mitigations_)) {
    return SBOX_ERROR_APPLY_ASLR_MITIGATIONS;
  }

  ResultCode ret = SetupAllInterceptions(target);

  if (ret != SBOX_ALL_OK)
    return ret;

  if (!SetupHandleCloser(target))
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

  AutoLock lock(&lock_);
  targets_.push_back(target);
  return SBOX_ALL_OK;
}

bool PolicyBase::OnJobEmpty(HANDLE job) {
  AutoLock lock(&lock_);
  TargetSet::iterator it;
  for (it = targets_.begin(); it != targets_.end(); ++it) {
    if ((*it)->Job() == job)
      break;
  }
  if (it == targets_.end()) {
    return false;
  }
  TargetProcess* target = *it;
  targets_.erase(it);
  delete target;
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

void PolicyBase::SetEnableOPMRedirection() {
  enable_opm_redirection_ = true;
}

bool PolicyBase::GetEnableOPMRedirection() {
  return enable_opm_redirection_;
}

ResultCode PolicyBase::AddAppContainerProfile(const wchar_t* package_name,
                                              bool create_profile) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return SBOX_ERROR_UNSUPPORTED;

  DCHECK(package_name);
  if (lowbox_sid_ || app_container_profile_ ||
      integrity_level_ != INTEGRITY_LEVEL_LAST) {
    return SBOX_ERROR_BAD_PARAMS;
  }

  if (create_profile) {
    app_container_profile_ = AppContainerProfileBase::Create(
        package_name, L"Chrome Sandbox", L"Profile for Chrome Sandbox");
  } else {
    app_container_profile_ = AppContainerProfileBase::Open(package_name);
  }
  if (!app_container_profile_)
    return SBOX_ERROR_CREATE_APPCONTAINER_PROFILE;

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

scoped_refptr<AppContainerProfile> PolicyBase::GetAppContainerProfile() {
  return GetAppContainerProfileBase();
}

void PolicyBase::SetEffectiveToken(HANDLE token) {
  CHECK(token);
  effective_token_ = token;
}

scoped_refptr<AppContainerProfileBase>
PolicyBase::GetAppContainerProfileBase() {
  return app_container_profile_;
}

ResultCode PolicyBase::SetupAllInterceptions(TargetProcess* target) {
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

bool PolicyBase::SetupHandleCloser(TargetProcess* target) {
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
      if (!file_system_init_) {
        if (!FileSystemPolicy::SetInitialRules(policy_maker_))
          return SBOX_ERROR_BAD_PARAMS;
        file_system_init_ = true;
      }
      if (!FileSystemPolicy::GenerateRules(pattern, semantics, policy_maker_)) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }
    case SUBSYS_SYNC: {
      if (!SyncPolicy::GenerateRules(pattern, semantics, policy_maker_)) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }
    case SUBSYS_PROCESS: {
      if (lockdown_level_ < USER_INTERACTIVE &&
          TargetPolicy::PROCESS_ALL_EXEC == semantics) {
        // This is unsupported. This is a huge security risk to give full access
        // to a process handle.
        return SBOX_ERROR_UNSUPPORTED;
      }
      if (!ProcessPolicy::GenerateRules(pattern, semantics, policy_maker_)) {
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
    case SUBSYS_REGISTRY: {
      if (!RegistryPolicy::GenerateRules(pattern, semantics, policy_maker_)) {
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

    default: { return SBOX_ERROR_UNSUPPORTED; }
  }

  return SBOX_ALL_OK;
}

}  // namespace sandbox
