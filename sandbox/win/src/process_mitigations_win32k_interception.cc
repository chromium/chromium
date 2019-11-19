// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_mitigations_win32k_interception.h"

#include <algorithm>

#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/policy_target.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sharedmem_ipc_client.h"
#include "sandbox/win/src/target_services.h"

namespace sandbox {

namespace {

// Implement a simple shared memory class as we can't use the base one.
class ScopedSharedMemory {
 public:
  ScopedSharedMemory(uint32_t size) : memory_(nullptr) {
    handle_.Set(::CreateFileMapping(INVALID_HANDLE_VALUE, nullptr,
                                    PAGE_READWRITE | SEC_COMMIT, 0, size,
                                    nullptr));
    if (handle_.IsValid()) {
      memory_ = ::MapViewOfFile(handle_.Get(), FILE_MAP_READ | FILE_MAP_WRITE,
                                0, 0, size);
    }
  }
  ~ScopedSharedMemory() {
    if (memory_)
      ::UnmapViewOfFile(memory_);
  }

  void* handle() { return handle_.Get(); }
  void* memory() { return memory_; }
  bool IsValid() { return handle_.IsValid() && memory_; }

 private:
  base::win::ScopedHandle handle_;
  void* memory_;
};

void UnicodeStringToString(PUNICODE_STRING unicode_string,
                           std::wstring* result) {
  *result = std::wstring(
      unicode_string->Buffer,
      unicode_string->Buffer +
          (unicode_string->Length / sizeof(unicode_string->Buffer[0])));
}

bool CallMonitorInfo(HMONITOR monitor, MONITORINFOEXW* monitor_info_ptr) {
  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return false;

  void* ipc_memory = GetGlobalIPCMemory();
  if (!ipc_memory)
    return false;

  CrossCallReturn answer = {};
  SharedMemIPCClient ipc(ipc_memory);
  InOutCountedBuffer buffer(monitor_info_ptr, sizeof(*monitor_info_ptr));
  ResultCode code = CrossCall(ipc, IpcTag::USER_GETMONITORINFO,
                              static_cast<void*>(monitor), buffer, &answer);

  if (code != SBOX_ALL_OK)
    return false;

  if (answer.win32_result != ERROR_SUCCESS)
    return false;

  return true;
}

}  // namespace

BOOL WINAPI
TargetGdiDllInitialize(GdiDllInitializeFunction orig_gdi_dll_initialize,
                       HANDLE dll,
                       DWORD reason) {
  return true;
}

HGDIOBJ WINAPI
TargetGetStockObject(GetStockObjectFunction orig_get_stock_object, int object) {
  return nullptr;
}

ATOM WINAPI
TargetRegisterClassW(RegisterClassWFunction orig_register_class_function,
                     const WNDCLASS* wnd_class) {
  return true;
}

BOOL WINAPI TargetEnumDisplayMonitors(EnumDisplayMonitorsFunction,
                                      HDC hdc,
                                      LPCRECT lprcClip,
                                      MONITORENUMPROC lpfnEnum,
                                      LPARAM dwData) {
  if (!lpfnEnum || hdc || lprcClip) {
    return false;
  }

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return false;

  void* ipc_memory = GetGlobalIPCMemory();
  if (!ipc_memory)
    return false;

  CrossCallReturn answer = {0};
  answer.nt_status = 0;
  EnumMonitorsResult result = {};
  InOutCountedBuffer result_buffer(&result, sizeof(result));
  SharedMemIPCClient ipc(ipc_memory);
  ResultCode code =
      CrossCall(ipc, IpcTag::USER_ENUMDISPLAYMONITORS, result_buffer, &answer);

  if (code != SBOX_ALL_OK)
    return false;

  if (answer.win32_result)
    return false;

  if (result.monitor_count > kMaxEnumMonitors)
    return false;

  for (uint32_t monitor_pos = 0; monitor_pos < result.monitor_count;
       ++monitor_pos) {
    bool continue_enum =
        lpfnEnum(result.monitors[monitor_pos], nullptr, nullptr, dwData);
    if (!continue_enum)
      return false;
  }

  return true;
}

BOOL WINAPI TargetEnumDisplayDevicesA(EnumDisplayDevicesAFunction,
                                      LPCSTR lpDevice,
                                      DWORD iDevNum,
                                      PDISPLAY_DEVICEA lpDisplayDevice,
                                      DWORD dwFlags) {
  return false;
}

BOOL WINAPI TargetGetMonitorInfoA(GetMonitorInfoAFunction,
                                  HMONITOR monitor,
                                  MONITORINFO* monitor_info_ptr) {
  if (!monitor_info_ptr)
    return false;
  DWORD size = monitor_info_ptr->cbSize;
  if (size != sizeof(MONITORINFO) && size != sizeof(MONITORINFOEXA))
    return false;
  MONITORINFOEXW monitor_info_tmp = {};
  monitor_info_tmp.cbSize = sizeof(monitor_info_tmp);
  bool success = CallMonitorInfo(monitor, &monitor_info_tmp);
  if (!success)
    return false;
  memcpy(monitor_info_ptr, &monitor_info_tmp, sizeof(*monitor_info_ptr));
  if (size == sizeof(MONITORINFOEXA)) {
    MONITORINFOEXA* monitor_info_exa =
        reinterpret_cast<MONITORINFOEXA*>(monitor_info_ptr);
    if (!::WideCharToMultiByte(CP_ACP, 0, monitor_info_tmp.szDevice, -1,
                               monitor_info_exa->szDevice,
                               sizeof(monitor_info_exa->szDevice), nullptr,
                               nullptr)) {
      return false;
    }
  }
  return true;
}

BOOL WINAPI TargetGetMonitorInfoW(GetMonitorInfoWFunction,
                                  HMONITOR monitor,
                                  LPMONITORINFO monitor_info_ptr) {
  if (!monitor_info_ptr)
    return false;
  DWORD size = monitor_info_ptr->cbSize;
  if (size != sizeof(MONITORINFO) && size != sizeof(MONITORINFOEXW))
    return false;
  MONITORINFOEXW monitor_info_tmp = {};
  monitor_info_tmp.cbSize = sizeof(monitor_info_tmp);
  if (!CallMonitorInfo(monitor, &monitor_info_tmp))
    return false;
  memcpy(monitor_info_ptr, &monitor_info_tmp, size);
  return true;
}

static NTSTATUS GetCertificateCommon(
    PUNICODE_STRING device_name,
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    DXGKMDT_CERTIFICATE_TYPE certificate_type,
    BYTE* certificate,
    ULONG certificate_size) {
  // Don't support arbitrarily large certificate buffers.
  if (certificate_size > kProtectedVideoOutputSectionSize)
    return STATUS_INVALID_PARAMETER;
  if (certificate_type != DXGKMDT_OPM_CERTIFICATE)
    return STATUS_INVALID_PARAMETER;
  if (device_name && device_name->Length == 0)
    return STATUS_INVALID_PARAMETER;
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return STATUS_ACCESS_DENIED;
  void* ipc_memory = GetGlobalIPCMemory();
  if (!ipc_memory)
    return STATUS_ACCESS_DENIED;

  ScopedSharedMemory buffer(certificate_size);
  if (!buffer.IsValid())
    return STATUS_INVALID_PARAMETER;
  std::wstring device_name_str;
  void* protected_output_handle = nullptr;
  if (device_name) {
    if (device_name->Length == 0)
      return STATUS_INVALID_PARAMETER;
    UnicodeStringToString(device_name, &device_name_str);
  } else {
    protected_output_handle = protected_output;
  }
  CrossCallReturn answer = {};
  SharedMemIPCClient ipc(ipc_memory);
  ResultCode code =
      CrossCall(ipc, IpcTag::GDI_GETCERTIFICATE, device_name_str.c_str(),
                protected_output_handle, buffer.handle(),
                static_cast<uint32_t>(certificate_size), &answer);

  if (code != SBOX_ALL_OK) {
    return STATUS_ACCESS_DENIED;
  }

  if (!answer.nt_status)
    memcpy(certificate, buffer.memory(), certificate_size);

  return answer.nt_status;
}

NTSTATUS WINAPI TargetGetCertificate(GetCertificateFunction,
                                     PUNICODE_STRING device_name,
                                     DXGKMDT_CERTIFICATE_TYPE certificate_type,
                                     BYTE* certificate,
                                     ULONG certificate_size) {
  return GetCertificateCommon(device_name, nullptr, certificate_type,
                              certificate, certificate_size);
}

static NTSTATUS GetCertificateSizeCommon(
    PUNICODE_STRING device_name,
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    DXGKMDT_CERTIFICATE_TYPE certificate_type,
    ULONG* certificate_length) {
  if (certificate_type != DXGKMDT_OPM_CERTIFICATE)
    return STATUS_INVALID_PARAMETER;
  if (device_name && device_name->Length == 0)
    return STATUS_INVALID_PARAMETER;
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return STATUS_ACCESS_DENIED;
  void* ipc_memory = GetGlobalIPCMemory();
  if (!ipc_memory)
    return STATUS_ACCESS_DENIED;

  CrossCallReturn answer = {};
  SharedMemIPCClient ipc(ipc_memory);
  std::wstring device_name_str;
  void* protected_output_handle = nullptr;
  if (device_name) {
    UnicodeStringToString(device_name, &device_name_str);
  } else {
    protected_output_handle = protected_output;
  }
  ResultCode code =
      CrossCall(ipc, IpcTag::GDI_GETCERTIFICATESIZE, device_name_str.c_str(),
                protected_output_handle, &answer);

  if (code != SBOX_ALL_OK) {
    return STATUS_ACCESS_DENIED;
  }

  if (!answer.nt_status)
    *certificate_length = answer.extended[0].unsigned_int;

  return answer.nt_status;
}

NTSTATUS WINAPI
TargetGetCertificateSize(GetCertificateSizeFunction,
                         PUNICODE_STRING device_name,
                         DXGKMDT_CERTIFICATE_TYPE certificate_type,
                         ULONG* certificate_length) {
  return GetCertificateSizeCommon(device_name, nullptr, certificate_type,
                                  certificate_length);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetGetCertificateByHandle(
    GetCertificateByHandleFunction orig_get_certificate_function,
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    DXGKMDT_CERTIFICATE_TYPE certificate_type,
    BYTE* certificate,
    ULONG certificate_length) {
  return GetCertificateCommon(nullptr, protected_output, certificate_type,
                              certificate, certificate_length);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetGetCertificateSizeByHandle(
    GetCertificateSizeByHandleFunction orig_get_certificate_size_function,
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    DXGKMDT_CERTIFICATE_TYPE certificate_type,
    ULONG* certificate_length) {
  return GetCertificateSizeCommon(nullptr, protected_output, certificate_type,
                                  certificate_length);
}

NTSTATUS WINAPI
TargetDestroyOPMProtectedOutput(DestroyOPMProtectedOutputFunction,
                                OPM_PROTECTED_OUTPUT_HANDLE protected_output) {
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return STATUS_ACCESS_DENIED;
  void* ipc_memory = GetGlobalIPCMemory();
  if (!ipc_memory)
    return STATUS_ACCESS_DENIED;

  CrossCallReturn answer = {};
  SharedMemIPCClient ipc(ipc_memory);
  ResultCode code = CrossCall(ipc, IpcTag::GDI_DESTROYOPMPROTECTEDOUTPUT,
                              static_cast<void*>(protected_output), &answer);

  if (code != SBOX_ALL_OK)
    return STATUS_ACCESS_DENIED;

  return answer.nt_status;
}

NTSTATUS WINAPI TargetConfigureOPMProtectedOutput(
    ConfigureOPMProtectedOutputFunction,
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    const DXGKMDT_OPM_CONFIGURE_PARAMETERS* parameters,
    ULONG additional_parameters_size,
    const BYTE* additional_parameters) {
  // Don't support additional parameters.
  if (additional_parameters_size > 0)
    return STATUS_INVALID_PARAMETER;

  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return STATUS_ACCESS_DENIED;
  void* ipc_memory = GetGlobalIPCMemory();
  if (!ipc_memory)
    return STATUS_ACCESS_DENIED;

  ScopedSharedMemory buffer(sizeof(*parameters));
  if (!buffer.IsValid())
    return STATUS_INVALID_PARAMETER;
  memcpy(buffer.memory(), parameters, sizeof(*parameters));
  CrossCallReturn answer = {};
  SharedMemIPCClient ipc(ipc_memory);
  ResultCode code =
      CrossCall(ipc, IpcTag::GDI_CONFIGUREOPMPROTECTEDOUTPUT,
                static_cast<void*>(protected_output), buffer.handle(), &answer);

  if (code != SBOX_ALL_OK) {
    return STATUS_ACCESS_DENIED;
  }

  return answer.nt_status;
}

NTSTATUS WINAPI TargetGetOPMInformation(
    GetOPMInformationFunction,
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    const DXGKMDT_OPM_GET_INFO_PARAMETERS* parameters,
    DXGKMDT_OPM_REQUESTED_INFORMATION* requested_information) {
  size_t max_size =
      std::max(sizeof(*parameters), sizeof(*requested_information));

  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return STATUS_ACCESS_DENIED;
  void* ipc_memory = GetGlobalIPCMemory();
  if (!ipc_memory)
    return STATUS_ACCESS_DENIED;

  ScopedSharedMemory buffer(base::checked_cast<uint32_t>(max_size));
  if (!buffer.IsValid())
    return STATUS_INVALID_PARAMETER;
  memcpy(buffer.memory(), parameters, sizeof(*parameters));
  CrossCallReturn answer = {};
  SharedMemIPCClient ipc(ipc_memory);
  ResultCode code =
      CrossCall(ipc, IpcTag::GDI_GETOPMINFORMATION,
                static_cast<void*>(protected_output), buffer.handle(), &answer);

  if (code != SBOX_ALL_OK)
    return STATUS_ACCESS_DENIED;

  if (!answer.nt_status) {
    memcpy(requested_information, buffer.memory(),
           sizeof(*requested_information));
  }

  return answer.nt_status;
}

NTSTATUS WINAPI
TargetGetOPMRandomNumber(GetOPMRandomNumberFunction,
                         OPM_PROTECTED_OUTPUT_HANDLE protected_output,
                         DXGKMDT_OPM_RANDOM_NUMBER* random_number) {
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return STATUS_ACCESS_DENIED;
  void* ipc_memory = GetGlobalIPCMemory();
  if (!ipc_memory)
    return STATUS_ACCESS_DENIED;

  CrossCallReturn answer = {};
  SharedMemIPCClient ipc(ipc_memory);
  InOutCountedBuffer buffer(random_number, sizeof(*random_number));
  ResultCode code =
      CrossCall(ipc, IpcTag::GDI_GETOPMRANDOMNUMBER,
                static_cast<void*>(protected_output), buffer, &answer);

  if (code != SBOX_ALL_OK)
    return STATUS_ACCESS_DENIED;

  return answer.nt_status;
}

NTSTATUS WINAPI TargetGetSuggestedOPMProtectedOutputArraySize(
    GetSuggestedOPMProtectedOutputArraySizeFunction,
    PUNICODE_STRING device_name,
    DWORD* suggested_output_size) {
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return STATUS_ACCESS_DENIED;
  void* ipc_memory = GetGlobalIPCMemory();
  if (!ipc_memory)
    return STATUS_ACCESS_DENIED;

  CrossCallReturn answer = {};
  SharedMemIPCClient ipc(ipc_memory);
  std::wstring device_name_str;
  UnicodeStringToString(device_name, &device_name_str);
  ResultCode code =
      CrossCall(ipc, IpcTag::GDI_GETSUGGESTEDOPMPROTECTEDOUTPUTARRAYSIZE,
                device_name_str.c_str(), &answer);

  if (code != SBOX_ALL_OK)
    return STATUS_ACCESS_DENIED;

  if (!answer.nt_status)
    *suggested_output_size = answer.extended[0].unsigned_int;

  return answer.nt_status;
}

NTSTATUS WINAPI TargetSetOPMSigningKeyAndSequenceNumbers(
    SetOPMSigningKeyAndSequenceNumbersFunction,
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    const DXGKMDT_OPM_ENCRYPTED_PARAMETERS* parameters) {
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return STATUS_ACCESS_DENIED;
  void* ipc_memory = GetGlobalIPCMemory();
  if (!ipc_memory)
    return STATUS_ACCESS_DENIED;

  DXGKMDT_OPM_ENCRYPTED_PARAMETERS temp_parameters = *parameters;

  CrossCallReturn answer = {};
  SharedMemIPCClient ipc(ipc_memory);
  InOutCountedBuffer buffer(&temp_parameters, sizeof(temp_parameters));
  ResultCode code =
      CrossCall(ipc, IpcTag::GDI_SETOPMSIGNINGKEYANDSEQUENCENUMBERS,
                static_cast<void*>(protected_output), buffer, &answer);

  if (code != SBOX_ALL_OK)
    return STATUS_ACCESS_DENIED;

  return answer.nt_status;
}

NTSTATUS WINAPI
TargetCreateOPMProtectedOutputs(CreateOPMProtectedOutputsFunction,
                                PUNICODE_STRING device_name,
                                DXGKMDT_OPM_VIDEO_OUTPUT_SEMANTICS vos,
                                DWORD outputs_array_size,
                                DWORD* output_size,
                                OPM_PROTECTED_OUTPUT_HANDLE* outputs_array) {
  if (vos != DXGKMDT_OPM_VOS_OPM_SEMANTICS)
    return STATUS_INVALID_PARAMETER;

  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return STATUS_ACCESS_DENIED;
  void* ipc_memory = GetGlobalIPCMemory();
  if (!ipc_memory)
    return STATUS_ACCESS_DENIED;

  CrossCallReturn answer = {};
  SharedMemIPCClient ipc(ipc_memory);
  base::CheckedNumeric<uint32_t> array_size = outputs_array_size;
  array_size *= sizeof(HANDLE);
  if (!array_size.IsValid())
    return STATUS_INVALID_PARAMETER;

  InOutCountedBuffer buffer(outputs_array, array_size.ValueOrDie());
  std::wstring device_name_str;
  UnicodeStringToString(device_name, &device_name_str);
  ResultCode code = CrossCall(ipc, IpcTag::GDI_CREATEOPMPROTECTEDOUTPUTS,
                              device_name_str.c_str(), buffer, &answer);

  if (code != SBOX_ALL_OK)
    return STATUS_ACCESS_DENIED;

  if (!answer.nt_status)
    *output_size = answer.extended[0].unsigned_int;

  return answer.nt_status;
}

}  // namespace sandbox
