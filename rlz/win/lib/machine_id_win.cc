// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <windows.h>

#include <Sddl.h>  // For ConvertSidToStringSidW.

#include <memory>
#include <string>

#include "base/containers/heap_array.h"
#include "base/strings/utf_string_conversions.h"
#include "rlz/lib/assert.h"

namespace rlz_lib {

namespace {

bool GetSystemVolumeSerialNumber(int* number) {
  if (!number)
    return false;

  *number = 0;

  // Find the system root path (e.g: C:\).
  wchar_t system_path[MAX_PATH + 1];
  if (!GetSystemDirectoryW(system_path, MAX_PATH))
    return false;

  wchar_t* first_slash = wcspbrk(system_path, L"\\/");
  if (first_slash != NULL)
    *(first_slash + 1) = 0;

  DWORD number_local = 0;
  if (!GetVolumeInformationW(system_path, NULL, 0, &number_local, NULL, NULL,
                             NULL, 0))
    return false;

  *number = number_local;
  return true;
}

bool GetComputerSid(const wchar_t* account_name, SID* sid, DWORD sid_size) {
  static const DWORD kStartDomainLength = 128;  // reasonable to start with

  base::HeapArray<wchar_t> domain_buffer =
      base::HeapArray<wchar_t>::Uninit(kStartDomainLength);
  DWORD domain_size = kStartDomainLength;
  DWORD sid_dword_size = sid_size;
  SID_NAME_USE sid_name_use;

  BOOL success =
      ::LookupAccountNameW(NULL, account_name, sid, &sid_dword_size,
                           domain_buffer.data(), &domain_size, &sid_name_use);
  if (!success && ::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    // We could have gotten the insufficient buffer error because
    // one or both of sid and szDomain was too small. Check for that
    // here.
    if (sid_dword_size > sid_size)
      return false;

    if (domain_size > kStartDomainLength)
      domain_buffer = base::HeapArray<wchar_t>::Uninit(domain_size);

    success =
        ::LookupAccountNameW(NULL, account_name, sid, &sid_dword_size,
                             domain_buffer.data(), &domain_size, &sid_name_use);
  }

  return success != FALSE;
}

std::u16string ConvertSidToString(SID* sid) {
  std::wstring sid_string;
  wchar_t* sid_buffer = NULL;
  if (ConvertSidToStringSidW(sid, &sid_buffer)) {
    sid_string = sid_buffer;
    LocalFree(sid_buffer);
  }
  return base::WideToUTF16(sid_string);
}

}  // namespace

bool GetRawMachineId(std::u16string* sid_string, int* volume_id) {
  // Calculate the Windows SID.

  wchar_t computer_name[MAX_COMPUTERNAME_LENGTH + 1] = {0};
  DWORD size = std::size(computer_name);

  if (GetComputerNameW(computer_name, &size)) {
    char sid_buffer[SECURITY_MAX_SID_SIZE];
    SID* sid = reinterpret_cast<SID*>(sid_buffer);
    if (GetComputerSid(computer_name, sid, SECURITY_MAX_SID_SIZE)) {
      *sid_string = ConvertSidToString(sid);
    }
  }

  // Get the system drive volume serial number.
  *volume_id = 0;
  if (!GetSystemVolumeSerialNumber(volume_id)) {
    ASSERT_STRING("GetMachineId: Failed to retrieve volume serial number");
    *volume_id = 0;
  }

  return true;
}

}  // namespace rlz_lib
