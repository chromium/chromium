// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_PROCESS_MITIGATIONS_WIN32K_POLICY_H_
#define SANDBOX_SRC_PROCESS_MITIGATIONS_WIN32K_POLICY_H_

#include <string>

#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/policy_low_level.h"
#include "sandbox/win/src/sandbox_policy.h"

namespace sandbox {

// A callback function type to get a function for testing.
typedef void* (*OverrideForTestFunction)(const char* name);

// This class centralizes most of the knowledge related to the process
// mitigations Win32K lockdown policy.
class ProcessMitigationsWin32KLockdownPolicy {
 public:
  // Creates the required low-level policy rules to evaluate a high-level
  // policy rule for the Win32K process mitigation policy.
  // name is the object name, semantics is the desired semantics for the
  // open or create and policy is the policy generator to which the rules are
  // going to be added.
  static bool GenerateRules(const wchar_t* name,
                            TargetPolicy::Semantics semantics,
                            LowLevelPolicy* policy);

  static uint32_t EnumDisplayMonitorsAction(const ClientInfo& client_info,
                                            HMONITOR* monitor_list,
                                            uint32_t monitor_list_size);
  static bool GetMonitorInfoAction(const ClientInfo& client_info,
                                   HMONITOR monitor,
                                   MONITORINFO* monitor_info);

  static NTSTATUS GetSuggestedOPMProtectedOutputArraySizeAction(
      const ClientInfo& client_info,
      const std::wstring& device_name,
      uint32_t* suggested_array_size);

  static NTSTATUS CreateOPMProtectedOutputsAction(
      const ClientInfo& client_info,
      const std::wstring& device_name,
      HANDLE* protected_outputs,
      uint32_t array_input_size,
      uint32_t* array_output_size);

  static NTSTATUS GetCertificateSizeAction(const ClientInfo& client_info,
                                           const std::wstring& device_name,
                                           uint32_t* cert_size);
  static NTSTATUS GetCertificateAction(const ClientInfo& client_info,
                                       const std::wstring& device_name,
                                       BYTE* cert_data,
                                       uint32_t cert_size);
  static NTSTATUS GetCertificateSizeByHandleAction(
      const ClientInfo& client_info,
      HANDLE protected_output,
      uint32_t* cert_size);
  static NTSTATUS GetCertificateByHandleAction(const ClientInfo& client_info,
                                               HANDLE protected_output,
                                               BYTE* cert_data,
                                               uint32_t cert_size);
  static NTSTATUS GetOPMRandomNumberAction(const ClientInfo& client_info,
                                           HANDLE protected_output,
                                           void* random_number);
  static NTSTATUS SetOPMSigningKeyAndSequenceNumbersAction(
      const ClientInfo& client_info,
      HANDLE protected_output,
      void* parameters);
  static NTSTATUS ConfigureOPMProtectedOutputAction(
      const ClientInfo& client_info,
      HANDLE protected_output,
      void* parameters_ptr);
  static NTSTATUS GetOPMInformationAction(const ClientInfo& client_info,
                                          HANDLE protected_output,
                                          void* parameters_ptr,
                                          void* requested_information_ptr);
  static NTSTATUS DestroyOPMProtectedOutputAction(HANDLE protected_output);
  static void SetOverrideForTestCallback(OverrideForTestFunction callback);
  static OverrideForTestFunction GetOverrideForTestCallback();

 private:
  static OverrideForTestFunction override_callback_;
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_PROCESS_MITIGATIONS_WIN32K_POLICY_H_
