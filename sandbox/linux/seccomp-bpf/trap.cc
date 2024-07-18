// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/seccomp-bpf/trap.h"

#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>

#include <algorithm>
#include <atomic>
#include <limits>
#include <tuple>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/seccomp_macros.h"
#include "sandbox/linux/seccomp-bpf/die.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"
#include "sandbox/linux/system_headers/linux_signal.h"

namespace {

struct arch_sigsys {
  // RAW_PTR_EXCLUSION: Points to a code address given to us by the kernel.
  RAW_PTR_EXCLUSION void* ip;
  int nr;
  unsigned int arch;
};

const int kCapacityIncrement = 20;

// Unsafe traps can only be turned on, if the user explicitly allowed them
// by setting the CHROME_SANDBOX_DEBUGGING environment variable.
const char kSandboxDebuggingEnv[] = "CHROME_SANDBOX_DEBUGGING";

// We need to tell whether we are performing a "normal" callback, or
// whether we were called recursively from within a UnsafeTrap() callback.
// This is a little tricky to do, because we need to somehow get access to
// per-thread data from within a signal context. Normal TLS storage is not
// safely accessible at this time. We could roll our own, but that involves
// a lot of complexity. Instead, we co-opt one bit in the signal mask.
// If BUS is blocked, we assume that we have been called recursively.
// There is a possibility for collision with other code that needs to do
// this, but in practice the risks are low.
// If SIGBUS turns out to be a problem, we could instead co-opt one of the
// realtime signals. There are plenty of them. Unfortunately, there is no
// way to mark a signal as allocated. So, the potential for collision is
// possibly even worse.
bool GetIsInSigHandler(const ucontext_t* ctx) {
  // Note: on Android, sigismember does not take a pointer to const.
  return sigismember(const_cast<sigset_t*>(&ctx->uc_sigmask), LINUX_SIGBUS);
}

void SetIsInSigHandler() {
  sigset_t mask;
  if (sigemptyset(&mask) || sigaddset(&mask, LINUX_SIGBUS) ||
      sandbox::sys_sigprocmask(LINUX_SIG_BLOCK, &mask, nullptr)) {
    SANDBOX_DIE("Failed to block SIGBUS");
  }
}

bool IsDefaultSignalAction(const struct sigaction& sa) {
  if (sa.sa_flags & SA_SIGINFO || sa.sa_handler != SIG_DFL) {
    return false;
  }
  return true;
}

}  // namespace

