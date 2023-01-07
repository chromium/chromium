// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/hid/hid_usage_and_page.h"

namespace device {

bool IsAlwaysProtected(const mojom::HidUsageAndPage& hid_usage_and_page) {
  const uint16_t usage = hid_usage_and_page.usage;
  const uint16_t usage_page = hid_usage_and_page.usage_page;

  if (usage_page == mojom::kPageKeyboard)
    return true;

  if (usage_page != mojom::kPageGenericDesktop)
    return false;

  if (usage == mojom::kGenericDesktopPointer ||
      usage == mojom::kGenericDesktopMouse ||
      usage == mojom::kGenericDesktopKeyboard ||
      usage == mojom::kGenericDesktopKeypad) {
    return true;
  }

  if (usage >= mojom::kGenericDesktopSystemControl &&
      usage <= mojom::kGenericDesktopSystemWarmRestart) {
    return true;
  }

  if (usage >= mojom::kGenericDesktopSystemDock &&
      usage <= mojom::kGenericDesktopSystemDisplaySwap) {
    return true;
  }

  return false;
}

}  // namespace device
