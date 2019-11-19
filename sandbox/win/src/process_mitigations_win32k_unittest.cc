// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_mitigations.h"

#include <windows.h>

#include <d3d9.h>
#include <initguid.h>
#include <opmapi.h>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/process_mitigations_win32k_policy.h"
#include "sandbox/win/tests/common/controller.h"
#include "sandbox/win/tests/integration_tests/integration_tests_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

//------------------------------------------------------------------------------
// Internal Functions
//------------------------------------------------------------------------------

BOOL CALLBACK MonitorEnumCallback(HMONITOR monitor,
                                  HDC hdc_monitor,
                                  LPRECT rect_monitor,
                                  LPARAM data) {
  std::map<HMONITOR, std::wstring>& monitors =
      *reinterpret_cast<std::map<HMONITOR, std::wstring>*>(data);
  MONITORINFOEXW monitor_info = {};
  monitor_info.cbSize = sizeof(monitor_info);

  if (!::GetMonitorInfoW(monitor,
                         reinterpret_cast<MONITORINFO*>(&monitor_info)))
    return false;
  monitors[monitor] = monitor_info.szDevice;
  return true;
}

std::map<HMONITOR, std::wstring> EnumerateMonitors() {
  std::map<HMONITOR, std::wstring> result;
  ::EnumDisplayMonitors(nullptr, nullptr, MonitorEnumCallback,
                        reinterpret_cast<LPARAM>(&result));
  return result;
}

#define HMONITOR_ENTRY(monitor)                 \
  result[reinterpret_cast<HMONITOR>(monitor)] = \
      base::StringPrintf(L"\\\\.\\DISPLAY%X", monitor)

std::map<HMONITOR, std::wstring> GetTestMonitors() {
  std::map<HMONITOR, std::wstring> result;

  HMONITOR_ENTRY(0x11111111);
  HMONITOR_ENTRY(0x22222222);
  HMONITOR_ENTRY(0x44444444);
  HMONITOR_ENTRY(0x88888888);
  return result;
}

std::wstring UnicodeStringToString(PUNICODE_STRING name) {
  return std::wstring(name->Buffer,
                      name->Buffer + (name->Length / sizeof(name->Buffer[0])));
}

// Returns an index 1, 2, 4 or 8 depening on the device. 0 on error.
DWORD GetTestDeviceMonitorIndex(PUNICODE_STRING device_name) {
  std::wstring name = UnicodeStringToString(device_name);
  std::map<HMONITOR, std::wstring> monitors = GetTestMonitors();
  for (const auto& monitor : monitors) {
    if (name == monitor.second)
      return static_cast<DWORD>(reinterpret_cast<uintptr_t>(monitor.first)) &
             0xF;
  }
  return 0;
}

NTSTATUS WINAPI GetSuggestedOPMProtectedOutputArraySizeTest(
    PUNICODE_STRING device_name,
    DWORD* suggested_output_array_size) {
  DWORD monitor = GetTestDeviceMonitorIndex(device_name);
  if (!monitor)
    return STATUS_OBJECT_NAME_NOT_FOUND;
  *suggested_output_array_size = monitor;
  return STATUS_SUCCESS;
}

NTSTATUS WINAPI
CreateOPMProtectedOutputsTest(PUNICODE_STRING device_name,
                              DXGKMDT_OPM_VIDEO_OUTPUT_SEMANTICS vos,
                              DWORD output_array_size,
                              DWORD* num_in_output_array,
                              OPM_PROTECTED_OUTPUT_HANDLE* output_array) {
  DWORD monitor = GetTestDeviceMonitorIndex(device_name);
  if (!monitor)
    return STATUS_OBJECT_NAME_NOT_FOUND;
  if (vos != DXGKMDT_OPM_VOS_OPM_SEMANTICS)
    return STATUS_INVALID_PARAMETER;
  if (output_array_size != monitor)
    return STATUS_INVALID_PARAMETER;
  *num_in_output_array = monitor - 1;
  for (DWORD index = 0; index < monitor - 1; ++index) {
    output_array[index] =
        reinterpret_cast<OPM_PROTECTED_OUTPUT_HANDLE>((monitor << 4) + index);
  }
  return STATUS_SUCCESS;
}

ULONG CalculateCertLength(DWORD monitor) {
  return (monitor * 0x800) + 0xabc;
}

