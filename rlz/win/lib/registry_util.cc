// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A helper library to keep track of a user's key by SID.
// Used by RLZ libary. Also to be used by SearchWithGoogle library.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "rlz/win/lib/registry_util.h"

#include <windows.h>

#include "base/process/process_info.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "rlz/lib/assert.h"
#include "rlz/win/lib/process_info.h"

namespace rlz_lib {

bool RegKeyReadValue(const base::win::RegKey& key, const wchar_t* name,
                     char* value, size_t* value_size) {
  value[0] = 0;

  std::wstring value_string;
  if (key.ReadValue(name, &value_string) != ERROR_SUCCESS) {
    return false;
  }

  if (value_string.length() > *value_size) {
    *value_size = value_string.length();
    return false;
  }

  // Note that RLZ string are always ASCII by design.
  strncpy(value, base::WideToUTF8(value_string).c_str(), *value_size);
  value[*value_size - 1] = 0;
  return true;
}

bool RegKeyWriteValue(base::win::RegKey* key, const wchar_t* name,
                      const char* value) {
  std::wstring value_string(base::ASCIIToWide(value));
  return key->WriteValue(name, value_string.c_str()) == ERROR_SUCCESS;
}

bool HasUserKeyAccess(bool write_access) {
  // The caller is trying to access HKEY_CURRENT_USER.  Test to see if we can
  // read from there.  Don't try HKEY_CURRENT_USER because this will cause
  // problems in the unit tests: if we open HKEY_CURRENT_USER directly here,
  // the overriding done for unit tests will no longer work.  So we try subkey
  // "Software" which is known to always exist.
  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER, L"Software", KEY_READ) != ERROR_SUCCESS)
    ASSERT_STRING("Could not open HKEY_CURRENT_USER");

  if (ProcessInfo::IsRunningAsSystem()) {
    ASSERT_STRING("UserKey::HasAccess: No access as SYSTEM without SID set.");
    return false;
  }

  if (write_access) {
    if (base::GetCurrentProcessIntegrityLevel() <= base::LOW_INTEGRITY) {
      ASSERT_STRING("UserKey::HasAccess: Cannot write from Low Integrity.");
      return false;
    }
  }
  return true;
}

}  // namespace rlz_lib
