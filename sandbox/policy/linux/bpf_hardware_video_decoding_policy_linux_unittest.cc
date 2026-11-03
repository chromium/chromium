// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_hardware_video_decoding_policy_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "sandbox/policy/linux/bpf_base_policy_linux.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox::policy {
namespace {

using bpf_dsl::Allow;
using bpf_dsl::ResultExpr;
using PolicyType = HardwareVideoDecodingProcessPolicy::PolicyType;

bool IsMemfdCreateOrFcntlOrPwrite(int sysno) {
  switch (sysno) {
    case __NR_memfd_create:
    case __NR_fcntl:
#if defined(__NR_fcntl64)
    case __NR_fcntl64:
#endif
    case __NR_pwrite64:
      return true;
    default:
      return false;
  }
}

// The production policy cannot be installed as-is in a test process: anything
// it does not handle itself is first offered to the syscall broker, which is
// not running here. This wrapper therefore delegates only the system calls
// under test:
//
// - truncate()/ftruncate() go to the real HardwareVideoDecodingProcessPolicy,
//   so the filter installed for them is exactly what ships.
// - memfd_create(), fcntl() and pwrite() go to BPFBasePolicy, which is what
//   the production policy falls through to for them (it has no rules of its
//   own for any of them). This pins down that the baseline allows the memfd
//   flags, the seals and the write needed to create a sealed shared memory
//   region of a given size in-process.
//
// Everything else is allowed so that the test body can run.
template <PolicyType kPolicyType>
class HardwareVideoDecodingPolicyWrapper : public bpf_dsl::Policy {
 public:
  HardwareVideoDecodingPolicyWrapper() = default;
  ~HardwareVideoDecodingPolicyWrapper() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    if (SyscallSets::IsTruncate(sysno)) {
      return real_policy_.EvaluateSyscall(sysno);
    }
    if (IsMemfdCreateOrFcntlOrPwrite(sysno)) {
      return base_policy_.EvaluateSyscall(sysno);
    }
    return Allow();
  }

 private:
  HardwareVideoDecodingProcessPolicy real_policy_{kPolicyType};
  BPFBasePolicy base_policy_;
};

using VaapiOnIntelPolicy =
    HardwareVideoDecodingPolicyWrapper<PolicyType::kVaapiOnIntel>;
using VaapiOnAMDPolicy =
    HardwareVideoDecodingPolicyWrapper<PolicyType::kVaapiOnAMD>;
using V4L2Policy = HardwareVideoDecodingPolicyWrapper<PolicyType::kV4L2>;
using VaapiAndV4L2Policy =
    HardwareVideoDecodingPolicyWrapper<PolicyType::kVaapiAndV4L2>;
using VaapiOnAMDAndV4L2Policy =
    HardwareVideoDecodingPolicyWrapper<PolicyType::kVaapiOnAMDAndV4L2>;

constexpr int kRegionSeals = F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL;

base::ScopedFD CreateMemfd() {
  base::ScopedFD fd(static_cast<int>(syscall(
      __NR_memfd_create, "hwvd_policy_test", MFD_CLOEXEC | MFD_ALLOW_SEALING)));
  BPF_ASSERT(fd.is_valid());
  return fd;
}

