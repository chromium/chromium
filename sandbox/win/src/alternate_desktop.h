// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_ALTERNATE_DESKTOP_H_
#define SANDBOX_WIN_SRC_ALTERNATE_DESKTOP_H_

#include "sandbox/win/src/sandbox.h"

#include <string>

namespace sandbox {
// Holds handles and integrity levels about alternate desktops.
class AlternateDesktop {
 public:
  AlternateDesktop()
      : desktop_(nullptr),
        winstation_(nullptr),
        integrity_(INTEGRITY_LEVEL_SYSTEM) {}
  ~AlternateDesktop();
  AlternateDesktop(const AlternateDesktop&) = delete;
  AlternateDesktop& operator=(const AlternateDesktop&) = delete;

  // Updates the desktop token's integrity level to be no higher than
  // `integrity_level`.
  ResultCode UpdateDesktopIntegrity(IntegrityLevel integrity_level);

  // Populate this object, creating a winstation if `alternate_winstation` is
  // true.
  ResultCode Initialize(bool alternate_winstation);

  std::wstring GetDesktopName();

 private:
  // Handle for the alternate desktop.
  HDESK desktop_;
  // Winstation for the alternate desktop, or nullptr if not used.
  HWINSTA winstation_;
  // Last set integrity level of the desktop.
  IntegrityLevel integrity_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_ALTERNATE_DESKTOP_H_
