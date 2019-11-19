// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_mitigations_win32k_dispatcher.h"

#include <algorithm>
#include <string>

#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/unguessable_token.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/process_mitigations_win32k_interception.h"
#include "sandbox/win/src/process_mitigations_win32k_policy.h"

namespace sandbox {

namespace {

base::UnsafeSharedMemoryRegion GetSharedMemoryRegion(
    const ClientInfo& client_info,
    HANDLE handle,
    size_t size) {
  HANDLE dup_handle = nullptr;
  intptr_t handle_int = reinterpret_cast<intptr_t>(handle);
  if (handle_int <= 0 ||
      !::DuplicateHandle(client_info.process, handle, ::GetCurrentProcess(),
                         &dup_handle, 0, false, DUPLICATE_SAME_ACCESS)) {
    return {};
  }
  // The raw handle returned from ::DuplicateHandle() must be wrapped in a
  // base::PlatformSharedMemoryRegion.
  base::subtle::PlatformSharedMemoryRegion platform_region =
      base::subtle::PlatformSharedMemoryRegion::Take(
          base::win::ScopedHandle(dup_handle),
          base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe, size,
          base::UnguessableToken::Create());
  // |platform_region| can now be wrapped in a base::UnsafeSharedMemoryRegion.
  return base::UnsafeSharedMemoryRegion::Deserialize(
      std::move(platform_region));
}

}  // namespace

ProtectedVideoOutput::~ProtectedVideoOutput() {
  ProcessMitigationsWin32KLockdownPolicy::DestroyOPMProtectedOutputAction(
      handle_);
}

scoped_refptr<ProtectedVideoOutput>
ProcessMitigationsWin32KDispatcher::GetProtectedVideoOutput(
    HANDLE handle,
    bool destroy_output) {
  base::AutoLock lock(protected_outputs_lock_);
  scoped_refptr<ProtectedVideoOutput> result;
  auto it = protected_outputs_.find(handle);
  if (it != protected_outputs_.end()) {
    result = it->second;
    if (destroy_output)
      protected_outputs_.erase(it);
  }
  return result;
}

ProcessMitigationsWin32KDispatcher::ProcessMitigationsWin32KDispatcher(
    PolicyBase* policy_base)
    : policy_base_(policy_base) {
  static const IPCCall enum_display_monitors_params = {
      {IpcTag::USER_ENUMDISPLAYMONITORS, {INOUTPTR_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &ProcessMitigationsWin32KDispatcher::EnumDisplayMonitors)};
  static const IPCCall get_monitor_info_params = {
      {IpcTag::USER_GETMONITORINFO, {VOIDPTR_TYPE, INOUTPTR_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &ProcessMitigationsWin32KDispatcher::GetMonitorInfo)};
  static const IPCCall get_suggested_output_size_params = {
      {IpcTag::GDI_GETSUGGESTEDOPMPROTECTEDOUTPUTARRAYSIZE, {WCHAR_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &ProcessMitigationsWin32KDispatcher::
              GetSuggestedOPMProtectedOutputArraySize)};
  static const IPCCall create_protected_outputs_params = {
      {IpcTag::GDI_CREATEOPMPROTECTEDOUTPUTS, {WCHAR_TYPE, INOUTPTR_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &ProcessMitigationsWin32KDispatcher::CreateOPMProtectedOutputs)};
  static const IPCCall get_cert_size_params = {
      {IpcTag::GDI_GETCERTIFICATESIZE, {WCHAR_TYPE, VOIDPTR_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &ProcessMitigationsWin32KDispatcher::GetCertificateSize)};
  static const IPCCall get_cert_params = {
      {IpcTag::GDI_GETCERTIFICATE,
       {WCHAR_TYPE, VOIDPTR_TYPE, VOIDPTR_TYPE, UINT32_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &ProcessMitigationsWin32KDispatcher::GetCertificate)};
  static const IPCCall destroy_protected_output_params = {
      {IpcTag::GDI_DESTROYOPMPROTECTEDOUTPUT, {VOIDPTR_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &ProcessMitigationsWin32KDispatcher::DestroyOPMProtectedOutput)};
  static const IPCCall get_random_number_params = {
      {IpcTag::GDI_GETOPMRANDOMNUMBER, {VOIDPTR_TYPE, INOUTPTR_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &ProcessMitigationsWin32KDispatcher::GetOPMRandomNumber)};
  static const IPCCall set_signing_key_params = {
      {IpcTag::GDI_SETOPMSIGNINGKEYANDSEQUENCENUMBERS,
       {VOIDPTR_TYPE, INOUTPTR_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &ProcessMitigationsWin32KDispatcher::
              SetOPMSigningKeyAndSequenceNumbers)};
  static const IPCCall configure_protected_output_params = {
      {IpcTag::GDI_CONFIGUREOPMPROTECTEDOUTPUT, {VOIDPTR_TYPE, VOIDPTR_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &ProcessMitigationsWin32KDispatcher::ConfigureOPMProtectedOutput)};
  static const IPCCall get_information_params = {
      {IpcTag::GDI_GETOPMINFORMATION, {VOIDPTR_TYPE, VOIDPTR_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &ProcessMitigationsWin32KDispatcher::GetOPMInformation)};

  ipc_calls_.push_back(enum_display_monitors_params);
  ipc_calls_.push_back(get_monitor_info_params);
  ipc_calls_.push_back(get_suggested_output_size_params);
  ipc_calls_.push_back(create_protected_outputs_params);
  ipc_calls_.push_back(get_cert_size_params);
  ipc_calls_.push_back(get_cert_params);
  ipc_calls_.push_back(destroy_protected_output_params);
  ipc_calls_.push_back(get_random_number_params);
  ipc_calls_.push_back(set_signing_key_params);
  ipc_calls_.push_back(configure_protected_output_params);
  ipc_calls_.push_back(get_information_params);
}

ProcessMitigationsWin32KDispatcher::~ProcessMitigationsWin32KDispatcher() {}

bool ProcessMitigationsWin32KDispatcher::SetupService(
    InterceptionManager* manager,
    IpcTag service) {
  if (!(policy_base_->GetProcessMitigations() &
        sandbox::MITIGATION_WIN32K_DISABLE)) {
    return false;
  }

  switch (service) {
    case IpcTag::GDI_GDIDLLINITIALIZE: {
      if (!INTERCEPT_EAT(manager, L"gdi32.dll", GdiDllInitialize,
                         GDIINITIALIZE_ID, 12)) {
        return false;
      }
      return true;
    }

    case IpcTag::GDI_GETSTOCKOBJECT: {
      if (!INTERCEPT_EAT(manager, L"gdi32.dll", GetStockObject,
                         GETSTOCKOBJECT_ID, 8)) {
        return false;
      }
      return true;
    }

    case IpcTag::USER_REGISTERCLASSW: {
      if (!INTERCEPT_EAT(manager, L"user32.dll", RegisterClassW,
                         REGISTERCLASSW_ID, 8)) {
        return false;
      }
      return true;
    }

    case IpcTag::USER_ENUMDISPLAYMONITORS: {
      if (!INTERCEPT_EAT(manager, L"user32.dll", EnumDisplayMonitors,
                         ENUMDISPLAYMONITORS_ID, 20)) {
        return false;
      }
      return true;
    }

    case IpcTag::USER_ENUMDISPLAYDEVICES: {
      if (!INTERCEPT_EAT(manager, L"user32.dll", EnumDisplayDevicesA,
                         ENUMDISPLAYDEVICESA_ID, 20)) {
        return false;
      }
      return true;
    }

    case IpcTag::USER_GETMONITORINFO: {
      if (!INTERCEPT_EAT(manager, L"user32.dll", GetMonitorInfoA,
                         GETMONITORINFOA_ID, 12)) {
        return false;
      }

      if (!INTERCEPT_EAT(manager, L"user32.dll", GetMonitorInfoW,
                         GETMONITORINFOW_ID, 12)) {
        return false;
      }
      return true;
    }

    case IpcTag::GDI_CREATEOPMPROTECTEDOUTPUTS:
      if (!INTERCEPT_EAT(manager, L"gdi32.dll", CreateOPMProtectedOutputs,
                         CREATEOPMPROTECTEDOUTPUTS_ID, 24)) {
        return false;
      }
      return true;

    case IpcTag::GDI_GETCERTIFICATE:
      if (!INTERCEPT_EAT(manager, L"gdi32.dll", GetCertificate,
                         GETCERTIFICATE_ID, 20)) {
        return false;
      }
      if (base::win::GetVersion() < base::win::Version::WIN10_TH2)
        return true;
      if (!INTERCEPT_EAT(manager, L"gdi32.dll", GetCertificateByHandle,
                         GETCERTIFICATEBYHANDLE_ID, 20)) {
        return false;
      }
      return true;

    case IpcTag::GDI_GETCERTIFICATESIZE:
      if (!INTERCEPT_EAT(manager, L"gdi32.dll", GetCertificateSize,
                         GETCERTIFICATESIZE_ID, 16)) {
        return false;
      }
      if (base::win::GetVersion() < base::win::Version::WIN10_TH2)
        return true;
      if (!INTERCEPT_EAT(manager, L"gdi32.dll", GetCertificateSizeByHandle,
                         GETCERTIFICATESIZEBYHANDLE_ID, 16)) {
        return false;
      }
      return true;

    case IpcTag::GDI_DESTROYOPMPROTECTEDOUTPUT:
      if (!INTERCEPT_EAT(manager, L"gdi32.dll", DestroyOPMProtectedOutput,
                         DESTROYOPMPROTECTEDOUTPUT_ID, 8)) {
        return false;
      }
      return true;

    case IpcTag::GDI_CONFIGUREOPMPROTECTEDOUTPUT:
      if (!INTERCEPT_EAT(manager, L"gdi32.dll", ConfigureOPMProtectedOutput,
                         CONFIGUREOPMPROTECTEDOUTPUT_ID, 20)) {
        return false;
      }
      return true;

    case IpcTag::GDI_GETOPMINFORMATION:
      if (!INTERCEPT_EAT(manager, L"gdi32.dll", GetOPMInformation,
                         GETOPMINFORMATION_ID, 16)) {
        return false;
      }
      return true;

    case IpcTag::GDI_GETOPMRANDOMNUMBER:
      if (!INTERCEPT_EAT(manager, L"gdi32.dll", GetOPMRandomNumber,
                         GETOPMRANDOMNUMBER_ID, 12)) {
        return false;
      }
      return true;

    case IpcTag::GDI_GETSUGGESTEDOPMPROTECTEDOUTPUTARRAYSIZE:
      if (!INTERCEPT_EAT(manager, L"gdi32.dll",
                         GetSuggestedOPMProtectedOutputArraySize,
                         GETSUGGESTEDOPMPROTECTEDOUTPUTARRAYSIZE_ID, 12)) {
        return false;
      }
      return true;

    case IpcTag::GDI_SETOPMSIGNINGKEYANDSEQUENCENUMBERS:
      if (!INTERCEPT_EAT(manager, L"gdi32.dll",
                         SetOPMSigningKeyAndSequenceNumbers,
                         SETOPMSIGNINGKEYANDSEQUENCENUMBERS_ID, 12)) {
        return false;
      }
      return true;

    default:
      break;
  }
  return false;
}

bool ProcessMitigationsWin32KDispatcher::EnumDisplayMonitors(
    IPCInfo* ipc,
    CountedBuffer* buffer) {
  if (!policy_base_->GetEnableOPMRedirection()) {
    ipc->return_info.win32_result = ERROR_ACCESS_DENIED;
    return true;
  }

  if (buffer->Size() != sizeof(EnumMonitorsResult)) {
    ipc->return_info.win32_result = ERROR_INVALID_PARAMETER;
    return true;
  }
  HMONITOR monitor_list[kMaxEnumMonitors] = {};

  uint32_t monitor_list_count =
      ProcessMitigationsWin32KLockdownPolicy::EnumDisplayMonitorsAction(
          *ipc->client_info, monitor_list, kMaxEnumMonitors);
  DCHECK(monitor_list_count <= kMaxEnumMonitors);

  EnumMonitorsResult* result =
      static_cast<EnumMonitorsResult*>(buffer->Buffer());
  for (uint32_t monitor_pos = 0; monitor_pos < monitor_list_count;
       ++monitor_pos) {
    result->monitors[monitor_pos] = monitor_list[monitor_pos];
  }
  result->monitor_count = monitor_list_count;
  ipc->return_info.win32_result = 0;

  return true;
}

bool ProcessMitigationsWin32KDispatcher::GetMonitorInfo(IPCInfo* ipc,
                                                        void* monitor,
                                                        CountedBuffer* buffer) {
  if (!policy_base_->GetEnableOPMRedirection()) {
    ipc->return_info.win32_result = ERROR_ACCESS_DENIED;
    return true;
  }
  if (buffer->Size() != sizeof(MONITORINFOEXW)) {
    ipc->return_info.win32_result = ERROR_INVALID_PARAMETER;
    return true;
  }
  MONITORINFO* monitor_info = static_cast<MONITORINFO*>(buffer->Buffer());
  // Ensure size is valid and represents what we've been passed.
  monitor_info->cbSize = buffer->Size();
  HMONITOR monitor_handle = static_cast<HMONITOR>(monitor);
  bool success = ProcessMitigationsWin32KLockdownPolicy::GetMonitorInfoAction(
      *ipc->client_info, monitor_handle, monitor_info);
  ipc->return_info.win32_result =
      success ? ERROR_SUCCESS : ERROR_INVALID_PARAMETER;
  return true;
}

bool ProcessMitigationsWin32KDispatcher::
    GetSuggestedOPMProtectedOutputArraySize(IPCInfo* ipc,
                                            std::wstring* device_name) {
  if (!policy_base_->GetEnableOPMRedirection()) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }
  NTSTATUS status = ProcessMitigationsWin32KLockdownPolicy::
      GetSuggestedOPMProtectedOutputArraySizeAction(
          *ipc->client_info, *device_name,
          &ipc->return_info.extended[0].unsigned_int);
  if (!status) {
    ipc->return_info.extended_count = 1;
  }
  ipc->return_info.nt_status = status;
  return true;
}

bool ProcessMitigationsWin32KDispatcher::CreateOPMProtectedOutputs(
    IPCInfo* ipc,
    std::wstring* device_name,
    CountedBuffer* protected_outputs) {
  if (!policy_base_->GetEnableOPMRedirection()) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }
  uint32_t output_array_size = 0;
  uint32_t input_array_size = protected_outputs->Size() / sizeof(HANDLE);
  HANDLE* handles = static_cast<HANDLE*>(protected_outputs->Buffer());
  NTSTATUS status =
      ProcessMitigationsWin32KLockdownPolicy::CreateOPMProtectedOutputsAction(
          *ipc->client_info, *device_name, handles, input_array_size,
          &output_array_size);
  if (!status && (output_array_size <= input_array_size)) {
    base::AutoLock lock(protected_outputs_lock_);
    ipc->return_info.extended_count = 1;
    ipc->return_info.extended[0].unsigned_int = output_array_size;
    for (uint32_t handle_pos = 0; handle_pos < output_array_size;
         handle_pos++) {
      HANDLE handle = handles[handle_pos];
      protected_outputs_[handle] = new ProtectedVideoOutput(handle);
    }
  }
  ipc->return_info.nt_status = status;
  return true;
}

bool ProcessMitigationsWin32KDispatcher::GetCertificateSize(
    IPCInfo* ipc,
    std::wstring* device_name,
    void* protected_output) {
  if (!policy_base_->GetEnableOPMRedirection()) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }
  NTSTATUS status = STATUS_INVALID_PARAMETER;
  if (device_name->size() > 0) {
    status = ProcessMitigationsWin32KLockdownPolicy::GetCertificateSizeAction(
        *ipc->client_info, *device_name,
        &ipc->return_info.extended[0].unsigned_int);
  } else {
    scoped_refptr<ProtectedVideoOutput> output =
        GetProtectedVideoOutput(protected_output, false);
    if (output) {
      status = ProcessMitigationsWin32KLockdownPolicy::
          GetCertificateSizeByHandleAction(
              *ipc->client_info, output.get()->handle(),
              &ipc->return_info.extended[0].unsigned_int);
    }
  }
  if (!status) {
    ipc->return_info.extended_count = 1;
  }
  ipc->return_info.nt_status = status;
  return true;
}

bool ProcessMitigationsWin32KDispatcher::GetCertificate(
    IPCInfo* ipc,
    std::wstring* device_name,
    void* protected_output,
    void* shared_buffer_handle,
    uint32_t shared_buffer_size) {
  if (!policy_base_->GetEnableOPMRedirection()) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }
  // Don't let caller map an arbitrarily large buffer into memory.
  if (shared_buffer_size > kProtectedVideoOutputSectionSize) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }
  base::UnsafeSharedMemoryRegion region = GetSharedMemoryRegion(
      *ipc->client_info, shared_buffer_handle, shared_buffer_size);
  if (!region.IsValid()) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }
  base::WritableSharedMemoryMapping cert_data = region.Map();
  if (!cert_data.IsValid()) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }
  NTSTATUS status = STATUS_INVALID_PARAMETER;
  if (device_name->size() > 0) {
    status = ProcessMitigationsWin32KLockdownPolicy::GetCertificateAction(
        *ipc->client_info, *device_name,
        cert_data.GetMemoryAsSpan<BYTE>().data(), shared_buffer_size);
  } else {
    scoped_refptr<ProtectedVideoOutput> output =
        GetProtectedVideoOutput(protected_output, false);
    if (output) {
      status =
          ProcessMitigationsWin32KLockdownPolicy::GetCertificateByHandleAction(
              *ipc->client_info, output.get()->handle(),
              cert_data.GetMemoryAsSpan<BYTE>().data(), shared_buffer_size);
    }
  }
  ipc->return_info.nt_status = status;
  return true;
}

bool ProcessMitigationsWin32KDispatcher::DestroyOPMProtectedOutput(
    IPCInfo* ipc,
    void* protected_output) {
  if (!policy_base_->GetEnableOPMRedirection()) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }
  scoped_refptr<ProtectedVideoOutput> output =
      GetProtectedVideoOutput(protected_output, true);
  NTSTATUS status = STATUS_INVALID_HANDLE;
  if (output)
    status = STATUS_SUCCESS;
  ipc->return_info.nt_status = status;
  return true;
}

bool ProcessMitigationsWin32KDispatcher::GetOPMRandomNumber(
    IPCInfo* ipc,
    void* protected_output,
    CountedBuffer* random_number) {
  if (!policy_base_->GetEnableOPMRedirection()) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }
  scoped_refptr<ProtectedVideoOutput> output =
      GetProtectedVideoOutput(protected_output, false);
  NTSTATUS status = STATUS_INVALID_PARAMETER;
  if (!output || random_number->Size() != sizeof(DXGKMDT_OPM_RANDOM_NUMBER)) {
    status = STATUS_INVALID_PARAMETER;
  } else {
    status = ProcessMitigationsWin32KLockdownPolicy::GetOPMRandomNumberAction(
        *ipc->client_info, output.get()->handle(), random_number->Buffer());
  }
  ipc->return_info.nt_status = status;
  return true;
}

bool ProcessMitigationsWin32KDispatcher::SetOPMSigningKeyAndSequenceNumbers(
    IPCInfo* ipc,
    void* protected_output,
    CountedBuffer* parameters) {
  if (!policy_base_->GetEnableOPMRedirection()) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }
  scoped_refptr<ProtectedVideoOutput> output =
      GetProtectedVideoOutput(protected_output, false);
  NTSTATUS status = STATUS_INVALID_PARAMETER;
  if (!output ||
      parameters->Size() != sizeof(DXGKMDT_OPM_ENCRYPTED_PARAMETERS)) {
    status = STATUS_INVALID_PARAMETER;
  } else {
    status = ProcessMitigationsWin32KLockdownPolicy::
        SetOPMSigningKeyAndSequenceNumbersAction(
            *ipc->client_info, output.get()->handle(), parameters->Buffer());
  }
  ipc->return_info.nt_status = status;
  return true;
}

bool ProcessMitigationsWin32KDispatcher::ConfigureOPMProtectedOutput(
    IPCInfo* ipc,
    void* protected_output,
    void* shared_buffer_handle) {
  if (!policy_base_->GetEnableOPMRedirection()) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }
  scoped_refptr<ProtectedVideoOutput> output =
      GetProtectedVideoOutput(protected_output, false);
  if (!output) {
    ipc->return_info.nt_status = STATUS_INVALID_HANDLE;
    return true;
  };
  base::UnsafeSharedMemoryRegion region =
      GetSharedMemoryRegion(*ipc->client_info, shared_buffer_handle,
                            sizeof(DXGKMDT_OPM_CONFIGURE_PARAMETERS));
  if (!region.IsValid()) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }
  base::WritableSharedMemoryMapping buffer = region.Map();
  if (!buffer.IsValid()) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }
  NTSTATUS status =
      ProcessMitigationsWin32KLockdownPolicy::ConfigureOPMProtectedOutputAction(
          *ipc->client_info, output.get()->handle(), buffer.memory());
  ipc->return_info.nt_status = status;
  return true;
}

