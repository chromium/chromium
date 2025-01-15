// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mac/task_port_policy.h"

#include "base/debug/crash_logging.h"
#include "base/strings/stringprintf.h"

extern "C" {
int __mac_syscall(const char* policy, int op, void* arg);
}

namespace content {

namespace {

// From AppleMobileFileIntegrity.kext`policy_syscall() on macOS 12.4 21F79.
enum AmfiStatusFlags {
  // Boot arg: `amfi_unrestrict_task_for_pid`.
  kAmfiOverrideUnrestrictedDebugging = 1 << 0,
  // Boot arg: `amfi_allow_any_signature`.
  kAmfiAllowInvalidSignatures = 1 << 1,
  // Boot arg: `amfi_get_out_of_my_way`.
  kAmfiAllowEverything = 1 << 2,
};

}  // namespace

bool MachTaskPortPolicy::AmfiIsAllowEverything() const {
  return (amfi_status & kAmfiAllowEverything) != 0;
}

base::expected<MachTaskPortPolicy, int> GetMachTaskPortPolicy() {
  MachTaskPortPolicy policy;

  // Undocumented MACF system call to Apple Mobile File Integrity.kext. In
  // macOS 12.4 21F79 (and at least back to macOS 12.0), this returns a
  // bitmask containing the AMFI status flags.
  if (__mac_syscall("AMFI", 0x60, &policy.amfi_status) != 0) {
    return base::unexpected(errno);
  }

  return policy;
}

void SetSystemPolicyCrashKeys() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "amfi-status", base::debug::CrashKeySize::Size64);

  auto task_port_policy = GetMachTaskPortPolicy();
  if (task_port_policy.has_value()) {
    base::debug::SetCrashKeyString(
        crash_key,
        base::StringPrintf("rv=0 status=0x%llx allow_everything=%d",
                           task_port_policy->amfi_status,
                           task_port_policy->AmfiIsAllowEverything()));
  } else {
    base::debug::SetCrashKeyString(
        crash_key, base::StringPrintf("rv=%d", task_port_policy.error()));
  }
}

}  // namespace content