NTSTATUS WINAPI GetCertificateTest(PUNICODE_STRING device_name,
                                   DXGKMDT_CERTIFICATE_TYPE certificate_type,
                                   BYTE* certificate,
                                   ULONG certificate_length) {
  DWORD monitor = GetTestDeviceMonitorIndex(device_name);
  if (!monitor)
    return STATUS_OBJECT_NAME_NOT_FOUND;
  if (certificate_type != DXGKMDT_OPM_CERTIFICATE)
    return STATUS_INVALID_PARAMETER;
  if (certificate_length != CalculateCertLength(monitor))
    return STATUS_INVALID_PARAMETER;
  memset(certificate, 'A' + monitor, certificate_length);
  return STATUS_SUCCESS;
}

NTSTATUS WINAPI
GetCertificateSizeTest(PUNICODE_STRING device_name,
                       DXGKMDT_CERTIFICATE_TYPE certificate_type,
                       ULONG* certificate_length) {
  DWORD monitor = GetTestDeviceMonitorIndex(device_name);
  if (!monitor)
    return STATUS_OBJECT_NAME_NOT_FOUND;
  if (certificate_type != DXGKMDT_OPM_CERTIFICATE)
    return STATUS_INVALID_PARAMETER;
  *certificate_length = CalculateCertLength(monitor);
  return STATUS_SUCCESS;
}

// Check for valid output handle and return the monitor index.
DWORD IsValidProtectedOutput(OPM_PROTECTED_OUTPUT_HANDLE protected_output) {
  uintptr_t handle = reinterpret_cast<uintptr_t>(protected_output);
  uintptr_t monitor = handle >> 4;
  uintptr_t index = handle & 0xF;
  switch (monitor) {
    case 1:
    case 2:
    case 4:
    case 8:
      break;
    default:
      return 0;
  }
  if (index >= (monitor - 1))
    return 0;
  return static_cast<DWORD>(monitor);
}

NTSTATUS WINAPI
GetCertificateByHandleTest(OPM_PROTECTED_OUTPUT_HANDLE protected_output,
                           DXGKMDT_CERTIFICATE_TYPE certificate_type,
                           BYTE* certificate,
                           ULONG certificate_length) {
  DWORD monitor = IsValidProtectedOutput(protected_output);
  if (!monitor)
    return STATUS_INVALID_HANDLE;
  if (certificate_type != DXGKMDT_OPM_CERTIFICATE)
    return STATUS_INVALID_PARAMETER;
  if (certificate_length != CalculateCertLength(monitor))
    return STATUS_INVALID_PARAMETER;
  memset(certificate, 'A' + monitor, certificate_length);
  return STATUS_SUCCESS;
}

NTSTATUS WINAPI
GetCertificateSizeByHandleTest(OPM_PROTECTED_OUTPUT_HANDLE protected_output,
                               DXGKMDT_CERTIFICATE_TYPE certificate_type,
                               ULONG* certificate_length) {
  DWORD monitor = IsValidProtectedOutput(protected_output);
  if (!monitor)
    return STATUS_INVALID_HANDLE;
  if (certificate_type != DXGKMDT_OPM_CERTIFICATE)
    return STATUS_INVALID_PARAMETER;
  *certificate_length = CalculateCertLength(monitor);
  return STATUS_SUCCESS;
}

NTSTATUS WINAPI
DestroyOPMProtectedOutputTest(OPM_PROTECTED_OUTPUT_HANDLE protected_output) {
  if (!IsValidProtectedOutput(protected_output))
    return STATUS_INVALID_HANDLE;
  return STATUS_SUCCESS;
}

NTSTATUS WINAPI ConfigureOPMProtectedOutputTest(
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    const DXGKMDT_OPM_CONFIGURE_PARAMETERS* parameters,
    ULONG additional_parameters_size,
    const BYTE* additional_parameters) {
  if (!IsValidProtectedOutput(protected_output))
    return STATUS_INVALID_HANDLE;
  if (additional_parameters && additional_parameters_size)
    return STATUS_INVALID_PARAMETER;
  return STATUS_SUCCESS;
}