bool ProcessMitigationsWin32KDispatcher::GetOPMInformation(
    IPCInfo* ipc,
    void* protected_output,
    void* shared_buffer_handle) {
  if (!policy_base_->GetEnableOPMRedirection()) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }
  scoped_refptr<ProtectedVideoOutput> output =
      GetProtectedVideoOutput(protected_output, false);
  if (!output) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }
  size_t shared_buffer_size =
      std::max(sizeof(DXGKMDT_OPM_GET_INFO_PARAMETERS),
               sizeof(DXGKMDT_OPM_REQUESTED_INFORMATION));

  base::UnsafeSharedMemoryRegion region = GetSharedMemoryRegion(
      *ipc->client_info, shared_buffer_handle, shared_buffer_size);
  if (!region.IsValid()) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }

  base::WritableSharedMemoryMapping buffer = region.Map();
  if (!buffer.IsValid()) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }
  DXGKMDT_OPM_REQUESTED_INFORMATION requested_info = {};
  NTSTATUS status =
      ProcessMitigationsWin32KLockdownPolicy::GetOPMInformationAction(
          *ipc->client_info, output.get()->handle(), buffer.memory(),
          &requested_info);
  if (!status) {
    memcpy(buffer.memory(), &requested_info,
           sizeof(DXGKMDT_OPM_REQUESTED_INFORMATION));
  }
  ipc->return_info.nt_status = status;
  return true;
}

}  // namespace sandbox
