// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/alternate_desktop.h"

#include <windows.h>

#include "base/win/win_util.h"
#include "sandbox/win/src/acl.h"
#include "sandbox/win/src/window.h"

namespace sandbox {

AlternateDesktop::~AlternateDesktop() {
  if (desktop_) {
    ::CloseDesktop(desktop_);
    desktop_ = nullptr;
  }
  if (winstation_) {
    ::CloseWindowStation(winstation_);
    winstation_ = nullptr;
  }
}

// Updates the desktop token's integrity level to be no higher than
// `integrity_level`.
ResultCode AlternateDesktop::UpdateDesktopIntegrity(
    IntegrityLevel integrity_level) {
  // Integrity label enum is reversed (higher level is a lower value).
  static_assert(INTEGRITY_LEVEL_SYSTEM < INTEGRITY_LEVEL_UNTRUSTED,
                "Integrity level ordering reversed.");
  DCHECK(integrity_level != INTEGRITY_LEVEL_LAST);
  // Require that the desktop has been set.
  if (!desktop_)
    return SBOX_ERROR_CANNOT_CREATE_DESKTOP;
  if (integrity_ < integrity_level) {
    DWORD result = SetObjectIntegrityLabel(
        desktop_, base::win::SecurityObjectType::kDesktop, 0, integrity_level);
    if (ERROR_SUCCESS != result)
      return SBOX_ERROR_CANNOT_SET_DESKTOP_INTEGRITY;
    integrity_ = integrity_level;
  }
  return SBOX_ALL_OK;
}

// Populate this object, creating a winstation if `alternate_winstation` is
// true.
ResultCode AlternateDesktop::Initialize(bool alternate_winstation) {
  DCHECK(!desktop_ && !winstation_);
  if (alternate_winstation) {
    // Create the window station.
    ResultCode result = CreateAltWindowStation(&winstation_);
    if (SBOX_ALL_OK != result)
      return result;

    // Verify that everything is fine.
    if (!winstation_ || base::win::GetWindowObjectName(winstation_).empty()) {
      return SBOX_ERROR_CANNOT_CREATE_DESKTOP;
    }
  }
  ResultCode result = CreateAltDesktop(winstation_, &desktop_);
  if (SBOX_ALL_OK != result)
    return result;

  // Verify that everything is fine.
  if (!desktop_ || base::win::GetWindowObjectName(desktop_).empty()) {
    return SBOX_ERROR_CANNOT_CREATE_DESKTOP;
  }
  return SBOX_ALL_OK;
}

std::wstring AlternateDesktop::GetDesktopName() {
  if (!desktop_)
    return std::wstring();
  return GetFullDesktopName(winstation_, desktop_);
}

}  // namespace sandbox
