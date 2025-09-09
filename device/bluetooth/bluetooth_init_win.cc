// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_init_win.h"

#include <optional>

#include "base/threading/scoped_thread_priority.h"
#include "base/win/delayload_helpers.h"

namespace device::bluetooth_init_win {

bool HasBluetoothStack() {
  static std::optional<bool> has_bluetooth_stack;

  if (!has_bluetooth_stack.has_value()) {
    // Mitigate the issues caused by loading DLLs on a background thread
    // (http://crbug/973868).
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY_REPEATEDLY();

    has_bluetooth_stack =
        base::win::LoadAllImportsForDll("bthprops.cpl").value_or(false);
  }

  return *has_bluetooth_stack;
}

}  // namespace device::bluetooth_init_win
