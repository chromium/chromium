// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// Note: any code in this file MUST be async-signal safe.

#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"

#include <fcntl.h>
#include <linux/net.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "base/check.h"
#include "base/debug/crash_logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/safe_sprintf.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"
#include "sandbox/linux/system_headers/linux_stat.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

#if BUILDFLAG(IS_ANDROID)
#include <android/log.h>
#endif

#if defined(__mips__)
// __NR_Linux, is defined in <asm/unistd.h>.
#include <asm/unistd.h>
#endif

#define SECCOMP_MESSAGE_COMMON_CONTENT "seccomp-bpf failure"
#define SECCOMP_MESSAGE_CLONE_CONTENT "clone() failure"
#define SECCOMP_MESSAGE_PRCTL_CONTENT "prctl() failure"
#define SECCOMP_MESSAGE_IOCTL_CONTENT "ioctl() failure"
#define SECCOMP_MESSAGE_KILL_CONTENT "(tg)kill() failure"
#define SECCOMP_MESSAGE_FUTEX_CONTENT "futex() failure"
#define SECCOMP_MESSAGE_PTRACE_CONTENT "ptrace() failure"
#define SECCOMP_MESSAGE_SOCKET_CONTENT "socket() failure"
#define SECCOMP_MESSAGE_SOCKOPT_CONTENT "*sockopt() failure"

namespace {

#if BUILDFLAG(IS_ANDROID)
constexpr char kLogTag[] = "cr_seccomp";
#endif

base::debug::CrashKeyString* seccomp_crash_key = nullptr;

inline bool IsArchitectureX86_64() {
#if defined(__x86_64__)
  return true;
#else
  return false;
#endif
}

// Write |error_message| to stderr. Similar to RawLog(), but a bit more careful
// about async-signal safety. |size| is the size to write and should typically
// not include a terminating \0.
void WriteToStdErr(const char* error_message, size_t size) {
#if BUILDFLAG(IS_ANDROID)
  // Write to the Android log. When running as an APK, stderr is not typically
  // sent to the log.
  __android_log_write(ANDROID_LOG_ERROR, kLogTag, error_message);
#endif

  while (size > 0) {
    // TODO(jln): query the current policy to check if send() is available and
    // use it to perform a non-blocking write.
    const int ret = HANDLE_EINTR(
        sandbox::sys_write(STDERR_FILENO, error_message, size));
    // We can't handle any type of error here.
    if (ret <= 0 || static_cast<size_t>(ret) > size) {
      break;
    }
    size -= ret;
    error_message += ret;
  }
}

// Invalid syscall values are truncated to zero.
// On architectures where base value is zero (Intel and Arm),
// syscall number is the same as offset from base.
// This function returns values between 0 and 1023 on all architectures.
// On architectures where base value is different than zero (currently only
// Mips), we are truncating valid syscall values to offset from base.
uint32_t SyscallNumberToOffsetFromBase(uint32_t sysno) {
#if defined(__mips__)
  // On MIPS syscall numbers are in different range than on x86 and ARM.
  // Valid MIPS O32 ABI syscall __NR_syscall will be truncated to zero for
  // simplicity.
  sysno = sysno - __NR_Linux;
#endif

  if (sysno >= 1024)
    sysno = 0;

  return sysno;
}

// Records the syscall number and first four arguments in a crash key, to help
// debug the failure.
void PrintAndSetSeccompCrashKey(const struct arch_seccomp_data& args) {
  char crash_key[256];
  // SafeSPrintf only returns -1 for invalid buffer sizes (i.e. never happens).
  size_t crash_key_length = base::strings::SafeSPrintf(
      crash_key, "nr=0x%x arg1=0x%x arg2=0x%x arg3=0x%x arg4=0x%x", args.nr,
      args.args[0], args.args[1], args.args[2], args.args[3]);

  base::debug::SetCrashKeyString(seccomp_crash_key, crash_key);

  // To aide debugging, also write this information to the log.
#if defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS)
  static constexpr char kSeccompError[] =
      __FILE__ ":**CRASHING**:" SECCOMP_MESSAGE_COMMON_CONTENT
               " in syscall (base 4000) ";
#else
  static constexpr char kSeccompError[] =
      __FILE__ ":**CRASHING**:" SECCOMP_MESSAGE_COMMON_CONTENT " in syscall ";
#endif
  WriteToStdErr(kSeccompError, sizeof(kSeccompError) - 1);
  WriteToStdErr(crash_key, crash_key_length);
#if !BUILDFLAG(IS_ANDROID)
  WriteToStdErr("\n", 1);
#endif
}

}  // namespace

