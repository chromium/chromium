// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_mitigations_win32k_policy.h"

#include <stddef.h>

#include "sandbox/win/src/process_mitigations_win32k_interception.h"

namespace sandbox {

namespace {

// Define GUIDs for OPM APIs
const GUID DXGKMDT_OPM_GET_CONNECTOR_TYPE = {
    0x81d0bfd5,
    0x6afe,
    0x48c2,
    {0x99, 0xc0, 0x95, 0xa0, 0x8f, 0x97, 0xc5, 0xda}};
const GUID DXGKMDT_OPM_GET_SUPPORTED_PROTECTION_TYPES = {
    0x38f2a801,
    0x9a6c,
    0x48bb,
    {0x91, 0x07, 0xb6, 0x69, 0x6e, 0x6f, 0x17, 0x97}};
const GUID DXGKMDT_OPM_GET_VIRTUAL_PROTECTION_LEVEL = {
    0xb2075857,
    0x3eda,
    0x4d5d,
    {0x88, 0xdb, 0x74, 0x8f, 0x8c, 0x1a, 0x05, 0x49}};
const GUID DXGKMDT_OPM_GET_ACTUAL_PROTECTION_LEVEL = {
    0x1957210a,
    0x7766,
    0x452a,
    {0xb9, 0x9a, 0xd2, 0x7a, 0xed, 0x54, 0xf0, 0x3a}};
const GUID DXGKMDT_OPM_SET_PROTECTION_LEVEL = {
    0x9bb9327c,
    0x4eb5,
    0x4727,
    {0x9f, 0x00, 0xb4, 0x2b, 0x09, 0x19, 0xc0, 0xda}};

void StringToUnicodeString(PUNICODE_STRING unicode_string,
                           const std::wstring& device_name) {
  static RtlInitUnicodeStringFunction RtlInitUnicodeString;
  if (!RtlInitUnicodeString) {
    HMODULE ntdll = ::GetModuleHandle(kNtdllName);
    RtlInitUnicodeString = reinterpret_cast<RtlInitUnicodeStringFunction>(
        GetProcAddress(ntdll, "RtlInitUnicodeString"));
  }
  RtlInitUnicodeString(unicode_string, device_name.c_str());
}

struct MonitorListState {
  HMONITOR* monitor_list;
  uint32_t monitor_list_size;
  uint32_t monitor_list_pos;
};

BOOL CALLBACK DisplayMonitorEnumProc(HMONITOR monitor,
                                     HDC hdc_monitor,
                                     LPRECT rect_monitor,
                                     LPARAM data) {
  MonitorListState* state = reinterpret_cast<MonitorListState*>(data);
  if (state->monitor_list_pos >= state->monitor_list_size)
    return false;
  state->monitor_list[state->monitor_list_pos++] = monitor;
  return true;
}

template <typename T>
T GetExportedFunc(const wchar_t* libname, const char* name) {
  OverrideForTestFunction test_override =
      ProcessMitigationsWin32KLockdownPolicy::GetOverrideForTestCallback();
  if (test_override)
    return reinterpret_cast<T>(test_override(name));

  static T func = nullptr;
  if (!func) {
    func =
        reinterpret_cast<T>(::GetProcAddress(::GetModuleHandle(libname), name));
    DCHECK(!!func);
  }
  return func;
}

#define GDIFUNC(name) GetExportedFunc<name##Function>(L"gdi32.dll", #name)
#define USERFUNC(name) GetExportedFunc<name##Function>(L"user32.dll", #name)

struct ValidateMonitorParams {
  HMONITOR monitor;
  std::wstring device_name;
  bool result;
};

bool GetMonitorDeviceName(HMONITOR monitor, std::wstring* device_name) {
  MONITORINFOEXW monitor_info = {};
  monitor_info.cbSize = sizeof(monitor_info);
  if (!USERFUNC(GetMonitorInfoW)(monitor, &monitor_info))
    return false;
  if (monitor_info.szDevice[CCHDEVICENAME - 1] != 0)
    return false;
  *device_name = monitor_info.szDevice;
  return true;
}

BOOL CALLBACK ValidateMonitorEnumProc(HMONITOR monitor,
                                      HDC,
                                      LPRECT,
                                      LPARAM data) {
  ValidateMonitorParams* valid_params =
      reinterpret_cast<ValidateMonitorParams*>(data);
  std::wstring device_name;
  bool result = false;
  if (valid_params->device_name.empty()) {
    result = monitor == valid_params->monitor;
  } else if (GetMonitorDeviceName(monitor, &device_name)) {
    result = device_name == valid_params->device_name;
  }
  valid_params->result = result;
  if (!result)
    return true;
  return false;
}

bool IsValidMonitorOrDeviceName(HMONITOR monitor, const wchar_t* device_name) {
  ValidateMonitorParams params = {};
  params.monitor = monitor;
  if (device_name)
    params.device_name = device_name;
  USERFUNC(EnumDisplayMonitors)
  (nullptr, nullptr, ValidateMonitorEnumProc,
   reinterpret_cast<LPARAM>(&params));
  return params.result;
}

}  // namespace

OverrideForTestFunction
    ProcessMitigationsWin32KLockdownPolicy::override_callback_;

bool ProcessMitigationsWin32KLockdownPolicy::GenerateRules(
    const wchar_t* name,
    TargetPolicy::Semantics semantics,
    LowLevelPolicy* policy) {
  PolicyRule rule(FAKE_SUCCESS);
  if (!policy->AddRule(IpcTag::GDI_GDIDLLINITIALIZE, &rule))
    return false;
  if (!policy->AddRule(IpcTag::GDI_GETSTOCKOBJECT, &rule))
    return false;
  if (!policy->AddRule(IpcTag::USER_REGISTERCLASSW, &rule))
    return false;
  if (semantics != TargetPolicy::IMPLEMENT_OPM_APIS)
    return true;
  if (!policy->AddRule(IpcTag::USER_ENUMDISPLAYMONITORS, &rule))
    return false;
  if (!policy->AddRule(IpcTag::USER_ENUMDISPLAYDEVICES, &rule))
    return false;
  if (!policy->AddRule(IpcTag::USER_GETMONITORINFO, &rule))
    return false;
  if (!policy->AddRule(IpcTag::GDI_CREATEOPMPROTECTEDOUTPUTS, &rule))
    return false;
  if (!policy->AddRule(IpcTag::GDI_GETCERTIFICATE, &rule))
    return false;
  if (!policy->AddRule(IpcTag::GDI_GETCERTIFICATESIZE, &rule))
    return false;
  if (!policy->AddRule(IpcTag::GDI_DESTROYOPMPROTECTEDOUTPUT, &rule))
    return false;
  if (!policy->AddRule(IpcTag::GDI_CONFIGUREOPMPROTECTEDOUTPUT, &rule))
    return false;
  if (!policy->AddRule(IpcTag::GDI_GETOPMINFORMATION, &rule))
    return false;
  if (!policy->AddRule(IpcTag::GDI_GETOPMRANDOMNUMBER, &rule))
    return false;
  if (!policy->AddRule(IpcTag::GDI_GETSUGGESTEDOPMPROTECTEDOUTPUTARRAYSIZE,
                       &rule))
    return false;
  if (!policy->AddRule(IpcTag::GDI_SETOPMSIGNINGKEYANDSEQUENCENUMBERS, &rule))
    return false;
  return true;
}

uint32_t ProcessMitigationsWin32KLockdownPolicy::EnumDisplayMonitorsAction(
    const ClientInfo& client_info,
    HMONITOR* monitor_list,
    uint32_t monitor_list_size) {
  MonitorListState state = {monitor_list, monitor_list_size, 0};
  USERFUNC(EnumDisplayMonitors)
  (nullptr, nullptr, DisplayMonitorEnumProc, reinterpret_cast<LPARAM>(&state));
  return state.monitor_list_pos;
}

bool ProcessMitigationsWin32KLockdownPolicy::GetMonitorInfoAction(
    const ClientInfo& client_info,
    HMONITOR monitor,
    MONITORINFO* monitor_info_ptr) {
  if (!IsValidMonitorOrDeviceName(monitor, nullptr))
    return false;
  MONITORINFOEXW monitor_info = {};
  monitor_info.cbSize = sizeof(MONITORINFOEXW);

  bool success = USERFUNC(GetMonitorInfoW)(
      monitor, reinterpret_cast<MONITORINFO*>(&monitor_info));
  if (success)
    memcpy(monitor_info_ptr, &monitor_info, sizeof(monitor_info));
  return success;
}

NTSTATUS ProcessMitigationsWin32KLockdownPolicy::
    GetSuggestedOPMProtectedOutputArraySizeAction(
        const ClientInfo& client_info,
        const std::wstring& device_name,
        uint32_t* suggested_array_size) {
  if (!IsValidMonitorOrDeviceName(nullptr, device_name.c_str())) {
    return STATUS_ACCESS_DENIED;
  }
  UNICODE_STRING unicode_device_name;
  StringToUnicodeString(&unicode_device_name, device_name);
  DWORD suggested_array_size_dword = 0;
  NTSTATUS status = GDIFUNC(GetSuggestedOPMProtectedOutputArraySize)(
      &unicode_device_name, &suggested_array_size_dword);
  if (!status)
    *suggested_array_size = suggested_array_size_dword;
  return status;
}

NTSTATUS
ProcessMitigationsWin32KLockdownPolicy::CreateOPMProtectedOutputsAction(
    const ClientInfo& client_info,
    const std::wstring& device_name,
    HANDLE* protected_outputs,
    uint32_t array_input_size,
    uint32_t* array_output_size) {
  if (!IsValidMonitorOrDeviceName(nullptr, device_name.c_str())) {
    return STATUS_ACCESS_DENIED;
  }

  UNICODE_STRING unicode_device_name;
  StringToUnicodeString(&unicode_device_name, device_name);
  DWORD output_size = 0;

  NTSTATUS status = GDIFUNC(CreateOPMProtectedOutputs)(
      &unicode_device_name, DXGKMDT_OPM_VOS_OPM_SEMANTICS, array_input_size,
      &output_size,
      reinterpret_cast<OPM_PROTECTED_OUTPUT_HANDLE*>(protected_outputs));
  if (!status)
    *array_output_size = output_size;
  return status;
}

NTSTATUS ProcessMitigationsWin32KLockdownPolicy::GetCertificateSizeAction(
    const ClientInfo& client_info,
    const std::wstring& device_name,
    uint32_t* cert_size) {
  if (!IsValidMonitorOrDeviceName(nullptr, device_name.c_str())) {
    return STATUS_ACCESS_DENIED;
  }
  UNICODE_STRING unicode_device_name;
  StringToUnicodeString(&unicode_device_name, device_name);

  return GDIFUNC(GetCertificateSize)(&unicode_device_name,
                                     DXGKMDT_OPM_CERTIFICATE,
                                     reinterpret_cast<DWORD*>(cert_size));
}

NTSTATUS ProcessMitigationsWin32KLockdownPolicy::GetCertificateAction(
    const ClientInfo& client_info,
    const std::wstring& device_name,
    BYTE* cert_data,
    uint32_t cert_size) {
  if (!IsValidMonitorOrDeviceName(nullptr, device_name.c_str())) {
    return STATUS_ACCESS_DENIED;
  }
  UNICODE_STRING unicode_device_name;
  StringToUnicodeString(&unicode_device_name, device_name);

  return GDIFUNC(GetCertificate)(&unicode_device_name, DXGKMDT_OPM_CERTIFICATE,
                                 cert_data, cert_size);
}

NTSTATUS
ProcessMitigationsWin32KLockdownPolicy::GetCertificateSizeByHandleAction(
    const ClientInfo& client_info,
    HANDLE protected_output,
    uint32_t* cert_size) {
  auto get_certificate_size_func = GDIFUNC(GetCertificateSizeByHandle);
  if (get_certificate_size_func) {
    return get_certificate_size_func(protected_output, DXGKMDT_OPM_CERTIFICATE,
                                     reinterpret_cast<DWORD*>(cert_size));
  }
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS ProcessMitigationsWin32KLockdownPolicy::GetCertificateByHandleAction(
    const ClientInfo& client_info,
    HANDLE protected_output,
    BYTE* cert_data,
    uint32_t cert_size) {
  auto get_certificate_func = GDIFUNC(GetCertificateByHandle);
  if (get_certificate_func) {
    return get_certificate_func(protected_output, DXGKMDT_OPM_CERTIFICATE,
                                cert_data, cert_size);
  }
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS ProcessMitigationsWin32KLockdownPolicy::GetOPMRandomNumberAction(
    const ClientInfo& client_info,
    HANDLE protected_output,
    void* random_number) {
  return GDIFUNC(GetOPMRandomNumber)(
      protected_output, static_cast<DXGKMDT_OPM_RANDOM_NUMBER*>(random_number));
}

NTSTATUS ProcessMitigationsWin32KLockdownPolicy::
    SetOPMSigningKeyAndSequenceNumbersAction(const ClientInfo& client_info,
                                             HANDLE protected_output,
                                             void* parameters) {
  return GDIFUNC(SetOPMSigningKeyAndSequenceNumbers)(
      protected_output,
      static_cast<DXGKMDT_OPM_ENCRYPTED_PARAMETERS*>(parameters));
}

NTSTATUS
ProcessMitigationsWin32KLockdownPolicy::ConfigureOPMProtectedOutputAction(
    const ClientInfo& client_info,
    HANDLE protected_output,
    void* parameters_ptr) {
  DXGKMDT_OPM_CONFIGURE_PARAMETERS parameters;
  memcpy(&parameters, parameters_ptr, sizeof(parameters));
  if (parameters.guidSetting != DXGKMDT_OPM_SET_PROTECTION_LEVEL ||
      parameters.cbParametersSize !=
          sizeof(DXGKMDT_OPM_SET_PROTECTION_LEVEL_PARAMETERS)) {
    return STATUS_INVALID_PARAMETER;
  }

  DXGKMDT_OPM_SET_PROTECTION_LEVEL_PARAMETERS prot_level;
  memcpy(&prot_level, parameters.abParameters, sizeof(prot_level));
  if (prot_level.Reserved || prot_level.Reserved2)
    return STATUS_INVALID_PARAMETER;

  if (prot_level.ulProtectionType != DXGKMDT_OPM_PROTECTION_TYPE_HDCP &&
      prot_level.ulProtectionType != DXGKMDT_OPM_PROTECTION_TYPE_DPCP) {
    return STATUS_INVALID_PARAMETER;
  }

  // Protection levels are same for HDCP and DPCP.
  if (prot_level.ulProtectionLevel != DXGKMDT_OPM_HDCP_OFF &&
      prot_level.ulProtectionLevel != DXGKMDT_OPM_HDCP_ON) {
    return STATUS_INVALID_PARAMETER;
  }

  return GDIFUNC(ConfigureOPMProtectedOutput)(protected_output, &parameters, 0,
                                              nullptr);
}

NTSTATUS ProcessMitigationsWin32KLockdownPolicy::GetOPMInformationAction(
    const ClientInfo& client_info,
    HANDLE protected_output,
    void* parameters_ptr,
    void* requested_info_ptr) {
  DXGKMDT_OPM_GET_INFO_PARAMETERS parameters;
  memcpy(&parameters, parameters_ptr, sizeof(parameters));

  bool valid_parameters = false;
  // Validate sizes based on the type being requested.
  if ((parameters.guidInformation == DXGKMDT_OPM_GET_CONNECTOR_TYPE ||
       parameters.guidInformation ==
           DXGKMDT_OPM_GET_SUPPORTED_PROTECTION_TYPES) &&
      parameters.cbParametersSize == 0) {
    valid_parameters = true;
  } else if ((parameters.guidInformation ==
                  DXGKMDT_OPM_GET_VIRTUAL_PROTECTION_LEVEL ||
              parameters.guidInformation ==
                  DXGKMDT_OPM_GET_ACTUAL_PROTECTION_LEVEL) &&
             parameters.cbParametersSize == sizeof(uint32_t)) {
    uint32_t param_value;
    memcpy(&param_value, parameters.abParameters, sizeof(param_value));
    if (param_value == DXGKMDT_OPM_PROTECTION_TYPE_HDCP ||
        param_value == DXGKMDT_OPM_PROTECTION_TYPE_DPCP) {
      valid_parameters = true;
    }
  }
  if (!valid_parameters)
    return STATUS_INVALID_PARAMETER;
  DXGKMDT_OPM_REQUESTED_INFORMATION requested_info = {};
  NTSTATUS status = GDIFUNC(GetOPMInformation)(protected_output, &parameters,
                                               &requested_info);
  if (!status)
    memcpy(requested_info_ptr, &requested_info, sizeof(requested_info));

  return status;
}

NTSTATUS
ProcessMitigationsWin32KLockdownPolicy::DestroyOPMProtectedOutputAction(
    HANDLE protected_output) {
  return GDIFUNC(DestroyOPMProtectedOutput)(protected_output);
}

void ProcessMitigationsWin32KLockdownPolicy::SetOverrideForTestCallback(
    OverrideForTestFunction callback) {
  override_callback_ = callback;
}

OverrideForTestFunction
ProcessMitigationsWin32KLockdownPolicy::GetOverrideForTestCallback() {
  return override_callback_;
}

}  // namespace sandbox
