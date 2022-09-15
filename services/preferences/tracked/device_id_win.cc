// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/device_id.h"

#include <windows.h>

#include <sddl.h>  // For ConvertSidToStringSidA.

#include <memory>

#include "base/check.h"

MachineIdStatus GetDeterministicMachineSpecificId(std::string* machine_id) {
  DCHECK(machine_id);

  wchar_t computer_name[MAX_COMPUTERNAME_LENGTH + 1] = {};
  DWORD computer_name_size = std::size(computer_name);

  if (!::GetComputerNameW(computer_name, &computer_name_size))
    return MachineIdStatus::FAILURE;

  DWORD sid_size = SECURITY_MAX_SID_SIZE;
  char sid_buffer[SECURITY_MAX_SID_SIZE];
  SID* sid = reinterpret_cast<SID*>(sid_buffer);
  DWORD domain_size = 128;  // Will expand below if needed.
  std::unique_ptr<wchar_t[]> domain_buffer(new wchar_t[domain_size]);
  SID_NAME_USE sid_name_use;

  // Although the fifth argument to |LookupAccountNameW()|,
  // |ReferencedDomainName|, is annotated as |_Out_opt_|, if a null
  // value is passed in, zero is returned and |GetLastError()| will
  // return |ERROR_INSUFFICIENT_BUFFER| (assuming that nothing else went
  // wrong). In order to ensure that the call to |LookupAccountNameW()|
  // has succeeded, it is necessary to include the following logic and
  // obtain the domain name.
  if (!::LookupAccountNameW(nullptr, computer_name, sid, &sid_size,
                            domain_buffer.get(), &domain_size, &sid_name_use)) {
    // If the initial size of |domain_buffer| was too small, the
    // required size is now found in |domain_size|. Resize and try
    // again.
    if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER)
      return MachineIdStatus::FAILURE;

    domain_buffer.reset(new wchar_t[domain_size]);
    if (!::LookupAccountNameW(nullptr, computer_name, sid, &sid_size,
                              domain_buffer.get(), &domain_size,
                              &sid_name_use)) {
      return MachineIdStatus::FAILURE;
    }
  }

  // Ensure that the correct type of SID was obtained. The
  // |LookupAccountNameW()| function seems to always return
  // |SidTypeDomain| instead of |SidTypeComputer| when the computer name
  // is passed in as its second argument and therefore both enum values
  // will be considered acceptable. If the computer name and user name
  // coincide, |LookupAccountNameW()| seems to always return the machine
  // SID and set the returned enum to |SidTypeDomain|.
  DCHECK(sid_name_use == SID_NAME_USE::SidTypeComputer ||
         sid_name_use == SID_NAME_USE::SidTypeDomain);

  char* sid_string = nullptr;
  if (!::ConvertSidToStringSidA(sid, &sid_string))
    return MachineIdStatus::FAILURE;

  *machine_id = sid_string;
  ::LocalFree(sid_string);

  return MachineIdStatus::SUCCESS;
}