NTSTATUS WINAPI GetOPMInformationTest(
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    const DXGKMDT_OPM_GET_INFO_PARAMETERS* parameters,
    DXGKMDT_OPM_REQUESTED_INFORMATION* requested_information) {
  DWORD monitor = IsValidProtectedOutput(protected_output);
  if (!monitor)
    return STATUS_INVALID_HANDLE;
  memset(requested_information, '0' + monitor,
         sizeof(DXGKMDT_OPM_REQUESTED_INFORMATION));
  return STATUS_SUCCESS;
}

NTSTATUS WINAPI
GetOPMRandomNumberTest(OPM_PROTECTED_OUTPUT_HANDLE protected_output,
                       DXGKMDT_OPM_RANDOM_NUMBER* random_number) {
  DWORD monitor = IsValidProtectedOutput(protected_output);
  if (!monitor)
    return STATUS_INVALID_HANDLE;
  memset(random_number->abRandomNumber, '!' + monitor,
         sizeof(random_number->abRandomNumber));
  return STATUS_SUCCESS;
}

NTSTATUS WINAPI SetOPMSigningKeyAndSequenceNumbersTest(
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    const DXGKMDT_OPM_ENCRYPTED_PARAMETERS* parameters) {
  DWORD monitor = IsValidProtectedOutput(protected_output);
  if (!monitor)
    return STATUS_INVALID_HANDLE;
  DXGKMDT_OPM_ENCRYPTED_PARAMETERS test_params = {};
  memset(test_params.abEncryptedParameters, 'a' + monitor,
         sizeof(test_params.abEncryptedParameters));
  if (memcmp(test_params.abEncryptedParameters,
             parameters->abEncryptedParameters,
             sizeof(test_params.abEncryptedParameters)) != 0)
    return STATUS_INVALID_PARAMETER;
  return STATUS_SUCCESS;
}

bool WINAPI EnumDisplayMonitorsTest(HDC hdc,
                                    LPCRECT clip_rect,
                                    MONITORENUMPROC enum_function,
                                    LPARAM data) {
  RECT rc = {};
  for (const auto& monitor : GetTestMonitors()) {
    if (!enum_function(monitor.first, hdc, &rc, data))
      return false;
  }
  return true;
}

bool WINAPI GetMonitorInfoWTest(HMONITOR monitor, LPMONITORINFO monitor_info) {
  std::map<HMONITOR, std::wstring> monitors = GetTestMonitors();
  if (monitor_info->cbSize != sizeof(MONITORINFO) &&
      monitor_info->cbSize != sizeof(MONITORINFOEXW))
    return false;
  auto it = monitors.find(monitor);
  if (it == monitors.end())
    return false;
  if (monitor_info->cbSize == sizeof(MONITORINFOEXW)) {
    MONITORINFOEXW* monitor_info_ex =
        reinterpret_cast<MONITORINFOEXW*>(monitor_info);
    size_t copy_size = (it->second.size() + 1) * sizeof(WCHAR);
    if (copy_size > sizeof(monitor_info_ex->szDevice) - sizeof(WCHAR))
      copy_size = sizeof(monitor_info_ex->szDevice) - sizeof(WCHAR);
    memset(monitor_info_ex->szDevice, 0, sizeof(monitor_info_ex->szDevice));
    memcpy(monitor_info_ex->szDevice, it->second.c_str(), copy_size);
  }
  return true;
}

