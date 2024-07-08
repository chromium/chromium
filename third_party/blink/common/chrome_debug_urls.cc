// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/chrome_debug_urls.h"

#include "base/debug/asan_invalid_access.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "third_party/blink/common/crash_helpers.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/debug/invalid_access_win.h"
#include "base/process/kill.h"
#elif BUILDFLAG(IS_POSIX)
#include <signal.h>
#elif BUILDFLAG(IS_FUCHSIA)
#include <zircon/syscalls.h>
#endif

#if BUILDFLAG(ENABLE_RUST_CRASH)
#include "third_party/blink/common/rust_crash/src/lib.rs.h"
#endif

namespace blink {

bool IsRendererDebugURL(const GURL& url) {
  if (!url.is_valid())
    return false;

  if (url.SchemeIs(url::kJavaScriptScheme))
    return true;

  if (!url.SchemeIs("chrome"))
    return false;

  if (url == kChromeUICheckCrashURL || url == kChromeUIBadCastCrashURL ||
      url == kChromeUICrashURL || url == kChromeUIDumpURL ||
      url == kChromeUIKillURL || url == kChromeUIHangURL ||
      url == kChromeUIShorthangURL || url == kChromeUIMemoryExhaustURL) {
    return true;
  }

#if BUILDFLAG(ENABLE_RUST_CRASH)
  if (url == kChromeUICrashRustURL) {
    return true;
  }
#endif

#if defined(ADDRESS_SANITIZER)
  if (url == kChromeUICrashHeapOverflowURL ||
      url == kChromeUICrashHeapUnderflowURL ||
      url == kChromeUICrashUseAfterFreeURL) {
    return true;
  }

#if BUILDFLAG(ENABLE_RUST_CRASH)
  if (url == kChromeUICrashRustOverflowURL) {
    return true;
  }
#endif  // BUILDFLAG(ENABLE_RUST_CRASH)
#endif  // defined(ADDRESS_SANITIZER)

#if BUILDFLAG(IS_WIN)
  if (url == kChromeUICfgViolationCrashURL)
    return true;
  if (url == kChromeUIHeapCorruptionCrashURL)
    return true;
#endif

#if DCHECK_IS_ON()
  if (url == kChromeUICrashDcheckURL)
    return true;
#endif

#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
  if (url == kChromeUICrashCorruptHeapBlockURL ||
      url == kChromeUICrashCorruptHeapURL) {
    return true;
  }
#endif

  return false;
}

namespace {

// The following methods are outside of the anonymous namespace to ensure that
// the corresponding symbols get emitted even on symbol_level 1.
NOINLINE void ExhaustMemory() {
  volatile void* ptr = nullptr;
  do {
    ptr = malloc(0x10000000);
    base::debug::Alias(&ptr);
  } while (ptr);
}

#if defined(ADDRESS_SANITIZER)
NOINLINE void MaybeTriggerAsanError(const GURL& url) {
  // NOTE(rogerm): We intentionally perform an invalid heap access here in
  //     order to trigger an Address Sanitizer (ASAN) error report.
  if (url == kChromeUICrashHeapOverflowURL) {
    LOG(ERROR) << "Intentionally causing ASAN heap overflow"
               << " because user navigated to " << url.spec();
    base::debug::AsanHeapOverflow();
  } else if (url == kChromeUICrashHeapUnderflowURL) {
    LOG(ERROR) << "Intentionally causing ASAN heap underflow"
               << " because user navigated to " << url.spec();
    base::debug::AsanHeapUnderflow();
  } else if (url == kChromeUICrashUseAfterFreeURL) {
    LOG(ERROR) << "Intentionally causing ASAN heap use-after-free"
               << " because user navigated to " << url.spec();
    base::debug::AsanHeapUseAfterFree();
#if BUILDFLAG(IS_WIN)
  } else if (url == kChromeUICrashCorruptHeapBlockURL) {
    LOG(ERROR) << "Intentionally causing ASAN corrupt heap block"
               << " because user navigated to " << url.spec();
    base::debug::AsanCorruptHeapBlock();
  } else if (url == kChromeUICrashCorruptHeapURL) {
    LOG(ERROR) << "Intentionally causing ASAN corrupt heap"
               << " because user navigated to " << url.spec();
    base::debug::AsanCorruptHeap();
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(ENABLE_RUST_CRASH)
  } else if (url == kChromeUICrashRustOverflowURL) {
    // Ensure that ASAN works even in Rust code.
    LOG(ERROR) << "Intentionally causing ASAN heap overflow in Rust"
               << " because user navigated to " << url.spec();
    crash_in_rust_with_overflow();
#endif
  }
}
#endif  // ADDRESS_SANITIZER

}  // namespace

void HandleChromeDebugURL(const GURL& url) {
  DCHECK(IsRendererDebugURL(url) && !url.SchemeIs("javascript"));
  if (url == kChromeUIBadCastCrashURL) {
    LOG(ERROR) << "Intentionally crashing (with bad cast)"
               << " because user navigated to " << url.spec();
    internal::BadCastCrashIntentionally();
  } else if (url == kChromeUICrashURL) {
    LOG(ERROR) << "Intentionally crashing (with null pointer dereference)"
               << " because user navigated to " << url.spec();
    internal::CrashIntentionally();
#if BUILDFLAG(ENABLE_RUST_CRASH)
  } else if (url == kChromeUICrashRustURL) {
    // Cause a typical crash in Rust code, so we can test that call stack
    // collection and symbol mangling work across the language boundary.
    crash_in_rust();
#endif
  } else if (url == kChromeUIDumpURL) {
    // This URL will only correctly create a crash dump file if content is
    // hosted in a process that has correctly called
    // base::debug::SetDumpWithoutCrashingFunction.  Refer to the documentation
    // of base::debug::DumpWithoutCrashing for more details.
    base::debug::DumpWithoutCrashing();
  } else if (url == kChromeUIKillURL) {
    LOG(ERROR) << "Intentionally terminating current process because user"
                  " navigated to "
               << url.spec();
    // Simulate termination such that the base::GetTerminationStatus() API will
    // return TERMINATION_STATUS_PROCESS_WAS_KILLED.
#if BUILDFLAG(IS_WIN)
    base::Process::TerminateCurrentProcessImmediately(
        base::win::kProcessKilledExitCode);
#elif BUILDFLAG(IS_POSIX)
    PCHECK(kill(base::Process::Current().Pid(), SIGTERM) == 0);
#elif BUILDFLAG(IS_FUCHSIA)
    zx_process_exit(ZX_TASK_RETCODE_SYSCALL_KILL);
#else
#error Unsupported platform
#endif
  } else if (url == kChromeUIHangURL) {
    LOG(ERROR) << "Intentionally hanging ourselves with sleep infinite loop"
               << " because user navigated to " << url.spec();
    for (;;) {
      base::PlatformThread::Sleep(base::Seconds(1));
    }
  } else if (url == kChromeUIShorthangURL) {
    LOG(ERROR) << "Intentionally sleeping renderer for 20 seconds"
               << " because user navigated to " << url.spec();
    base::PlatformThread::Sleep(base::Seconds(20));
  } else if (url == kChromeUIMemoryExhaustURL) {
    LOG(ERROR)
        << "Intentionally exhausting renderer memory because user navigated to "
        << url.spec();
    ExhaustMemory();
  } else if (url == kChromeUICheckCrashURL) {
    LOG(ERROR) << "Intentionally causing CHECK because user navigated to "
               << url.spec();
    CHECK(false);
  }

#if BUILDFLAG(IS_WIN)
  if (url == kChromeUICfgViolationCrashURL) {
    LOG(ERROR) << "Intentionally causing cfg crash because user navigated to "
               << url.spec();
    base::debug::win::TerminateWithControlFlowViolation();
  }
  if (url == kChromeUIHeapCorruptionCrashURL) {
    LOG(ERROR)
        << "Intentionally causing heap corruption because user navigated to "
        << url.spec();
    base::debug::win::TerminateWithHeapCorruption();
  }
#endif

#if DCHECK_IS_ON()
  if (url == kChromeUICrashDcheckURL) {
    LOG(ERROR) << "Intentionally causing DCHECK because user navigated to "
               << url.spec();

    DCHECK(false) << "Intentional DCHECK.";
  }
#endif

#if defined(ADDRESS_SANITIZER)
  MaybeTriggerAsanError(url);
#endif  // ADDRESS_SANITIZER
}

}  // namespace blink