namespace sandbox {

intptr_t CrashSIGSYS_Handler(const struct arch_seccomp_data& args, void* aux) {
  uint32_t syscall = SyscallNumberToOffsetFromBase(args.nr);

  PrintAndSetSeccompCrashKey(args);

  // Encode 8-bits of the 1st two arguments too, so we can discern which socket
  // type, which fcntl, ... etc., without being likely to hit a mapped
  // address.
  // Do not encode more bits here without thinking about increasing the
  // likelihood of collision with mapped pages.
  syscall |= ((args.args[0] & 0xffUL) << 12);
  syscall |= ((args.args[1] & 0xffUL) << 20);
  // Purposefully dereference the syscall as an address so it'll show up very
  // clearly and easily in crash dumps.
  volatile char* addr = reinterpret_cast<volatile char*>(syscall);
  *addr = '\0';
  // In case we hit a mapped address, hit the null page with just the syscall,
  // for paranoia.
  syscall &= 0xfffUL;
  addr = reinterpret_cast<volatile char*>(syscall);
  *addr = '\0';
  for (;;)
    _exit(1);
}

// TODO(jln): refactor the reporting functions.

intptr_t SIGSYSCloneFailure(const struct arch_seccomp_data& args, void* aux) {
  static constexpr char kSeccompCloneError[] =
      __FILE__ ":**CRASHING**:" SECCOMP_MESSAGE_CLONE_CONTENT "\n";
  WriteToStdErr(kSeccompCloneError, sizeof(kSeccompCloneError) - 1);
  PrintAndSetSeccompCrashKey(args);
  // "flags" is the first argument in the kernel's clone().
  // Mark as volatile to be able to find the value on the stack in a minidump.
  volatile uint64_t clone_flags = args.args[0];
  volatile char* addr;
  if (IsArchitectureX86_64()) {
    addr = reinterpret_cast<volatile char*>(clone_flags & 0xFFFFFF);
    *addr = '\0';
  }
  // Hit the NULL page if this fails to fault.
  addr = reinterpret_cast<volatile char*>(clone_flags & 0xFFF);
  *addr = '\0';
  for (;;)
    _exit(1);
}

intptr_t SIGSYSPrctlFailure(const struct arch_seccomp_data& args,
                            void* /* aux */) {
  static constexpr char kSeccompPrctlError[] =
      __FILE__ ":**CRASHING**:" SECCOMP_MESSAGE_PRCTL_CONTENT "\n";
  WriteToStdErr(kSeccompPrctlError, sizeof(kSeccompPrctlError) - 1);
  PrintAndSetSeccompCrashKey(args);
  // Mark as volatile to be able to find the value on the stack in a minidump.
  volatile uint64_t option = args.args[0];
  volatile char* addr =
      reinterpret_cast<volatile char*>(option & 0xFFF);
  *addr = '\0';
  for (;;)
    _exit(1);
}

intptr_t SIGSYSIoctlFailure(const struct arch_seccomp_data& args,
                            void* /* aux */) {
  static constexpr char kSeccompIoctlError[] =
      __FILE__ ":**CRASHING**:" SECCOMP_MESSAGE_IOCTL_CONTENT "\n";
  WriteToStdErr(kSeccompIoctlError, sizeof(kSeccompIoctlError) - 1);
  PrintAndSetSeccompCrashKey(args);
  // Make "request" volatile so that we can see it on the stack in a minidump.
  volatile uint64_t request = args.args[1];
  volatile char* addr = reinterpret_cast<volatile char*>(request & 0xFFFF);
  *addr = '\0';
  // Hit the NULL page if this fails.
  addr = reinterpret_cast<volatile char*>(request & 0xFFF);
  *addr = '\0';
  for (;;)
    _exit(1);
}

intptr_t SIGSYSKillFailure(const struct arch_seccomp_data& args,
                           void* /* aux */) {
  static constexpr char kSeccompKillError[] =
      __FILE__ ":**CRASHING**:" SECCOMP_MESSAGE_KILL_CONTENT "\n";
  WriteToStdErr(kSeccompKillError, sizeof(kSeccompKillError) - 1);
  PrintAndSetSeccompCrashKey(args);
  // Make "pid" volatile so that we can see it on the stack in a minidump.
  volatile uint64_t my_pid = sys_getpid();
  volatile char* addr = reinterpret_cast<volatile char*>(my_pid & 0xFFF);
  *addr = '\0';
  for (;;)
    _exit(1);
}

intptr_t SIGSYSFutexFailure(const struct arch_seccomp_data& args,
                            void* /* aux */) {
  static constexpr char kSeccompFutexError[] =
      __FILE__ ":**CRASHING**:" SECCOMP_MESSAGE_FUTEX_CONTENT "\n";
  WriteToStdErr(kSeccompFutexError, sizeof(kSeccompFutexError) - 1);
  PrintAndSetSeccompCrashKey(args);
  volatile int futex_op = args.args[1];
  volatile char* addr = reinterpret_cast<volatile char*>(futex_op & 0xFFF);
  *addr = '\0';
  for (;;)
    _exit(1);
}

intptr_t SIGSYSPtraceFailure(const struct arch_seccomp_data& args,
                             void* /* aux */) {
  static constexpr char kSeccompPtraceError[] =
      __FILE__ ":**CRASHING**:" SECCOMP_MESSAGE_PTRACE_CONTENT "\n";
  WriteToStdErr(kSeccompPtraceError, sizeof(kSeccompPtraceError) - 1);
  PrintAndSetSeccompCrashKey(args);
  volatile int ptrace_op = args.args[0];
  volatile char* addr = reinterpret_cast<volatile char*>(ptrace_op & 0xFFF);
  *addr = '\0';
  for (;;)
    _exit(1);
}

intptr_t SIGSYSSocketFailure(const struct arch_seccomp_data& args, void* aux) {
  static constexpr char kSeccompSocketError[] =
      __FILE__ ":**CRASHING**:" SECCOMP_MESSAGE_SOCKET_CONTENT "\n";
  WriteToStdErr(kSeccompSocketError, sizeof(kSeccompSocketError) - 1);

  char arguments[128];
  ssize_t arguments_size = base::strings::SafeSPrintf(
      arguments, "domain=0x%x type=0x%x protocol=0x%x\n", args.args[0],
      args.args[1], args.args[2]);
  WriteToStdErr(arguments, arguments_size);

  PrintAndSetSeccompCrashKey(args);
  // Make args volatile so that we can see it on the stack in a minidump.
  volatile int domain = static_cast<int>(args.args[0]);
  volatile int type = static_cast<int>(args.args[1]);
  volatile int protocol = static_cast<int>(args.args[2]);

  // Encode argument bits into an address and dereference it, hoping to avoid
  // hitting mapped pages.
  uintptr_t addr = (domain & 0x3ful);   // 6 bits for domain
  addr |= ((type & 0xful) << 6);        // 4 bits for type
  addr |= ((protocol & 0x1ful) << 10);  // 5 bits for protocol
  *reinterpret_cast<volatile char*>(addr) = '\0';

  // Hit the NULL page with just the domain and type if this fails.
  *reinterpret_cast<volatile char*>((domain & 0x3ful) | ((type & 0xful) << 6)) =
      '\0';
  for (;;) {
    _exit(1);
  }
}

intptr_t SIGSYSSockoptFailure(const struct arch_seccomp_data& args, void* aux) {
  static constexpr char kSeccompSockoptError[] =
      __FILE__ ":**CRASHING**:" SECCOMP_MESSAGE_SOCKOPT_CONTENT "\n";
  WriteToStdErr(kSeccompSockoptError, sizeof(kSeccompSockoptError) - 1);

  char arguments[64];
  ssize_t arguments_size = base::strings::SafeSPrintf(
      arguments, "level=0x%x optname=0x%x\n", args.args[1], args.args[2]);
  WriteToStdErr(arguments, arguments_size);

  PrintAndSetSeccompCrashKey(args);
  // Make args volatile so that we can see it on the stack in a minidump.
  volatile int level = static_cast<int>(args.args[1]);
  volatile int optname = static_cast<int>(args.args[2]);

  // Encode argument bits into an address and dereference it, hoping to avoid
  // hitting mapped pages.
  // 9 bits for level. The levels are listed in /etc/protocols and most are
  // below 256, except one.
  uintptr_t addr = (level & 0x1fful);
  addr |= ((optname & 0x7ful) << 9);  // 7 bits for optname.
  *reinterpret_cast<volatile char*>(addr) = '\0';

  // Hit the NULL page with just the level.
  *reinterpret_cast<volatile char*>((level & 0x1fful)) = '\0';
  for (;;) {
    _exit(1);
  }
}

intptr_t SIGSYSSchedHandler(const struct arch_seccomp_data& args,
                            void* aux) {
  switch (args.nr) {
    case __NR_sched_getaffinity:
    case __NR_sched_getattr:
    case __NR_sched_getparam:
    case __NR_sched_getscheduler:
    case __NR_sched_rr_get_interval:
    case __NR_sched_setaffinity:
    case __NR_sched_setattr:
    case __NR_sched_setparam:
    case __NR_sched_setscheduler:
      const pid_t tid = sys_gettid();
      // The first argument is the pid.  If is our thread id, then replace it
      // with 0, which is equivalent and allowed by the policy.
      if (args.args[0] == static_cast<uint64_t>(tid)) {
        return Syscall::Call(args.nr,
                             0,
                             static_cast<intptr_t>(args.args[1]),
                             static_cast<intptr_t>(args.args[2]),
                             static_cast<intptr_t>(args.args[3]),
                             static_cast<intptr_t>(args.args[4]),
                             static_cast<intptr_t>(args.args[5]));
      }
      break;
  }

  CrashSIGSYS_Handler(args, aux);

  // Should never be reached.
  RAW_CHECK(false);
  return -ENOSYS;
}

intptr_t SIGSYSFstatatHandler(const struct arch_seccomp_data& args,
                              void* fs_denied_errno) {
  if (args.nr == __NR_fstatat_default) {
    if (*reinterpret_cast<const char*>(args.args[1]) == '\0' &&
        args.args[3] == static_cast<uint64_t>(AT_EMPTY_PATH)) {
      return syscall(__NR_fstat_default, static_cast<int>(args.args[0]),
                     reinterpret_cast<default_stat_struct*>(args.args[2]));
    }
    return -reinterpret_cast<intptr_t>(fs_denied_errno);
  }

  CrashSIGSYS_Handler(args, fs_denied_errno);

  // Should never be reached.
  RAW_CHECK(false);
  return -ENOSYS;
}

bpf_dsl::ResultExpr CrashSIGSYS() {
  return bpf_dsl::Trap(CrashSIGSYS_Handler, nullptr);
}

bpf_dsl::ResultExpr CrashSIGSYSClone() {
  return bpf_dsl::Trap(SIGSYSCloneFailure, nullptr);
}

bpf_dsl::ResultExpr CrashSIGSYSPrctl() {
  return bpf_dsl::Trap(SIGSYSPrctlFailure, nullptr);
}

bpf_dsl::ResultExpr CrashSIGSYSIoctl() {
  return bpf_dsl::Trap(SIGSYSIoctlFailure, nullptr);
}

bpf_dsl::ResultExpr CrashSIGSYSKill() {
  return bpf_dsl::Trap(SIGSYSKillFailure, nullptr);
}

bpf_dsl::ResultExpr CrashSIGSYSFutex() {
  return bpf_dsl::Trap(SIGSYSFutexFailure, nullptr);
}

bpf_dsl::ResultExpr CrashSIGSYSPtrace() {
  return bpf_dsl::Trap(SIGSYSPtraceFailure, nullptr);
}

bpf_dsl::ResultExpr CrashSIGSYSSocket() {
  return bpf_dsl::Trap(SIGSYSSocketFailure, nullptr);
}

bpf_dsl::ResultExpr CrashSIGSYSSockopt() {
  return bpf_dsl::Trap(SIGSYSSockoptFailure, nullptr);
}

bpf_dsl::ResultExpr RewriteSchedSIGSYS() {
  return bpf_dsl::Trap(SIGSYSSchedHandler, nullptr);
}

bpf_dsl::ResultExpr RewriteFstatatSIGSYS(int fs_denied_errno) {
  return bpf_dsl::Trap(SIGSYSFstatatHandler,
                       reinterpret_cast<void*>(fs_denied_errno));
}

#if defined(__NR_socketcall)
bool CanRewriteSocketcall() {
  static bool can_rewrite_socketcall = []() {
    // Call socket(2) with invalid flags and see if it returns ENOSYS.
    base::ScopedFD socket_fd(
        syscall(__NR_socket, 0xffffff, 0xffffff, 0xffffff));
    if (!socket_fd.is_valid() && errno == ENOSYS) {
      return false;
    }
    return true;
  }();
  return can_rewrite_socketcall;
}

intptr_t SIGSYSSocketcallHandler(const struct arch_seccomp_data& args,
                                 void* aux) {
  const long kLastSocketcall = SYS_SENDMMSG;
  // This array is mostly copy and pasted from the Linux kernel (net/socket.c)
  static const struct {
    long sysno;
    size_t num_args;
    size_t num_zeroes = 0;
  } socketcall_args[kLastSocketcall + 1] = {
      {.sysno = -1, .num_args = 0},
      {.sysno = __NR_socket, .num_args = 3},
      {.sysno = __NR_bind, .num_args = 3},
      {.sysno = __NR_connect, .num_args = 3},
      {.sysno = __NR_listen, .num_args = 2},
      // SYS_ACCEPT does not always have a corresponding accept() syscall, but
      // always has an accept4() with flags == 0.
      {.sysno = __NR_accept4, .num_args = 3, .num_zeroes = 1},
      {.sysno = __NR_getsockname, .num_args = 3},
      {.sysno = __NR_getpeername, .num_args = 3},
      {.sysno = __NR_socketpair, .num_args = 4},
      // The SYS_SEND and SYS_RECV calls do not have corresponding __NR_send and
      // __NR_recv syscalls, but are equivalent to sendto() and recvfrom() with
      // the final arguments as 0.
      {.sysno = __NR_sendto, .num_args = 4, .num_zeroes = 2},
      {.sysno = __NR_recvfrom, .num_args = 4, .num_zeroes = 2},
      {.sysno = __NR_sendto, .num_args = 6},
      {.sysno = __NR_recvfrom, .num_args = 6},
      {.sysno = __NR_shutdown, .num_args = 2},
      {.sysno = __NR_setsockopt, .num_args = 5},
      {.sysno = __NR_getsockopt, .num_args = 5},
      {.sysno = __NR_sendmsg, .num_args = 3},
      {.sysno = __NR_recvmsg, .num_args = 3},
      {.sysno = __NR_accept4, .num_args = 4},
      {.sysno = __NR_recvmmsg, .num_args = 5},
      {.sysno = __NR_sendmmsg, .num_args = 4}};
  uint64_t call = args.args[0];
  if (args.nr == __NR_socketcall && 0 < call && call <= kLastSocketcall) {
    const size_t real_args_arr_len =
        socketcall_args[call].num_args + socketcall_args[call].num_zeroes;
// The length of this array is bounded by the entries in the array above,
// but the compiler isn't smart enough to figure that out.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-extension"
    unsigned long real_args_arr[real_args_arr_len];
#pragma clang diagnostic pop
    memcpy(real_args_arr, reinterpret_cast<unsigned long*>(args.args[1]),
           real_args_arr_len * sizeof(unsigned long));
    memset(real_args_arr + socketcall_args[call].num_args, 0,
           socketcall_args[call].num_zeroes * sizeof(unsigned long));
    switch (real_args_arr_len) {
      case 2:
        return syscall(socketcall_args[call].sysno, real_args_arr[0],
                       real_args_arr[1]);
      case 3:
        return syscall(socketcall_args[call].sysno, real_args_arr[0],
                       real_args_arr[1], real_args_arr[2]);
      case 4:
        return syscall(socketcall_args[call].sysno, real_args_arr[0],
                       real_args_arr[1], real_args_arr[2], real_args_arr[3]);
      case 5:
        return syscall(socketcall_args[call].sysno, real_args_arr[0],
                       real_args_arr[1], real_args_arr[2], real_args_arr[3],
                       real_args_arr[4]);
      case 6:
        return syscall(socketcall_args[call].sysno, real_args_arr[0],
                       real_args_arr[1], real_args_arr[2], real_args_arr[3],
                       real_args_arr[4], real_args_arr[5]);
      default:
        break;
    }
  }

  CrashSIGSYS_Handler(args, aux);

  // Should never be reached.
  RAW_CHECK(false);
  return -ENOSYS;
}

bpf_dsl::ResultExpr RewriteSocketcallSIGSYS() {
  return bpf_dsl::Trap(SIGSYSSocketcallHandler, nullptr);
}
#endif  // defined(__NR_socketcall)

void AllocateCrashKeys() {
  if (seccomp_crash_key)
    return;

  seccomp_crash_key = base::debug::AllocateCrashKeyString(
      "seccomp-sigsys", base::debug::CrashKeySize::Size256);
}

const char* GetErrorMessageContentForTests() {
  return SECCOMP_MESSAGE_COMMON_CONTENT;
}

const char* GetCloneErrorMessageContentForTests() {
  return SECCOMP_MESSAGE_CLONE_CONTENT;
}

const char* GetPrctlErrorMessageContentForTests() {
  return SECCOMP_MESSAGE_PRCTL_CONTENT;
}

const char* GetIoctlErrorMessageContentForTests() {
  return SECCOMP_MESSAGE_IOCTL_CONTENT;
}

const char* GetKillErrorMessageContentForTests() {
  return SECCOMP_MESSAGE_KILL_CONTENT;
}

const char* GetFutexErrorMessageContentForTests() {
  return SECCOMP_MESSAGE_FUTEX_CONTENT;
}

const char* GetPtraceErrorMessageContentForTests() {
  return SECCOMP_MESSAGE_PTRACE_CONTENT;
}

const char* GetSocketErrorMessageContentForTests() {
  return SECCOMP_MESSAGE_SOCKET_CONTENT;
}

const char* GetSockoptErrorMessageContentForTests() {
  return SECCOMP_MESSAGE_SOCKOPT_CONTENT;
}

}  // namespace sandbox.