#define RETURN_TEST_FUNC(n)                  \
  if (strcmp(name, #n) == 0) {               \
    return reinterpret_cast<void*>(n##Test); \
  }

void* FunctionOverrideForTest(const char* name) {
  RETURN_TEST_FUNC(GetSuggestedOPMProtectedOutputArraySize);
  RETURN_TEST_FUNC(CreateOPMProtectedOutputs);
  RETURN_TEST_FUNC(GetCertificate);
  RETURN_TEST_FUNC(GetCertificateSize);
  RETURN_TEST_FUNC(DestroyOPMProtectedOutput);
  RETURN_TEST_FUNC(ConfigureOPMProtectedOutput);
  RETURN_TEST_FUNC(GetOPMInformation);
  RETURN_TEST_FUNC(GetOPMRandomNumber);
  RETURN_TEST_FUNC(SetOPMSigningKeyAndSequenceNumbers);
  RETURN_TEST_FUNC(EnumDisplayMonitors);
  RETURN_TEST_FUNC(GetMonitorInfoW);
  RETURN_TEST_FUNC(GetCertificateByHandle);
  RETURN_TEST_FUNC(GetCertificateSizeByHandle);
  NOTREACHED();
  return nullptr;
}

bool RunTestsOnVideoOutputConfigure(uintptr_t monitor_index,
                                    IOPMVideoOutput* video_output) {
  OPM_CONFIGURE_PARAMETERS config_params = {};
  OPM_SET_PROTECTION_LEVEL_PARAMETERS* protection_level =
      reinterpret_cast<OPM_SET_PROTECTION_LEVEL_PARAMETERS*>(
          config_params.abParameters);
  protection_level->ulProtectionType = OPM_PROTECTION_TYPE_HDCP;
  protection_level->ulProtectionLevel = OPM_HDCP_ON;
  config_params.guidSetting = OPM_SET_PROTECTION_LEVEL;
  config_params.cbParametersSize = sizeof(OPM_SET_PROTECTION_LEVEL_PARAMETERS);
  HRESULT hr = video_output->Configure(&config_params, 0, nullptr);
  if (FAILED(hr))
    return false;
  protection_level->ulProtectionType = OPM_PROTECTION_TYPE_DPCP;
  hr = video_output->Configure(&config_params, 0, nullptr);
  if (FAILED(hr))
    return false;
  protection_level->ulProtectionLevel = OPM_HDCP_OFF;
  hr = video_output->Configure(&config_params, 0, nullptr);
  if (FAILED(hr))
    return false;
  BYTE dummy_byte = 0;
  hr = video_output->Configure(&config_params, 1, &dummy_byte);
  if (SUCCEEDED(hr))
    return false;
  protection_level->ulProtectionType = 0xFFFFFFFF;
  hr = video_output->Configure(&config_params, 0, nullptr);
  if (SUCCEEDED(hr))
    return false;
  // Invalid protection level test.
  protection_level->ulProtectionType = OPM_PROTECTION_TYPE_HDCP;
  protection_level->ulProtectionLevel = OPM_HDCP_ON + 1;
  hr = video_output->Configure(&config_params, 0, nullptr);
  if (SUCCEEDED(hr))
    return false;
  hr = video_output->Configure(&config_params, 0, nullptr);
  if (SUCCEEDED(hr))
    return false;
  config_params.guidSetting = OPM_SET_HDCP_SRM;
  OPM_SET_HDCP_SRM_PARAMETERS* srm_parameters =
      reinterpret_cast<OPM_SET_HDCP_SRM_PARAMETERS*>(
          config_params.abParameters);
  srm_parameters->ulSRMVersion = 1;
  config_params.cbParametersSize = sizeof(OPM_SET_HDCP_SRM_PARAMETERS);
  hr = video_output->Configure(&config_params, 0, nullptr);
  if (SUCCEEDED(hr))
    return false;
  return true;
}

bool RunTestsOnVideoOutputFinishInitialization(uintptr_t monitor_index,
                                               IOPMVideoOutput* video_output) {
  OPM_ENCRYPTED_INITIALIZATION_PARAMETERS init_params = {};
  memset(init_params.abEncryptedInitializationParameters,
         'a' + static_cast<DWORD>(monitor_index),
         sizeof(init_params.abEncryptedInitializationParameters));
  HRESULT hr = video_output->FinishInitialization(&init_params);
  if (FAILED(hr))
    return false;
  memset(init_params.abEncryptedInitializationParameters,
         'Z' + static_cast<DWORD>(monitor_index),
         sizeof(init_params.abEncryptedInitializationParameters));
  hr = video_output->FinishInitialization(&init_params);
  if (SUCCEEDED(hr))
    return false;
  return true;
}

bool RunTestsOnVideoOutputStartInitialization(uintptr_t monitor_index,
                                              IOPMVideoOutput* video_output) {
  OPM_RANDOM_NUMBER random_number = {};
  BYTE* certificate = nullptr;
  ULONG certificate_length = 0;

  HRESULT hr = video_output->StartInitialization(&random_number, &certificate,
                                                 &certificate_length);
  if (FAILED(hr))
    return false;

  if (certificate_length !=
      CalculateCertLength(static_cast<DWORD>(monitor_index)))
    return false;

  for (ULONG i = 0; i < certificate_length; ++i) {
    if (certificate[i] != 'A' + monitor_index)
      return false;
  }

  for (ULONG i = 0; i < sizeof(random_number.abRandomNumber); ++i) {
    if (random_number.abRandomNumber[i] != '!' + monitor_index)
      return false;
  }

  return true;
}

static bool SendSingleGetInfoRequest(uintptr_t monitor_index,
                                     IOPMVideoOutput* video_output,
                                     const GUID& request,
                                     ULONG data_length,
                                     void* data) {
  OPM_GET_INFO_PARAMETERS params = {};
  OPM_REQUESTED_INFORMATION requested_information = {};
  BYTE* requested_information_ptr =
      reinterpret_cast<BYTE*>(&requested_information);
  params.guidInformation = request;
  params.cbParametersSize = data_length;
  memcpy(params.abParameters, data, data_length);
  HRESULT hr = video_output->GetInformation(&params, &requested_information);
  if (FAILED(hr))
    return false;
  for (size_t i = 0; i < sizeof(OPM_REQUESTED_INFORMATION); ++i) {
    if (requested_information_ptr[i] != '0' + monitor_index)
      return false;
  }
  return true;
}

bool RunTestsOnVideoOutputGetInformation(uintptr_t monitor_index,
                                         IOPMVideoOutput* video_output) {
  ULONG dummy = 0;
  if (!SendSingleGetInfoRequest(monitor_index, video_output,
                                OPM_GET_CONNECTOR_TYPE, 0, nullptr)) {
    return false;
  }
  if (!SendSingleGetInfoRequest(monitor_index, video_output,
                                OPM_GET_SUPPORTED_PROTECTION_TYPES, 0,
                                nullptr)) {
    return false;
  }
  // These should fail due to invalid parameter sizes.
  if (SendSingleGetInfoRequest(monitor_index, video_output,
                               OPM_GET_CONNECTOR_TYPE, sizeof(dummy), &dummy)) {
    return false;
  }
  if (SendSingleGetInfoRequest(monitor_index, video_output,
                               OPM_GET_SUPPORTED_PROTECTION_TYPES,
                               sizeof(dummy), &dummy)) {
    return false;
  }
  ULONG protection_type = OPM_PROTECTION_TYPE_HDCP;
  if (!SendSingleGetInfoRequest(monitor_index, video_output,
                                OPM_GET_ACTUAL_PROTECTION_LEVEL,
                                sizeof(protection_type), &protection_type)) {
    return false;
  }
  protection_type = OPM_PROTECTION_TYPE_DPCP;
  if (!SendSingleGetInfoRequest(monitor_index, video_output,
                                OPM_GET_ACTUAL_PROTECTION_LEVEL,
                                sizeof(protection_type), &protection_type)) {
    return false;
  }
  // These should fail as unsupported or invalid parameters.
  protection_type = OPM_PROTECTION_TYPE_ACP;
  if (SendSingleGetInfoRequest(monitor_index, video_output,
                               OPM_GET_ACTUAL_PROTECTION_LEVEL,
                               sizeof(protection_type), &protection_type)) {
    return false;
  }
  if (SendSingleGetInfoRequest(monitor_index, video_output,
                               OPM_GET_ACTUAL_PROTECTION_LEVEL, 0, nullptr)) {
    return false;
  }
  protection_type = OPM_PROTECTION_TYPE_HDCP;
  if (!SendSingleGetInfoRequest(monitor_index, video_output,
                                OPM_GET_VIRTUAL_PROTECTION_LEVEL,
                                sizeof(protection_type), &protection_type)) {
    return false;
  }
  protection_type = OPM_PROTECTION_TYPE_DPCP;
  if (!SendSingleGetInfoRequest(monitor_index, video_output,
                                OPM_GET_VIRTUAL_PROTECTION_LEVEL,
                                sizeof(protection_type), &protection_type)) {
    return false;
  }
  // These should fail as unsupported or invalid parameters.
  protection_type = OPM_PROTECTION_TYPE_ACP;
  if (SendSingleGetInfoRequest(monitor_index, video_output,
                               OPM_GET_VIRTUAL_PROTECTION_LEVEL,
                               sizeof(protection_type), &protection_type)) {
    return false;
  }
  if (SendSingleGetInfoRequest(monitor_index, video_output,
                               OPM_GET_VIRTUAL_PROTECTION_LEVEL, 0, nullptr)) {
    return false;
  }
  // This should fail with unsupported request.
  if (SendSingleGetInfoRequest(monitor_index, video_output, OPM_GET_CODEC_INFO,
                               0, nullptr)) {
    return false;
  }
  return true;
}

int RunTestsOnVideoOutput(uintptr_t monitor_index,
                          IOPMVideoOutput* video_output) {
  if (!RunTestsOnVideoOutputStartInitialization(monitor_index, video_output))
    return sandbox::SBOX_TEST_FIRST_ERROR;

  if (!RunTestsOnVideoOutputFinishInitialization(monitor_index, video_output))
    return sandbox::SBOX_TEST_SECOND_ERROR;

  if (!RunTestsOnVideoOutputConfigure(monitor_index, video_output))
    return sandbox::SBOX_TEST_THIRD_ERROR;

  if (!RunTestsOnVideoOutputGetInformation(monitor_index, video_output))
    return sandbox::SBOX_TEST_FOURTH_ERROR;

  return sandbox::SBOX_TEST_SUCCEEDED;
}

}  // namespace

namespace sandbox {

//------------------------------------------------------------------------------
// Exported functions called by child test processes.
//------------------------------------------------------------------------------

SBOX_TESTS_COMMAND int CheckWin8MonitorsRedirection(int argc, wchar_t** argv) {
  std::map<HMONITOR, std::wstring> monitors = EnumerateMonitors();
  std::map<HMONITOR, std::wstring> monitors_to_test = GetTestMonitors();
  if (monitors.size() != monitors_to_test.size())
    return SBOX_TEST_FIRST_ERROR;

  for (const auto& monitor : monitors) {
    auto result = monitors_to_test.find(monitor.first);
    if (result == monitors_to_test.end())
      return SBOX_TEST_SECOND_ERROR;
    if (result->second != monitor.second)
      return SBOX_TEST_THIRD_ERROR;
  }
  return SBOX_TEST_SUCCEEDED;
}

SBOX_TESTS_COMMAND int CheckWin8MonitorInfo(int argc, wchar_t** argv) {
  std::map<HMONITOR, std::wstring> monitors_to_test = GetTestMonitors();
  MONITORINFO monitor_info = {};
  MONITORINFOEXW monitor_info_exw = {};
  MONITORINFOEXA monitor_info_exa = {};
  HMONITOR valid_monitor = monitors_to_test.begin()->first;
  std::wstring valid_device = monitors_to_test.begin()->second;
  monitor_info.cbSize = sizeof(MONITORINFO);
  if (!::GetMonitorInfoW(valid_monitor, &monitor_info))
    return SBOX_TEST_FIRST_ERROR;
  monitor_info.cbSize = sizeof(MONITORINFO);
  if (!::GetMonitorInfoA(valid_monitor, &monitor_info))
    return SBOX_TEST_SECOND_ERROR;
  monitor_info_exw.cbSize = sizeof(MONITORINFOEXW);
  if (!::GetMonitorInfoW(valid_monitor,
                         reinterpret_cast<MONITORINFO*>(&monitor_info_exw)) ||
      valid_device != monitor_info_exw.szDevice) {
    return SBOX_TEST_THIRD_ERROR;
  }
  monitor_info_exa.cbSize = sizeof(MONITORINFOEXA);
  if (!::GetMonitorInfoA(valid_monitor,
                         reinterpret_cast<MONITORINFO*>(&monitor_info_exa)) ||
      valid_device != base::UTF8ToWide(monitor_info_exa.szDevice)) {
    return SBOX_TEST_FOURTH_ERROR;
  }

  // Invalid size checks.
  monitor_info.cbSize = 0;
  if (::GetMonitorInfoW(valid_monitor, &monitor_info))
    return SBOX_TEST_FIFTH_ERROR;
  monitor_info.cbSize = 0x10000;
  if (::GetMonitorInfoW(valid_monitor, &monitor_info))
    return SBOX_TEST_SIXTH_ERROR;

  // Check that an invalid handle isn't accepted.
  HMONITOR invalid_monitor = reinterpret_cast<HMONITOR>(-1);
  monitor_info.cbSize = sizeof(MONITORINFO);
  if (::GetMonitorInfoW(invalid_monitor, &monitor_info))
    return SBOX_TEST_SEVENTH_ERROR;

  return SBOX_TEST_SUCCEEDED;
}

SBOX_TESTS_COMMAND int CheckWin8OPMApis(int argc, wchar_t** argv) {
  std::map<HMONITOR, std::wstring> monitors = GetTestMonitors();
  for (const auto& monitor : monitors) {
    ULONG output_count = 0;
    IOPMVideoOutput** outputs = nullptr;
    uintptr_t monitor_index = reinterpret_cast<uintptr_t>(monitor.first) & 0xF;
    HRESULT hr = OPMGetVideoOutputsFromHMONITOR(
        monitor.first, OPM_VOS_OPM_SEMANTICS, &output_count, &outputs);
    if (monitor_index > 4) {
      // These should fail because the certificate is too large.
      if (SUCCEEDED(hr))
        return SBOX_TEST_FIRST_ERROR;
      continue;
    }
    if (FAILED(hr))
      return SBOX_TEST_SECOND_ERROR;
    if (output_count != monitor_index - 1)
      return SBOX_TEST_THIRD_ERROR;
    for (ULONG output_index = 0; output_index < output_count; ++output_index) {
      int result = RunTestsOnVideoOutput(monitor_index, outputs[output_index]);
      outputs[output_index]->Release();
      if (result != SBOX_TEST_SUCCEEDED)
        return result;
    }
    ::CoTaskMemFree(outputs);
  }
  return SBOX_TEST_SUCCEEDED;
}

//------------------------------------------------------------------------------
// Exported Win32k Lockdown Tests
//------------------------------------------------------------------------------

// This test validates that setting the MITIGATION_WIN32K_DISABLE mitigation on
// the target process causes the launch to fail in process initialization.
// The test process itself links against user32/gdi32.
TEST(ProcessMitigationsWin32kTest, CheckWin8LockDownFailure) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  std::wstring test_policy_command = L"CheckPolicy ";
  test_policy_command += std::to_wstring(TESTPOLICY_WIN32K);

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_WIN32K_DISABLE),
            SBOX_ALL_OK);
  EXPECT_NE(SBOX_TEST_SUCCEEDED, runner.RunTest(test_policy_command.c_str()));
}