namespace sandbox {

Trap::Trap() {
  // Set new SIGSYS handler
  struct sigaction sa = {};
  // In some toolchain, sa_sigaction is not declared in struct sigaction.
  // So, here cast the pointer to the sa_handler's type. This works because
  // |sa_handler| and |sa_sigaction| shares the same memory.
  sa.sa_handler = reinterpret_cast<void (*)(int)>(SigSysAction);
  sa.sa_flags = LINUX_SA_SIGINFO | LINUX_SA_NODEFER;
  struct sigaction old_sa = {};
  if (sys_sigaction(LINUX_SIGSYS, &sa, &old_sa) < 0) {
    SANDBOX_DIE("Failed to configure SIGSYS handler");
  }

  if (!IsDefaultSignalAction(old_sa)) {
    static const char kExistingSIGSYSMsg[] =
        "Existing signal handler when trying to install SIGSYS. SIGSYS needs "
        "to be reserved for seccomp-bpf.";
    DLOG(FATAL) << kExistingSIGSYSMsg;
    LOG(ERROR) << kExistingSIGSYSMsg;
  }

  // Unmask SIGSYS
  sigset_t mask;
  if (sigemptyset(&mask) || sigaddset(&mask, LINUX_SIGSYS) ||
      sys_sigprocmask(LINUX_SIG_UNBLOCK, &mask, nullptr)) {
    SANDBOX_DIE("Failed to configure SIGSYS handler");
  }
}

bpf_dsl::TrapRegistry* Trap::Registry() {
  // Note: This class is not thread safe. It is the caller's responsibility
  // to avoid race conditions. Normally, this is a non-issue as the sandbox
  // can only be initialized if there are no other threads present.
  // Also, this is not a normal singleton. Once created, the global trap
  // object must never be destroyed again.
  if (!global_trap_) {
    global_trap_ = new Trap();
    if (!global_trap_) {
      SANDBOX_DIE("Failed to allocate global trap handler");
    }
  }
  return global_trap_;
}

void Trap::SigSysAction(int nr, LinuxSigInfo* info, void* void_context) {
  if (info) {
    MSAN_UNPOISON(info, sizeof(*info));
  }

  // Obtain the signal context. This, most notably, gives us access to
  // all CPU registers at the time of the signal.
  ucontext_t* ctx = reinterpret_cast<ucontext_t*>(void_context);
  if (ctx) {
    MSAN_UNPOISON(ctx, sizeof(*ctx));
  }

  if (!global_trap_) {
    RAW_SANDBOX_DIE(
        "This can't happen. Found no global singleton instance "
        "for Trap() handling.");
  }
  global_trap_->SigSys(nr, info, ctx);
}

void Trap::SigSys(int nr, LinuxSigInfo* info, ucontext_t* ctx) {
  // Signal handlers should always preserve "errno". Otherwise, we could
  // trigger really subtle bugs.
  const int old_errno = errno;

  // Various sanity checks to make sure we actually received a signal
  // triggered by a BPF filter. If something else triggered SIGSYS
  // (e.g. kill()), there is really nothing we can do with this signal.
  if (nr != LINUX_SIGSYS || info->si_code != SYS_SECCOMP || !ctx ||
      info->si_errno <= 0 ||
      static_cast<size_t>(info->si_errno) > trap_array_size_) {
    // ATI drivers seem to send SIGSYS, so this cannot be FATAL.
    // See crbug.com/178166.
    // TODO(jln): add a DCHECK or move back to FATAL.
    RAW_LOG(ERROR, "Unexpected SIGSYS received.");
    errno = old_errno;
    return;
  }


  // Obtain the siginfo information that is specific to SIGSYS.
  struct arch_sigsys sigsys;
#if defined(si_call_addr)
  sigsys.ip = info->si_call_addr;
  sigsys.nr = info->si_syscall;
  sigsys.arch = info->si_arch;
#else
  // If the version of glibc doesn't include this information in
  // siginfo_t (older than 2.17), we need to explicitly copy it
  // into an arch_sigsys structure.
  memcpy(&sigsys, &info->_sifields, sizeof(sigsys));
#endif

#if defined(__mips__)
  // When indirect syscall (syscall(__NR_foo, ...)) is made on Mips, the
  // number in register SECCOMP_SYSCALL(ctx) is always __NR_syscall and the
  // real number of a syscall (__NR_foo) is in SECCOMP_PARM1(ctx)
  bool sigsys_nr_is_bad = sigsys.nr != static_cast<int>(SECCOMP_SYSCALL(ctx)) &&
                          sigsys.nr != static_cast<int>(SECCOMP_PARM1(ctx));
#else
  bool sigsys_nr_is_bad = sigsys.nr != static_cast<int>(SECCOMP_SYSCALL(ctx));
#endif

  // Some more sanity checks.
  if (sigsys.ip != reinterpret_cast<void*>(SECCOMP_IP(ctx)) ||
      sigsys_nr_is_bad || sigsys.arch != SECCOMP_ARCH) {
    // TODO(markus):
    // SANDBOX_DIE() can call LOG(FATAL). This is not normally async-signal
    // safe and can lead to bugs. We should eventually implement a different
    // logging and reporting mechanism that is safe to be called from
    // the sigSys() handler.
    RAW_SANDBOX_DIE("Sanity checks are failing after receiving SIGSYS.");
  }

  intptr_t rc;
  if (has_unsafe_traps_ && GetIsInSigHandler(ctx)) {
    errno = old_errno;
    if (sigsys.nr == __NR_clone) {
      RAW_SANDBOX_DIE("Cannot call clone() from an UnsafeTrap() handler.");
    }
#if defined(__mips__)
    // Mips supports up to eight arguments for syscall.
    // However, seccomp bpf can filter only up to six arguments, so using eight
    // arguments has sense only when using UnsafeTrap() handler.
    rc = Syscall::Call(SECCOMP_SYSCALL(ctx),
                       SECCOMP_PARM1(ctx),
                       SECCOMP_PARM2(ctx),
                       SECCOMP_PARM3(ctx),
                       SECCOMP_PARM4(ctx),
                       SECCOMP_PARM5(ctx),
                       SECCOMP_PARM6(ctx),
                       SECCOMP_PARM7(ctx),
                       SECCOMP_PARM8(ctx));
#else
    rc = Syscall::Call(SECCOMP_SYSCALL(ctx),
                       SECCOMP_PARM1(ctx),
                       SECCOMP_PARM2(ctx),
                       SECCOMP_PARM3(ctx),
                       SECCOMP_PARM4(ctx),
                       SECCOMP_PARM5(ctx),
                       SECCOMP_PARM6(ctx));
#endif  // defined(__mips__)
  } else {
    const auto& trap = trap_array_[info->si_errno - 1];
    if (!trap.safe) {
      SetIsInSigHandler();
    }

    // Copy the seccomp-specific data into a arch_seccomp_data structure. This
    // is what we are showing to TrapFnc callbacks that the system call
    // evaluator registered with the sandbox.
    struct arch_seccomp_data data = {
        static_cast<int>(SECCOMP_SYSCALL(ctx)),
        SECCOMP_ARCH,
        reinterpret_cast<uint64_t>(sigsys.ip),
        {static_cast<uint64_t>(SECCOMP_PARM1(ctx)),
         static_cast<uint64_t>(SECCOMP_PARM2(ctx)),
         static_cast<uint64_t>(SECCOMP_PARM3(ctx)),
         static_cast<uint64_t>(SECCOMP_PARM4(ctx)),
         static_cast<uint64_t>(SECCOMP_PARM5(ctx)),
         static_cast<uint64_t>(SECCOMP_PARM6(ctx))}};

    // Now call the TrapFnc callback associated with this particular instance
    // of SECCOMP_RET_TRAP.
    rc = trap.fnc(data, reinterpret_cast<void*>(trap.aux));
  }

  // Update the CPU register that stores the return code of the system call
  // that we just handled, and restore "errno" to the value that it had
  // before entering the signal handler.
  Syscall::PutValueInUcontext(rc, ctx);
  errno = old_errno;

  return;
}

uint16_t Trap::Add(const Handler& handler) {
  if (!handler.safe && !SandboxDebuggingAllowedByUser()) {
    // Unless the user set the CHROME_SANDBOX_DEBUGGING environment variable,
    // we never return an ErrorCode that is marked as "unsafe". This also
    // means, the BPF compiler will never emit code that allow unsafe system
    // calls to by-pass the filter (because they use the magic return address
    // from Syscall::Call(-1)).

    // This SANDBOX_DIE() can optionally be removed. It won't break security,
    // but it might make error messages from the BPF compiler a little harder
    // to understand. Removing the SANDBOX_DIE() allows callers to easily check
    // whether unsafe traps are supported (by checking whether the returned
    // ErrorCode is ET_INVALID).
    SANDBOX_DIE(
        "Cannot use unsafe traps unless CHROME_SANDBOX_DEBUGGING "
        "is enabled");
  }

  // We return unique identifiers together with SECCOMP_RET_TRAP. This allows
  // us to associate trap with the appropriate handler. The kernel allows us
  // identifiers in the range from 0 to SECCOMP_RET_DATA (0xFFFF). We want to
  // avoid 0, as it could be confused for a trap without any specific id.
  // The nice thing about sequentially numbered identifiers is that we can also
  // trivially look them up from our signal handler without making any system
  // calls that might be async-signal-unsafe.
  // In order to do so, we store all of our traps in a C-style trap_array_.

  auto iter = trap_ids_.find(handler);
  if (iter != trap_ids_.end()) {
    // We have seen this pair before. Return the same id that we assigned
    // earlier.
    return iter->second;
  }

  // This is a new pair. Remember it and assign a new id.
  if (trap_array_size_ >= SECCOMP_RET_DATA /* 0xFFFF */ ||
      trap_array_size_ >= std::numeric_limits<uint16_t>::max()) {
    // In practice, this is pretty much impossible to trigger, as there
    // are other kernel limitations that restrict overall BPF program sizes.
    SANDBOX_DIE("Too many SECCOMP_RET_TRAP callback instances");
  }

  // Our callers ensure that there are no other threads accessing trap_array_
  // concurrently (typically this is done by ensuring that we are single-
  // threaded while the sandbox is being set up). But we nonetheless are
  // modifying a live data structure that could be accessed any time a
  // system call is made; as system calls could be triggering SIGSYS.
  // So, we have to be extra careful that we update trap_array_ atomically.
  // In particular, this means we shouldn't be using realloc() to resize it.
  // Instead, we allocate a new array, copy the values, and then switch the
  // pointer. We only really care about the pointer being updated atomically
  // and the data that is pointed to being valid, as these are the only
  // values accessed from the signal handler. It is OK if trap_array_size_
  // is inconsistent with the pointer, as it is monotonously increasing.
  // Also, we only care about compiler barriers, as the signal handler is
  // triggered synchronously from a system call. We don't have to protect
  // against issues with the memory model or with completely asynchronous
  // events.
  if (trap_array_size_ >= trap_array_capacity_) {
    trap_array_capacity_ += kCapacityIncrement;
    auto* old_trap_array = trap_array_;
    auto* new_trap_array = new TrapRegistry::Handler[trap_array_capacity_];
    std::copy_n(old_trap_array, trap_array_size_, new_trap_array);

    trap_array_ = new_trap_array;
    // Prevent the compiler from moving delete[] before the store of the
    // |new_trap_array|, otherwise a concurrent SIGSYS may see a |trap_array_|
    // that still points to |old_trap_array| after it has been deleted.
    std::atomic_signal_fence(std::memory_order_release);
    delete[] old_trap_array;
  }

  uint16_t id = trap_array_size_ + 1;
  trap_ids_[handler] = id;
  trap_array_[trap_array_size_] = handler;
  trap_array_size_++;
  return id;
}

bool Trap::SandboxDebuggingAllowedByUser() {
  const char* debug_flag = getenv(kSandboxDebuggingEnv);
  return debug_flag && *debug_flag;
}

bool Trap::EnableUnsafeTraps() {
  if (!has_unsafe_traps_) {
    // Unsafe traps are a one-way fuse. Once enabled, they can never be turned
    // off again.
    // We only allow enabling unsafe traps, if the user explicitly set an
    // appropriate environment variable. This prevents bugs that accidentally
    // disable all sandboxing for all users.
    if (SandboxDebuggingAllowedByUser()) {
      // We only ever print this message once, when we enable unsafe traps the
      // first time.
      SANDBOX_INFO("WARNING! Disabling sandbox for debugging purposes");
      has_unsafe_traps_ = true;
    } else {
      SANDBOX_INFO(
          "Cannot disable sandbox and use unsafe traps unless "
          "CHROME_SANDBOX_DEBUGGING is turned on first");
    }
  }
  // Returns the, possibly updated, value of has_unsafe_traps_.
  return has_unsafe_traps_;
}

Trap* Trap::global_trap_;

}  // namespace sandbox
