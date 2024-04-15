// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A library to manage RLZ information for access-points shared
// across different client applications.

#include <windows.h>

#include <aclapi.h>
#include <stddef.h>
#include <winerror.h>

#include <memory>

#include "base/win/registry.h"
#include "rlz/lib/assert.h"
#include "rlz/lib/machine_deal_win.h"
#include "rlz/lib/rlz_value_store.h"
#include "rlz/win/lib/machine_deal.h"
#include "rlz/win/lib/rlz_value_store_registry.h"

namespace rlz_lib {

// OEM Deal confirmation storage functions.

template<class T>
class typed_buffer_ptr {
  std::unique_ptr<char[]> buffer_;

 public:
  typed_buffer_ptr() {
  }

  explicit typed_buffer_ptr(size_t size) : buffer_(new char[size]) {
  }

  void reset(size_t size) {
    buffer_.reset(new char[size]);
  }

  operator T*() {
    return reinterpret_cast<T*>(buffer_.get());
  }
};

// Check if this SID has the desired access by scanning the ACEs in the DACL.
// This function is part of the rlz_lib namespace so that it can be called from
// unit tests.  Non-unit test code should not call this function.
bool HasAccess(PSID sid, ACCESS_MASK access_mask, ACL* dacl) {
  if (dacl == NULL)
    return false;

  ACL_SIZE_INFORMATION info;
  if (!GetAclInformation(dacl, &info, sizeof(info), AclSizeInformation))
    return false;

  GENERIC_MAPPING generic_mapping = {KEY_READ, KEY_WRITE, KEY_EXECUTE,
                                     KEY_ALL_ACCESS};
  MapGenericMask(&access_mask, &generic_mapping);

  for (DWORD i = 0; i < info.AceCount; ++i) {
    ACCESS_ALLOWED_ACE* ace;
    if (GetAce(dacl, i, reinterpret_cast<void**>(&ace))) {
      if ((ace->Header.AceFlags & INHERIT_ONLY_ACE) == INHERIT_ONLY_ACE)
        continue;

      PSID existing_sid = reinterpret_cast<PSID>(&ace->SidStart);
      DWORD mask = ace->Mask;
      MapGenericMask(&mask, &generic_mapping);

      if (ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE &&
         (mask & access_mask) == access_mask && EqualSid(existing_sid, sid))
        return true;

      if (ace->Header.AceType == ACCESS_DENIED_ACE_TYPE &&
         (mask & access_mask) != 0 && EqualSid(existing_sid, sid))
        return false;
    }
  }

  return false;
}

bool CreateMachineState() {
  LibMutex lock;
  if (lock.failed())
    return false;

  base::win::RegKey hklm_key;
  if (hklm_key.Create(HKEY_LOCAL_MACHINE,
                      RlzValueStoreRegistry::GetWideLibKeyName().c_str(),
                      KEY_ALL_ACCESS | KEY_WOW64_32KEY) != ERROR_SUCCESS) {
    ASSERT_STRING("rlz_lib::CreateMachineState: "
                  "Unable to create / open machine key.");
    return false;
  }

  // Create a SID that represents ALL USERS.
  DWORD users_sid_size = SECURITY_MAX_SID_SIZE;
  typed_buffer_ptr<SID> users_sid(users_sid_size);
  CreateWellKnownSid(WinBuiltinUsersSid, NULL, users_sid, &users_sid_size);

  // Get the security descriptor for the registry key.
  DWORD original_sd_size = 0;
  ::RegGetKeySecurity(hklm_key.Handle(), DACL_SECURITY_INFORMATION, NULL,
      &original_sd_size);
  typed_buffer_ptr<SECURITY_DESCRIPTOR> original_sd(original_sd_size);

  LONG result = ::RegGetKeySecurity(hklm_key.Handle(),
      DACL_SECURITY_INFORMATION, original_sd, &original_sd_size);
  if (result != ERROR_SUCCESS) {
    ASSERT_STRING("rlz_lib::CreateMachineState: "
                  "Unable to create / open machine key.");
    return false;
  }

  // Make a copy of the security descriptor so we can modify it.  The one
  // returned by RegGetKeySecurity() is self-relative, so we need to make it
  // absolute.
  DWORD new_sd_size = 0;
  DWORD dacl_size = 0;
  DWORD sacl_size = 0;
  DWORD owner_size = 0;
  DWORD group_size = 0;
  ::MakeAbsoluteSD(original_sd, NULL, &new_sd_size, NULL, &dacl_size,
                        NULL, &sacl_size, NULL, &owner_size,
                        NULL, &group_size);

  typed_buffer_ptr<SECURITY_DESCRIPTOR> new_sd(new_sd_size);
  // Make sure the DACL is big enough to add one more ACE.
  typed_buffer_ptr<ACL> dacl(dacl_size + SECURITY_MAX_SID_SIZE);
  typed_buffer_ptr<ACL> sacl(sacl_size);
  typed_buffer_ptr<SID> owner(owner_size);
  typed_buffer_ptr<SID> group(group_size);

  if (!::MakeAbsoluteSD(original_sd, new_sd, &new_sd_size, dacl, &dacl_size,
                        sacl, &sacl_size, owner, &owner_size,
                        group, &group_size)) {
    ASSERT_STRING("rlz_lib::CreateMachineState: MakeAbsoluteSD failed");
    return false;
  }

  // If all users already have read/write access to the registry key, then
  // nothing to do.  Otherwise change the security descriptor of the key to
  // give everyone access.
  if (HasAccess(users_sid, KEY_ALL_ACCESS, dacl)) {
    return false;
  }

  // Add ALL-USERS ALL-ACCESS ACL.
  EXPLICIT_ACCESS ea;
  ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));
  ea.grfAccessPermissions = GENERIC_ALL | KEY_ALL_ACCESS;
  ea.grfAccessMode = GRANT_ACCESS;
  ea.grfInheritance= SUB_CONTAINERS_AND_OBJECTS_INHERIT;
  ea.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
  ea.Trustee.ptstrName = const_cast<wchar_t*>(L"Everyone");

  ACL* new_dacl = NULL;
  result = SetEntriesInAcl(1, &ea, dacl, &new_dacl);
  if (result != ERROR_SUCCESS) {
    ASSERT_STRING("rlz_lib::CreateMachineState: SetEntriesInAcl failed");
    return false;
  }

  BOOL ok = SetSecurityDescriptorDacl(new_sd, TRUE, new_dacl, FALSE);
  if (!ok) {
    ASSERT_STRING("rlz_lib::CreateMachineState: "
                  "SetSecurityDescriptorOwner failed");
    LocalFree(new_dacl);
    return false;
  }

  result = ::RegSetKeySecurity(hklm_key.Handle(),
                               DACL_SECURITY_INFORMATION,
                               new_sd);
  // Note that the new DACL cannot be freed until after the call to
  // RegSetKeySecurity().
  LocalFree(new_dacl);

  bool success = true;
  if (result != ERROR_SUCCESS) {
    ASSERT_STRING("rlz_lib::CreateMachineState: "
                  "Unable to create / open machine key.");
    success = false;
  }


  return success;
}

bool SetMachineDealCode(const char* dcc) {
  return MachineDealCode::Set(dcc);
}

bool GetMachineDealCodeAsCgi(char* cgi, size_t cgi_size) {
  return MachineDealCode::GetAsCgi(cgi, cgi_size);
}

bool GetMachineDealCode(char* dcc, size_t dcc_size) {
  return MachineDealCode::Get(dcc, dcc_size);
}

// Combined functions.

bool SetMachineDealCodeFromPingResponse(const char* response) {
  return MachineDealCode::SetFromPingResponse(response);
}

}  // namespace rlz_lib