// Truncation must be refused with EPERM - not SIGSYS - on every path, so that
// base can fall back to pwrite() for sizing its own memfds, and must happen
// before any path lookup for the path-based variant.
void ExpectTruncateRefusedWithEperm() {
  base::ScopedFD fd = CreateMemfd();

  errno = 0;
  BPF_ASSERT_EQ(-1, HANDLE_EINTR(ftruncate(fd.get(), 4096)));
  BPF_ASSERT_EQ(EPERM, errno);

  struct stat st;
  BPF_ASSERT_EQ(0, fstat(fd.get(), &st));
  BPF_ASSERT_EQ(0, st.st_size);

  errno = 0;
  BPF_ASSERT_EQ(-1, truncate("/this/path/does/not/exist", 0));
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST_C(HardwareVideoDecodingPolicy,
           TruncateRefusedWithEpermVaapiOnIntel,
           VaapiOnIntelPolicy) {
  ExpectTruncateRefusedWithEperm();
}

BPF_TEST_C(HardwareVideoDecodingPolicy,
           TruncateRefusedWithEpermVaapiOnAMD,
           VaapiOnAMDPolicy) {
  ExpectTruncateRefusedWithEperm();
}

BPF_TEST_C(HardwareVideoDecodingPolicy,
           TruncateRefusedWithEpermV4L2,
           V4L2Policy) {
  ExpectTruncateRefusedWithEperm();
}

BPF_TEST_C(HardwareVideoDecodingPolicy,
           TruncateRefusedWithEpermVaapiAndV4L2,
           VaapiAndV4L2Policy) {
  ExpectTruncateRefusedWithEperm();
}

BPF_TEST_C(HardwareVideoDecodingPolicy,
           TruncateRefusedWithEpermVaapiOnAMDAndV4L2,
           VaapiOnAMDAndV4L2Policy) {
  ExpectTruncateRefusedWithEperm();
}

// The sequence a process needs to create a sealed shared memory region of a
// given size in-process when ftruncate() is refused: memfd_create(), pwrite()
// of the last byte to extend the file, then F_ADD_SEALS. All of it must be
// permitted, and the region must stay unresizable afterwards.
BPF_TEST_C(HardwareVideoDecodingPolicy,
           MemfdCanBeSizedWithPwriteAndSealed,
           VaapiOnIntelPolicy) {
  const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
  const size_t size = 4 * page_size;
  base::ScopedFD fd = CreateMemfd();

  errno = 0;
  BPF_ASSERT_EQ(-1, HANDLE_EINTR(ftruncate(fd.get(), size)));
  BPF_ASSERT_EQ(EPERM, errno);

  static constexpr char kZero = 0;
  BPF_ASSERT_EQ(1, HANDLE_EINTR(pwrite(fd.get(), &kZero, 1, size - 1)));

  struct stat st;
  BPF_ASSERT_EQ(0, fstat(fd.get(), &st));
  BPF_ASSERT_EQ(static_cast<off_t>(size), st.st_size);
  // Only the page holding the written byte is instantiated.
  BPF_ASSERT_GT(st.st_blocks, 0);
  BPF_ASSERT(static_cast<size_t>(st.st_blocks) * 512 <= page_size);

  BPF_ASSERT_EQ(0, fcntl(fd.get(), F_ADD_SEALS, kRegionSeals));
  BPF_ASSERT_EQ(kRegionSeals, fcntl(fd.get(), F_GET_SEALS));

  // Still unresizable afterwards (and F_SEAL_GROW now also stops pwrite()
  // from extending the file), and still fully usable.
  errno = 0;
  BPF_ASSERT_EQ(-1, HANDLE_EINTR(ftruncate(fd.get(), 0)));
  BPF_ASSERT_EQ(EPERM, errno);
  errno = 0;
  BPF_ASSERT_EQ(-1, HANDLE_EINTR(pwrite(fd.get(), &kZero, 1, size)));
  BPF_ASSERT_EQ(EPERM, errno);

  // Mapping still works. Map only the file's last page (the one pwrite()
  // instantiated), write its first byte through the mapping and read that
  // same file offset back through the descriptor.
  const off_t last_page_offset = static_cast<off_t>(size - page_size);
  void* last_page = mmap(nullptr, page_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                         fd.get(), last_page_offset);
  BPF_ASSERT_NE(MAP_FAILED, last_page);
  auto* byte = static_cast<unsigned char*>(last_page);
  BPF_ASSERT_EQ(0, *byte);
  *byte = 2;
  BPF_ASSERT_EQ(0, munmap(last_page, page_size));
  char read_back = 0;
  BPF_ASSERT_EQ(1,
                HANDLE_EINTR(pread(fd.get(), &read_back, 1, last_page_offset)));
  BPF_ASSERT_EQ(2, read_back);
}

// Shared memory regions are sealed with F_SEAL_SHRINK | F_SEAL_GROW |
// F_SEAL_SEAL. Any other seal is outside the baseline allowlist and kills the
// process, so a change to that set in a client must come with a sandbox change.
BPF_DEATH_TEST_C(HardwareVideoDecodingPolicy,
                 SealOutsideAllowlistCrashes,
                 DEATH_SEGV_MESSAGE(GetErrorMessageContentForTests()),
                 VaapiOnIntelPolicy) {
  base::ScopedFD fd = CreateMemfd();
  BPF_ASSERT_EQ(0, fcntl(fd.get(), F_ADD_SEALS, kRegionSeals));
  // This should crash.
  fcntl(fd.get(), F_ADD_SEALS, F_SEAL_WRITE);
}

}  // namespace
}  // namespace sandbox::policy
