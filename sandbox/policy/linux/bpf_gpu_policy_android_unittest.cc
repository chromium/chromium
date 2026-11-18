// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_gpu_policy_android.h"

#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/debug/crash_logging.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_test_runner.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox::policy {
namespace {

void WriteAllStdErr(std::string_view piece) {
  base::span<const char> remaining(piece);
  while (!remaining.empty()) {
    const ssize_t written = HANDLE_EINTR(
        sandbox::sys_write(STDERR_FILENO, remaining.data(), remaining.size()));
    if (written <= 0 || static_cast<size_t>(written) > remaining.size()) {
      break;
    }
    remaining = remaining.subspan(static_cast<size_t>(written));
  }
}

size_t GetSystemPageSize() {
  const long page_size = sysconf(_SC_PAGESIZE);
  SANDBOX_ASSERT(page_size > 0);
  return static_cast<size_t>(page_size);
}

// Records allocated crash keys and emits async-signal-safe "name=value" lines
// to stderr when Set() is invoked from a SIGSYS trap handler.
class SignalSafeCrashKeyImplementation
    : public base::debug::CrashKeyImplementation {
 public:
  SignalSafeCrashKeyImplementation() = default;
  SignalSafeCrashKeyImplementation(const SignalSafeCrashKeyImplementation&) =
      delete;
  SignalSafeCrashKeyImplementation& operator=(
      const SignalSafeCrashKeyImplementation&) = delete;
  ~SignalSafeCrashKeyImplementation() override = default;

  base::debug::CrashKeyString* Allocate(
      const char name[],
      base::debug::CrashKeySize size) override {
    allocated_names_.emplace_back(name);
    keys_.push_back(std::make_unique<base::debug::CrashKeyString>(name, size));
    return keys_.back().get();
  }

  void Set(base::debug::CrashKeyString* crash_key,
           std::string_view value) override {
    if (!crash_key || !crash_key->name) {
      return;
    }
    WriteAllStdErr(crash_key->name);
    WriteAllStdErr("=");
    WriteAllStdErr(value);
    WriteAllStdErr("\n");
  }

  void Clear(base::debug::CrashKeyString* crash_key) override {}

  void OutputCrashKeysToStream(std::ostream& out) override {}

  bool HasAllocatedKey(std::string_view name) const {
    return std::ranges::contains(allocated_names_, name);
  }

 private:
  std::vector<std::string> allocated_names_;
  std::vector<std::unique_ptr<base::debug::CrashKeyString>> keys_;
};

class GpuPolicyCrashKeyDelegate : public BPFTesterDelegate {
 public:
  GpuPolicyCrashKeyDelegate() = default;
  GpuPolicyCrashKeyDelegate(const GpuPolicyCrashKeyDelegate&) = delete;
  GpuPolicyCrashKeyDelegate& operator=(const GpuPolicyCrashKeyDelegate&) =
      delete;
  ~GpuPolicyCrashKeyDelegate() override = default;

  std::unique_ptr<bpf_dsl::Policy> GetSandboxBPFPolicy() override {
    sandbox::ResetCrashKeysForTesting();
    base::debug::SetCrashKeyImplementation(
        std::make_unique<SignalSafeCrashKeyImplementation>());
    return std::make_unique<GpuProcessPolicyAndroid>();
  }

  void RunTestFunction() override {
    const size_t page_size = GetSystemPageSize();
    mmap(nullptr, page_size, PROT_READ | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS,
         -1, 0);
  }
};

SANDBOX_TEST(GpuProcessPolicyAndroid, AllocatesSeccompCrashKeys) {
  sandbox::ResetCrashKeysForTesting();
  auto impl = std::make_unique<SignalSafeCrashKeyImplementation>();
  SignalSafeCrashKeyImplementation* impl_ptr = impl.get();
  base::debug::SetCrashKeyImplementation(std::move(impl));

  GpuProcessPolicyAndroid policy;
  const bool has_sigsys_key = impl_ptr->HasAllocatedKey("seccomp-sigsys");
  const bool has_ioctl_key = impl_ptr->HasAllocatedKey("seccomp-sigsys-ioctl");

  impl_ptr = nullptr;
  sandbox::ResetCrashKeysForTesting();
  base::debug::SetCrashKeyImplementation(nullptr);

  // Verify ResetCrashKeysForTesting() allows a replacement implementation to
  // reallocate fresh crash keys without retaining dangling pointers.
  auto replacement_impl = std::make_unique<SignalSafeCrashKeyImplementation>();
  SignalSafeCrashKeyImplementation* replacement_ptr = replacement_impl.get();
  base::debug::SetCrashKeyImplementation(std::move(replacement_impl));

  GpuProcessPolicyAndroid second_policy;
  const bool reallocated_sigsys_key =
      replacement_ptr->HasAllocatedKey("seccomp-sigsys");
  const bool reallocated_ioctl_key =
      replacement_ptr->HasAllocatedKey("seccomp-sigsys-ioctl");

  replacement_ptr = nullptr;
  sandbox::ResetCrashKeysForTesting();
  base::debug::SetCrashKeyImplementation(nullptr);

  SANDBOX_ASSERT(has_sigsys_key);
  SANDBOX_ASSERT(has_ioctl_key);
  SANDBOX_ASSERT(reallocated_sigsys_key);
  SANDBOX_ASSERT(reallocated_ioctl_key);
}

BPF_DEATH_TEST_D(GpuProcessPolicyAndroid,
                 MmapProtExecSetsSeccompCrashKey,
                 DEATH_SEGV_MESSAGE_PATTERN("*seccomp-sigsys=nr=0x*"),
                 GpuPolicyCrashKeyDelegate)

BPF_TEST_C(GpuProcessPolicyAndroid,
           NonExecMmapAndMprotectAllowed,
           GpuProcessPolicyAndroid) {
  const size_t page_size = GetSystemPageSize();
  void* ptr = mmap(nullptr, page_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  BPF_ASSERT_NE(MAP_FAILED, ptr);
  BPF_ASSERT_EQ(0, mprotect(ptr, page_size, PROT_READ));
  BPF_ASSERT_EQ(0, munmap(ptr, page_size));
}

BPF_DEATH_TEST_C(GpuProcessPolicyAndroid,
                 MprotectProtExecBlocked,
                 DEATH_SEGV_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 GpuProcessPolicyAndroid) {
  const size_t page_size = GetSystemPageSize();
  void* ptr = mmap(nullptr, page_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  BPF_ASSERT_NE(MAP_FAILED, ptr);
  mprotect(ptr, page_size, PROT_READ | PROT_EXEC);
}

}  // namespace
}  // namespace sandbox::policy
