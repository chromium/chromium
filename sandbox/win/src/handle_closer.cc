// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/handle_closer.h"

#include <stddef.h>

#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

void HandleCloser::AddHandle(HandleToClose handle_info) {
  handles_to_close_.handle_closer_enabled = true;
  switch (handle_info) {
    case HandleToClose::kWindowsShellGlobalCounters:
      handles_to_close_.section_windows_global_shell_counters = true;
      break;
    case HandleToClose::kDeviceApi:
      handles_to_close_.file_device_api = true;
      break;
    case HandleToClose::kKsecDD:
      handles_to_close_.file_ksecdd = true;
      break;
    case HandleToClose::kDisconnectCsrss:
      handles_to_close_.disconnect_csrss = true;
      break;
  }
}

bool HandleCloser::InitializeTargetHandles(TargetProcess& target) {
  // Do nothing on an empty list (target's config already initialized to zero).
  if (!handles_to_close_.handle_closer_enabled) {
    return true;
  }

  static_assert(sizeof(g_handle_closer_info) == sizeof(handles_to_close_));
  ResultCode rc = target.TransferVariable(
      "g_handle_closer_info", &handles_to_close_, &g_handle_closer_info,
      sizeof(g_handle_closer_info));

  return (SBOX_ALL_OK == rc);
}

}  // namespace sandbox
