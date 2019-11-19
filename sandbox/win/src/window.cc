// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/window.h"

#include <aclapi.h>

#include <memory>

#include "base/logging.h"
#include "base/win/win_util.h"
#include "sandbox/win/src/acl.h"
#include "sandbox/win/src/sid.h"

namespace {

// Gets the security attributes of a window object referenced by |handle|. The
// lpSecurityDescriptor member of the SECURITY_ATTRIBUTES parameter returned
// must be freed using LocalFree by the caller.
bool GetSecurityAttributes(HANDLE handle, SECURITY_ATTRIBUTES* attributes) {
  attributes->bInheritHandle = false;
  attributes->nLength = sizeof(SECURITY_ATTRIBUTES);

  PACL dacl = nullptr;
  DWORD result = ::GetSecurityInfo(
      handle, SE_WINDOW_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr,
      &dacl, nullptr, &attributes->lpSecurityDescriptor);
  if (ERROR_SUCCESS == result)
    return true;

  return false;
}

}  // namespace

namespace sandbox {

ResultCode CreateAltWindowStation(HWINSTA* winsta) {
  // Get the security attributes from the current window station; we will
  // use this as the base security attributes for the new window station.
  HWINSTA current_winsta = ::GetProcessWindowStation();
  if (!current_winsta)
    return SBOX_ERROR_CANNOT_GET_WINSTATION;

  SECURITY_ATTRIBUTES attributes = {0};
  if (!GetSecurityAttributes(current_winsta, &attributes))
    return SBOX_ERROR_CANNOT_QUERY_WINSTATION_SECURITY;

  // Create the window station using nullptr for the name to ask the os to
  // generate it.
  *winsta = ::CreateWindowStationW(
      nullptr, 0, GENERIC_READ | WINSTA_CREATEDESKTOP, &attributes);
  if (!*winsta && ::GetLastError() == ERROR_ACCESS_DENIED) {
    *winsta = ::CreateWindowStationW(
        nullptr, 0, WINSTA_READATTRIBUTES | WINSTA_CREATEDESKTOP, &attributes);
  }
  LocalFree(attributes.lpSecurityDescriptor);

  if (*winsta)
    return SBOX_ALL_OK;

  return SBOX_ERROR_CANNOT_CREATE_WINSTATION;
}

ResultCode CreateAltDesktop(HWINSTA winsta, HDESK* desktop) {
  std::wstring desktop_name = L"sbox_alternate_desktop_";

  if (!winsta) {
    desktop_name += L"local_winstation_";
  }

  // Append the current PID to the desktop name.
  wchar_t buffer[16];
  _snwprintf_s(buffer, sizeof(buffer) / sizeof(wchar_t), L"0x%X",
               ::GetCurrentProcessId());
  desktop_name += buffer;

  HDESK current_desktop = GetThreadDesktop(GetCurrentThreadId());

  if (!current_desktop)
    return SBOX_ERROR_CANNOT_GET_DESKTOP;

  // Get the security attributes from the current desktop, we will use this as
  // the base security attributes for the new desktop.
  SECURITY_ATTRIBUTES attributes = {0};
  if (!GetSecurityAttributes(current_desktop, &attributes))
    return SBOX_ERROR_CANNOT_QUERY_DESKTOP_SECURITY;

  // Back up the current window station, in case we need to switch it.
  HWINSTA current_winsta = ::GetProcessWindowStation();

  if (winsta) {
    // We need to switch to the alternate window station before creating the
    // desktop.
    if (!::SetProcessWindowStation(winsta)) {
      ::LocalFree(attributes.lpSecurityDescriptor);
      return SBOX_ERROR_CANNOT_CREATE_DESKTOP;
    }
  }

  // Create the destkop.
  *desktop = ::CreateDesktop(desktop_name.c_str(), nullptr, nullptr, 0,
                             DESKTOP_CREATEWINDOW | DESKTOP_READOBJECTS |
                                 READ_CONTROL | WRITE_DAC | WRITE_OWNER,
                             &attributes);
  ::LocalFree(attributes.lpSecurityDescriptor);

  if (winsta) {
    // Revert to the right window station.
    if (!::SetProcessWindowStation(current_winsta)) {
      return SBOX_ERROR_FAILED_TO_SWITCH_BACK_WINSTATION;
    }
  }

  if (*desktop) {
    // Replace the DACL on the new Desktop with a reduced privilege version.
    // We can soft fail on this for now, as it's just an extra mitigation.
    static const ACCESS_MASK kDesktopDenyMask =
        WRITE_DAC | WRITE_OWNER | DELETE | DESKTOP_CREATEMENU |
        DESKTOP_CREATEWINDOW | DESKTOP_HOOKCONTROL | DESKTOP_JOURNALPLAYBACK |
        DESKTOP_JOURNALRECORD | DESKTOP_SWITCHDESKTOP;
    AddKnownSidToObject(*desktop, SE_WINDOW_OBJECT, Sid(WinRestrictedCodeSid),
                        DENY_ACCESS, kDesktopDenyMask);
    return SBOX_ALL_OK;
  }

  return SBOX_ERROR_CANNOT_CREATE_DESKTOP;
}

std::wstring GetFullDesktopName(HWINSTA winsta, HDESK desktop) {
  if (!desktop) {
    NOTREACHED();
    return std::wstring();
  }

  std::wstring name;
  if (winsta) {
    name = base::win::GetWindowObjectName(winsta);
    name += L'\\';
  }

  name += base::win::GetWindowObjectName(desktop);
  return name;
}

}  // namespace sandbox
