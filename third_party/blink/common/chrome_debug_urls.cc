// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/chrome_debug_urls.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/debug/asan_invalid_access.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/strings/string_split.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "third_party/blink/common/crash_helpers.h"
#include "third_party/blink/common/rust_crash/src/lib.rs.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/debug/invalid_access_win.h"
#include "base/process/kill.h"
#elif BUILDFLAG(IS_POSIX)
#include <signal.h>
#elif BUILDFLAG(IS_FUCHSIA)
#include <zircon/syscalls.h>
#endif


namespace blink {

bool ParseCrashURL(const GURL& url,
                   std::string* process,
                   std::string* crash_type) {
  if (!(url.is_valid() && url.SchemeIs("chrome") && url.DomainIs("crash") &&
        url.has_path())) {
    return false;
  }
  std::string_view path = url.path();
  if (path.empty() || path[0] != '/') {
    return false;
  }
  std::vector<std::string_view> parts = base::SplitStringPiece(
      path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() != 2) {
    return false;
  }
  *process = std::string(parts[0]);
  *crash_type = std::string(parts[1]);
  return true;
}

bool IsRendererDebugURL(const GURL& url) {
  if (!url.is_valid())
    return false;

  if (url.SchemeIs(url::kJavaScriptScheme))
    return true;

  if (!url.SchemeIs("chrome"))
    return false;

  std::string process;
  std::string crash_type;
  if (ParseCrashURL(url, &process, &crash_type)) {
    if (process == kAsanRendererProcess) {
      return (crash_type == kAsanHeapOverflowAction ||
              crash_type == kAsanHeapUnderflowAction ||
              crash_type == kAsanUseAfterFreeAction ||
              crash_type == kAsanMemberDereferenceAfterFreeAction);
    }
    return false;
  }

  if (url == kChromeUICheckCrashURL || url == kChromeUIBadCastCrashURL ||
      url == kChromeUICrashURL || url == kChromeUIDumpURL ||
      url == kChromeUIKillURL || url == kChromeUIHangURL ||
      url == kChromeUIShorthangURL || url == kChromeUIMemoryExhaustURL ||
      url == kChromeUIV8OOMURL || url == kChromeUICrashRustURL) {
    return true;
  }

#if defined(ADDRESS_SANITIZER)
  if (url == kChromeUICrashRustOverflowURL) {
    return true;
  }
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
#if BUILDFLAG(IS_WIN)
  if (url == kChromeUICrashCorruptHeapBlockURL) {
    LOG(ERROR) << "Intentionally causing ASAN corrupt heap block"
               << " because user navigated to " << url.spec();
    base::debug::AsanCorruptHeapBlock();
    return;
  }
  if (url == kChromeUICrashCorruptHeapURL) {
    LOG(ERROR) << "Intentionally causing ASAN corrupt heap"
               << " because user navigated to " << url.spec();
    base::debug::AsanCorruptHeap();
    return;
  }
#endif  // BUILDFLAG(IS_WIN)
  if (url == kChromeUICrashRustOverflowURL) {
    // Ensure that ASAN works even in Rust code.
    LOG(ERROR) << "Intentionally causing ASAN heap overflow in Rust"
               << " because user navigated to " << url.spec();
    crash_in_rust_with_overflow();
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
  } else if (url == kChromeUICrashRustURL) {
    // Cause a typical crash in Rust code, so we can test that call stack
    // collection and symbol mangling work across the language boundary.
    crash_in_rust();
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
  } else {
    std::string process;
    std::string crash_type;
    if (ParseCrashURL(url, &process, &crash_type)) {
      if (process == kAsanRendererProcess) {
        if (crash_type == kAsanHeapOverflowAction) {
          LOG(ERROR) << "Intentionally causing Renderer Heap Overflow"
                     << " because user navigated to " << url.spec();
          base::debug::AsanHeapOverflow();
        } else if (crash_type == kAsanHeapUnderflowAction) {
          LOG(ERROR) << "Intentionally causing Renderer Heap Underflow"
                     << " because user navigated to " << url.spec();
          base::debug::AsanHeapUnderflow();
        } else if (crash_type == kAsanUseAfterFreeAction) {
          LOG(ERROR) << "Intentionally causing Renderer Heap UaF"
                     << " because user navigated to " << url.spec();
          base::debug::AsanHeapUseAfterFree();
        } else if (crash_type == kAsanMemberDereferenceAfterFreeAction) {
          LOG(ERROR)
              << "Intentionally causing Renderer Heap Member Dereference UaF"
              << " because user navigated to " << url.spec();
          base::debug::AsanHeapMemberDereferenceAfterFree();
        }
      }
    }
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
