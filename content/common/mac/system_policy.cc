// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mac/system_policy.h"

#include "base/debug/crash_logging.h"
#include "base/strings/stringprintf.h"

extern "C" {
int __mac_syscall(const char* policy, int op, void* arg);
int csr_get_active_config(uint32_t* config);
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

// From
// https://github.com/apple-oss-distributions/xnu/blob/xnu-11215.41.3/bsd/sys/csr.h
enum SystemSecurityPolicy : uint32_t {
  AllowUntrustedKernelExtensions = 1 << 0,
  AllowUnrestrictedFileSystem = 1 << 1,
  AllowTaskForPid = 1 << 2,
  AllowKernelDebugger = 1 << 3,
  AllowAppleInternal = 1 << 4,
  AllowUnrestrictedDTrace = 1 << 5,
  AllowUnrestrictedNVRAM = 1 << 6,
  AllowDeviceConfiguration = 1 << 7,
  AllowAnyRecoveryOS = 1 << 8,
  AllowUnapprovedKernelExtensions = 1 << 9,
  AllowExecutablePolicyOverride = 1 << 10,
  AllowUnauthenticatedRoot = 1 < 11,
  AllowResearchGuests = 1 << 12,
};

base::expected<SystemSecurityPolicy, int> GetSystemSecurityPolicy() {
  uint32_t policy = 0;
  if (int error = csr_get_active_config(&policy)) {
    return base::unexpected(error);
  }

  return base::expected<SystemSecurityPolicy, int>(
      static_cast<SystemSecurityPolicy>(policy));
}

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
  static auto* task_port_policy_crash_key = base::debug::AllocateCrashKeyString(
      "amfi-status", base::debug::CrashKeySize::Size64);
  static auto* system_security_policy_crash_key =
      base::debug::AllocateCrashKeyString("system-integrity-protection",
                                          base::debug::CrashKeySize::Size64);

  auto task_port_policy = GetMachTaskPortPolicy();
  if (task_port_policy.has_value()) {
    base::debug::SetCrashKeyString(
        task_port_policy_crash_key,
        base::StringPrintf("rv=0 status=0x%llx allow_everything=%d",
                           task_port_policy->amfi_status,
                           task_port_policy->AmfiIsAllowEverything()));
  } else {
    base::debug::SetCrashKeyString(
        task_port_policy_crash_key,
        base::StringPrintf("rv=%d", task_port_policy.error()));
  }

  auto system_security_policy = GetSystemSecurityPolicy();
  if (system_security_policy.has_value()) {
    base::debug::SetCrashKeyString(
        system_security_policy_crash_key,
        base::StringPrintf(
            "rv=0 status=0x%llx task_for_pid=%d kernel_debugger=%d",
            system_security_policy.value(),
            (system_security_policy.value() & AllowTaskForPid) != 0,
            (system_security_policy.value() & AllowKernelDebugger) != 0));

  } else {
    base::debug::SetCrashKeyString(
        system_security_policy_crash_key,
        base::StringPrintf("rv=%d", task_port_policy.error()));
  }
}

}  // namespace content
