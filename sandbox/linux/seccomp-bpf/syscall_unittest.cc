// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/seccomp-bpf/syscall.h"

#include <asm/unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <vector>

#include "base/memory/page_size.h"
#include "base/memory/raw_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/types/fixed_array.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;
using sandbox::bpf_dsl::Trap;

namespace sandbox {

namespace {

TEST(Syscall, InvalidCallReturnsENOSYS) {
  EXPECT_EQ(-ENOSYS, Syscall::InvalidCall());
}

// Loads a `T`, possibly unaligned, from `ptr - sizeof(T)`.
template <typename T>
T LoadBehind(intptr_t ptr) {
  T ret;
  memcpy(&ret, reinterpret_cast<const void*>(ptr - sizeof(T)), sizeof(ret));
  return ret;
}

TEST(Syscall, WellKnownEntryPoint) {
// Test that Syscall::Call(-1) is handled specially. Don't do this on ARM,
// where syscall(-1) crashes with SIGILL. Not running the test is fine, as we
// are still testing ARM code in the next set of tests.
#if !defined(__arm__) && !defined(__aarch64__)
  EXPECT_NE(Syscall::Call(-1), syscall(-1));
#endif

// If possible, test that Syscall::Call(-1) returns the address right
// after a kernel entry point.
#if defined(__i386__)
  EXPECT_EQ(0x80CDu, LoadBehind<uint16_t>(Syscall::Call(-1)));  // INT 0x80
#elif defined(__x86_64__)
  EXPECT_EQ(0x050Fu, LoadBehind<uint16_t>(Syscall::Call(-1)));  // SYSCALL
#elif defined(__arm__)
#if defined(__thumb__)
  EXPECT_EQ(0xDF00u, LoadBehind<uint16_t>(Syscall::Call(-1)));  // SWI 0
#else
  EXPECT_EQ(0xEF000000u, LoadBehind<uint32_t>(Syscall::Call(-1)));  // SVC 0
#endif
#elif defined(__mips__)
  // Opcode for MIPS sycall is in the lower 16-bits
  EXPECT_EQ(0x0cu, LoadBehind<uint32_t>(Syscall::Call(-1)) & 0x0000FFFF);
#elif defined(__aarch64__)
  EXPECT_EQ(0xD4000001u, LoadBehind<uint32_t>(Syscall::Call(-1)));  // SVC 0
#else
#warning Incomplete test case; need port for target platform
#endif
}

TEST(Syscall, TrivialSyscallNoArgs) {
  // Test that we can do basic system calls
  EXPECT_EQ(Syscall::Call(__NR_getpid), syscall(__NR_getpid));
}

TEST(Syscall, TrivialSyscallOneArg) {
  int new_fd;
  // Duplicate standard error and close it.
  ASSERT_GE(new_fd = Syscall::Call(__NR_dup, 2), 0);
  int close_return_value = IGNORE_EINTR(Syscall::Call(__NR_close, new_fd));
  ASSERT_EQ(close_return_value, 0);
}

TEST(Syscall, TrivialFailingSyscall) {
  errno = -42;
  int ret = Syscall::Call(__NR_dup, -1);
  ASSERT_EQ(-EBADF, ret);
  // Verify that Syscall::Call does not touch errno.
  ASSERT_EQ(-42, errno);
}

// SIGSYS trap handler that will be called on __NR_uname.
intptr_t CopySyscallArgsToAux(const struct arch_seccomp_data& args, void* aux) {
  // |aux| is our BPF_AUX pointer.
  std::vector<uint64_t>* const seen_syscall_args =
      static_cast<std::vector<uint64_t>*>(aux);
  BPF_ASSERT(std::size(args.args) == 6);
  seen_syscall_args->assign(args.args, args.args + std::size(args.args));
  return -ENOMEM;
}

class CopyAllArgsOnUnamePolicy : public bpf_dsl::Policy {
 public:
  explicit CopyAllArgsOnUnamePolicy(std::vector<uint64_t>* aux) : aux_(aux) {}

  CopyAllArgsOnUnamePolicy(const CopyAllArgsOnUnamePolicy&) = delete;
  CopyAllArgsOnUnamePolicy& operator=(const CopyAllArgsOnUnamePolicy&) = delete;

  ~CopyAllArgsOnUnamePolicy() override {}

  ResultExpr EvaluateSyscall(int sysno) const override {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    if (sysno == __NR_uname) {
      return Trap(CopySyscallArgsToAux, aux_);
    } else {
      return Allow();
    }
  }

