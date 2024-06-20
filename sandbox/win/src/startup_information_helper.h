// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_STARTUP_INFORMATION_HELPER_H_
#define SANDBOX_WIN_SRC_STARTUP_INFORMATION_HELPER_H_

#include <Windows.h>

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/win/startup_information.h"
#include "sandbox/win/src/app_container_base.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/security_capabilities.h"

namespace sandbox {
using base::win::StartupInformation;

// Wraps base::win::StartupInformation and allows some querying of what is
// set. This is specialized for the dance between
// BrokerServices::SpawnTarget() and TargetProcess::Create().
class StartupInformationHelper {
 public:
  StartupInformationHelper();
  ~StartupInformationHelper();

  // Adds flags to the |dwFlags| field.
  void UpdateFlags(DWORD flags);
  // Sets |lpDesktop|. If |desktop| is empty, sets as nullptr.
  void SetDesktop(std::wstring desktop);
  // Creates PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY based on |flags|.
  void SetMitigations(MitigationFlags flags);
  // Creates PROC_THREAD_ATTRIBUTE_CHILD_PROCESS_POLICY if |restrict| is true.
  void SetRestrictChildProcessCreation(bool restrict);
  // Sets stdout and stderr handles. Also allows these to be inherited into
  // the child process. Should be called once only.
  void SetStdHandles(HANDLE stdout_handle, HANDLE stderr_handle);
  // Ignores duplicate or invalid handles.
  void AddInheritedHandle(HANDLE handle);
  // Create PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES and
  //        PROC_THREAD_ATTRIBUTE_ALL_APPLICATION_PACKAGES_POLICY
  // based on |container|. |container| should be valid.
  void SetAppContainer(AppContainer* container);
  // Creates PROC_THREAD_ATTRIBUTE_JOB_LIST with |job_handle|.
  void AddJobToAssociate(HANDLE job_handle);

  // Will one or more jobs be associated via the wrapped StartupInformation.
  bool HasJobsToAssociate() { return !job_handle_list_.empty(); }
  // Have handles been provided for secure inheritance?
  bool ShouldInheritHandles() { return inherit_handles_; }

  // Compiles fields into PROC_THREAD_ attributes and populates startup
  // information. Must be called before GetStartupInformation().
  bool BuildStartupInformation();

  // Sets whether or not the process created using this startup information will
  // have its environment filtered.
  void SetFilterEnvironment(bool filter);

  // Obtains whether or not the environment for the process created with this
  // startup information should be filtered.
  bool IsEnvironmentFiltered();

  // Gets wrapped object, valid once BuildStartupInformation() has been called.
  base::win::StartupInformation* GetStartupInformation() {
    return &startup_info_;
  }

 private:
  void operator=(const StartupInformationHelper&) = delete;
  StartupInformationHelper(const StartupInformationHelper&) = delete;

  DWORD CountAttributes();

  // Fields that are not passed into CreateProcessAsUserW().
  // This can only be true if security_capabilities_ is also initialized.
  bool enable_low_privilege_app_container_ = false;
  bool restrict_child_process_creation_ = false;
  HANDLE stdout_handle_ = INVALID_HANDLE_VALUE;
  HANDLE stderr_handle_ = INVALID_HANDLE_VALUE;
  bool inherit_handles_ = false;
  bool filter_environment_ = false;
  size_t mitigations_size_ = 0;

  // startup_info_.startup_info() is passed to CreateProcessAsUserW().
  StartupInformation startup_info_;

  // These need to have the same lifetime as startup_info_.startup_info();
  std::wstring desktop_;
  DWORD64 mitigations_[2]{};
  COMPONENT_FILTER component_filter_{};
  DWORD child_process_creation_ = 0;
  DWORD all_applications_package_policy_ = 0;
  std::vector<HANDLE> inherited_handle_list_;
  std::vector<HANDLE> job_handle_list_;
  std::unique_ptr<SecurityCapabilities> security_capabilities_;
};
}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_STARTUP_INFORMATION_HELPER_H_
