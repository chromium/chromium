// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/window.h"

#include <windows.h>

#include <optional>

#include "base/notreached.h"
#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#include "base/win/win_util.h"

namespace sandbox {

ResultCode CreateAltWindowStation(HWINSTA* winsta) {
  // Get the security attributes from the current window station; we will
  // use this as the base security attributes for the new window station.
  HWINSTA current_winsta = ::GetProcessWindowStation();
  if (!current_winsta)
    return SBOX_ERROR_CANNOT_GET_WINSTATION;

  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromHandle(
          current_winsta, base::win::SecurityObjectType::kWindowStation,
          DACL_SECURITY_INFORMATION);
  if (!sd) {
    return SBOX_ERROR_CANNOT_QUERY_WINSTATION_SECURITY;
  }

  SECURITY_DESCRIPTOR sd_absolute;
  sd->ToAbsolute(sd_absolute);
  SECURITY_ATTRIBUTES attributes = {sizeof(SECURITY_ATTRIBUTES), &sd_absolute,
                                    FALSE};

  // Create the window station using nullptr for the name to ask the os to
  // generate it.
  *winsta = ::CreateWindowStationW(
      nullptr, 0, GENERIC_READ | WINSTA_CREATEDESKTOP, &attributes);
  if (!*winsta && ::GetLastError() == ERROR_ACCESS_DENIED) {
    *winsta = ::CreateWindowStationW(
        nullptr, 0, WINSTA_READATTRIBUTES | WINSTA_CREATEDESKTOP, &attributes);
  }

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
  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromHandle(
          current_desktop, base::win::SecurityObjectType::kDesktop,
          DACL_SECURITY_INFORMATION);
  if (!sd) {
    return SBOX_ERROR_CANNOT_QUERY_DESKTOP_SECURITY;
  }

  if (sd->dacl() && sd->dacl()->is_null()) {
    // If the desktop had a NULL DACL, it allowed access to everything. When
    // we apply a new ACE with |kDesktopDenyMask| below, a NULL DACL would be
    // replaced with a new DACL with one ACE that denies access - which means
    // there is no ACE to allow anything access to the desktop. In this case,
    // replace the NULL DACL with one that has a single ACE that allows access
    // to everyone, so the desktop remains accessible when we further modify
    // the DACL. Also need WinBuiltinAnyPackageSid for AppContainer processes.
    sd->SetDaclEntry(base::win::WellKnownSid::kAllApplicationPackages,
                     base::win::SecurityAccessMode::kGrant, GENERIC_ALL, 0);
    sd->SetDaclEntry(base::win::WellKnownSid::kWorld,
                     base::win::SecurityAccessMode::kGrant, GENERIC_ALL, 0);
  }

  // Replace the DACL on the new Desktop with a reduced privilege version.
  // We can soft fail on this for now, as it's just an extra mitigation.
  static const ACCESS_MASK kDesktopDenyMask =
      WRITE_DAC | WRITE_OWNER | DELETE | DESKTOP_CREATEMENU |
      DESKTOP_CREATEWINDOW | DESKTOP_HOOKCONTROL | DESKTOP_JOURNALPLAYBACK |
      DESKTOP_JOURNALRECORD | DESKTOP_SWITCHDESKTOP;
  sd->SetDaclEntry(base::win::WellKnownSid::kRestricted,
                   base::win::SecurityAccessMode::kDeny, kDesktopDenyMask, 0);

  SECURITY_DESCRIPTOR sd_absolute;
  sd->ToAbsolute(sd_absolute);
  SECURITY_ATTRIBUTES attributes = {sizeof(SECURITY_ATTRIBUTES), &sd_absolute,
                                    FALSE};

  // Back up the current window station, in case we need to switch it.
  HWINSTA current_winsta = ::GetProcessWindowStation();

  if (winsta) {
    // We need to switch to the alternate window station before creating the
    // desktop.
    if (!::SetProcessWindowStation(winsta)) {
      return SBOX_ERROR_CANNOT_CREATE_DESKTOP;
    }
  }

  // Create the destkop.
  *desktop = ::CreateDesktop(desktop_name.c_str(), nullptr, nullptr, 0,
                             DESKTOP_CREATEWINDOW | DESKTOP_READOBJECTS |
                                 READ_CONTROL | WRITE_DAC | WRITE_OWNER,
                             &attributes);

  if (winsta) {
    // Revert to the right window station.
    if (!::SetProcessWindowStation(current_winsta)) {
      return SBOX_ERROR_FAILED_TO_SWITCH_BACK_WINSTATION;
    }
  }

  if (*desktop) {
    return SBOX_ALL_OK;
  }

  return SBOX_ERROR_CANNOT_CREATE_DESKTOP;
}

std::wstring GetFullDesktopName(HWINSTA winsta, HDESK desktop) {
  if (!desktop) {
    NOTREACHED();
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