// This test validates that setting the MITIGATION_WIN32K_DISABLE mitigation
// along with the policy to fake user32 and gdi32 initialization successfully
// launches the target process.
// The test process itself links against user32/gdi32.

TEST(ProcessMitigationsWin32kTest, CheckWin8LockDownSuccess) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  std::wstring test_policy_command = L"CheckPolicy ";
  test_policy_command += std::to_wstring(TESTPOLICY_WIN32K);

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();
  ProcessMitigationsWin32KLockdownPolicy::SetOverrideForTestCallback(
      FunctionOverrideForTest);

  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_WIN32K_DISABLE),
            SBOX_ALL_OK);
  EXPECT_EQ(policy->AddRule(sandbox::TargetPolicy::SUBSYS_WIN32K_LOCKDOWN,
                            sandbox::TargetPolicy::FAKE_USER_GDI_INIT, nullptr),
            sandbox::SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_policy_command.c_str()));
  EXPECT_NE(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"CheckWin8MonitorsRedirection"));
  EXPECT_NE(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckWin8MonitorInfo"));
  EXPECT_NE(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckWin8OPMApis"));
}

// This test validates the even though we're running under win32k lockdown
// we can use the IPC redirection to enumerate the list of monitors.
// Flaky. https://crbug.com/840335
TEST(ProcessMitigationsWin32kTest, CheckWin8Redirection) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  std::wstring test_policy_command = L"CheckPolicy ";
  test_policy_command += std::to_wstring(TESTPOLICY_WIN32K);

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();
  ProcessMitigationsWin32KLockdownPolicy::SetOverrideForTestCallback(
      FunctionOverrideForTest);

  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_WIN32K_DISABLE),
            SBOX_ALL_OK);
  EXPECT_EQ(policy->AddRule(sandbox::TargetPolicy::SUBSYS_WIN32K_LOCKDOWN,
                            sandbox::TargetPolicy::IMPLEMENT_OPM_APIS, nullptr),
            sandbox::SBOX_ALL_OK);
  policy->SetEnableOPMRedirection();
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_policy_command.c_str()));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"CheckWin8MonitorsRedirection"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckWin8MonitorInfo"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckWin8OPMApis"));
}

}  // namespace sandbox
