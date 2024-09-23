// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Information about the current process.

#include "rlz/win/lib/process_info.h"

#include <windows.h>

#include <stddef.h>

#include <string>

#include "base/process/process_info.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "rlz/lib/assert.h"

namespace {

HRESULT GetElevationType(PTOKEN_ELEVATION_TYPE elevation) {
  if (!elevation)
    return E_POINTER;

  *elevation = TokenElevationTypeDefault;

  HANDLE process_token;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token))
    return HRESULT_FROM_WIN32(GetLastError());

  base::win::ScopedHandle scoped_process_token(process_token);

  DWORD size;
  TOKEN_ELEVATION_TYPE elevation_type;
  if (!GetTokenInformation(process_token, TokenElevationType, &elevation_type,
                           sizeof(elevation_type), &size)) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  *elevation = elevation_type;
  return S_OK;
}

}  //anonymous


namespace rlz_lib {

bool ProcessInfo::IsRunningAsSystem() {
  static std::wstring user_sid;
  if (user_sid.empty()) {
    if (!base::win::GetUserSidString(&user_sid))
      return false;
  }
  return (user_sid == L"S-1-5-18");
}

bool ProcessInfo::HasAdminRights() {
  static bool evaluated = false;
  static bool has_rights = false;

  if (!evaluated) {
    if (IsRunningAsSystem()) {
      has_rights = true;
    } else {
      TOKEN_ELEVATION_TYPE elevation;
      if (SUCCEEDED(GetElevationType(&elevation))) {
        base::IntegrityLevel level = base::GetCurrentProcessIntegrityLevel();
        if (level != base::INTEGRITY_UNKNOWN) {
          has_rights = (elevation == TokenElevationTypeFull) ||
                       (level == base::HIGH_INTEGRITY);
        }
      }
    }
  }

  evaluated = true;
  if (!has_rights)
    ASSERT_STRING("ProcessInfo::HasAdminRights: Does not have admin rights.");

  return has_rights;
}

}  // namespace rlz_lib