 private:
  raw_ptr<std::vector<uint64_t>> aux_;
};

// We are testing Syscall::Call() by making use of a BPF filter that
// allows us
// to inspect the system call arguments that the kernel saw.
BPF_TEST(Syscall,
         SyntheticSixArgs,
         CopyAllArgsOnUnamePolicy,
         std::vector<uint64_t> /* (*BPF_AUX) */) {
  const int kExpectedValue = 42;
  // In this test we only pass integers to the kernel. We might want to make
  // additional tests to try other types. What we will see depends on
  // implementation details of kernel BPF filters and we will need to document
  // the expected behavior very clearly.
  int syscall_args[6];
  for (size_t i = 0; i < std::size(syscall_args); ++i) {
    syscall_args[i] = kExpectedValue + i;
  }

  // We could use pretty much any system call we don't need here. uname() is
  // nice because it doesn't have any dangerous side effects.
  BPF_ASSERT(Syscall::Call(__NR_uname,
                           syscall_args[0],
                           syscall_args[1],
                           syscall_args[2],
                           syscall_args[3],
                           syscall_args[4],
                           syscall_args[5]) == -ENOMEM);

  // We expect the trap handler to have copied the 6 arguments.
  BPF_ASSERT(BPF_AUX->size() == 6);

  // Don't loop here so that we can see which argument does cause the failure
  // easily from the failing line.
  // uint64_t is the type passed to our SIGSYS handler.
  BPF_ASSERT((*BPF_AUX)[0] == static_cast<uint64_t>(syscall_args[0]));
  BPF_ASSERT((*BPF_AUX)[1] == static_cast<uint64_t>(syscall_args[1]));
  BPF_ASSERT((*BPF_AUX)[2] == static_cast<uint64_t>(syscall_args[2]));
  BPF_ASSERT((*BPF_AUX)[3] == static_cast<uint64_t>(syscall_args[3]));
  BPF_ASSERT((*BPF_AUX)[4] == static_cast<uint64_t>(syscall_args[4]));
  BPF_ASSERT((*BPF_AUX)[5] == static_cast<uint64_t>(syscall_args[5]));
}

TEST(Syscall, ComplexSyscallSixArgs) {
  int fd;
  const size_t kPageSize = base::GetPageSize();

  ASSERT_LE(0,
            fd = Syscall::Call(__NR_openat, AT_FDCWD, "/dev/null", O_RDWR, 0L));

  // Use mmap() to allocate some read-only memory
  char* addr0;
  ASSERT_NE(
      (char*)NULL,
      addr0 = reinterpret_cast<char*>(Syscall::Call(kMMapNr,
                                                    (void*)NULL,
                                                    kPageSize,
                                                    PROT_READ,
                                                    MAP_PRIVATE | MAP_ANONYMOUS,
                                                    fd,
                                                    0L)));

  // Try to replace the existing mapping with a read-write mapping
  char* addr1;
  ASSERT_EQ(addr0,
            addr1 = reinterpret_cast<char*>(
                Syscall::Call(kMMapNr,
                              addr0,
                              kPageSize,
                              PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                              fd,
                              0L)));
  ++*addr1;  // This should not seg fault

  // Clean up
  EXPECT_EQ(0, Syscall::Call(__NR_munmap, addr1, kPageSize));
  EXPECT_EQ(0, IGNORE_EINTR(Syscall::Call(__NR_close, fd)));

  // Check that the offset argument (i.e. the sixth argument) is processed
  // correctly.
  ASSERT_GE(
      fd = Syscall::Call(__NR_openat, AT_FDCWD, "/proc/self/exe", O_RDONLY, 0L),
      0);
  char* addr2, *addr3;
  ASSERT_NE((char*)NULL,
            addr2 = reinterpret_cast<char*>(Syscall::Call(kMMapNr,
                                                          (void*)NULL,
                                                          2 * kPageSize,
                                                          PROT_READ,
                                                          MAP_PRIVATE,
                                                          fd,
                                                          0L
                                                          )));
  ASSERT_NE((char*)NULL,
            addr3 = reinterpret_cast<char*>(Syscall::Call(kMMapNr,
                                                          (void*)NULL,
                                                          kPageSize,
                                                          PROT_READ,
                                                          MAP_PRIVATE,
                                                          fd,
#if defined(__NR_mmap2)
                                                          1L
#else
                                                          kPageSize
#endif
                                                          )));
#if !defined(MEMORY_SANITIZER)
  // MSan considers the memory backing addr2 uninitialized.
  EXPECT_EQ(0, memcmp(addr2 + kPageSize, addr3, kPageSize));

  // Just to be absolutely on the safe side, also verify that the file
  // contents matches what we are getting from a read() operation.
  base::FixedArray<char> buf(2 * kPageSize);
  EXPECT_EQ(2 * kPageSize, static_cast<size_t>(Syscall::Call(
                               __NR_read, fd, buf.data(), 2 * kPageSize)));
  EXPECT_EQ(0, memcmp(addr2, buf.data(), 2 * kPageSize));
#endif

  // Clean up
  EXPECT_EQ(0, Syscall::Call(__NR_munmap, addr2, 2 * kPageSize));
  EXPECT_EQ(0, Syscall::Call(__NR_munmap, addr3, kPageSize));
  EXPECT_EQ(0, IGNORE_EINTR(Syscall::Call(__NR_close, fd)));
}

}  // namespace

}  // namespace sandbox
