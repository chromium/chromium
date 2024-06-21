// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/debug_urls.h"

#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/debug/asan_invalid_access.h"
#include "base/debug/profiler.h"
#include "base/functional/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/sanitizer_buildflags.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/browser/ppapi_plugin_process_host.h"  // nogncheck
#include "ppapi/proxy/ppapi_messages.h"                 // nogncheck
#endif

#if BUILDFLAG(IS_WIN)
#include "base/debug/invalid_access_win.h"
#endif

namespace content {

class ScopedAllowWaitForDebugURL {
 private:
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope wait;
};

namespace {

// Define the Asan debug URLs.
const char kAsanCrashDomain[] = "crash";
const char kAsanHeapOverflow[] = "/browser-heap-overflow";
const char kAsanHeapUnderflow[] = "/browser-heap-underflow";
const char kAsanUseAfterFree[] = "/browser-use-after-free";

#if BUILDFLAG(IS_WIN)
const char kAsanCorruptHeapBlock[] = "/browser-corrupt-heap-block";
const char kAsanCorruptHeap[] = "/browser-corrupt-heap";
#endif

bool IsAsanDebugURL(const GURL& url) {
  if (!(url.is_valid() && url.SchemeIs(kChromeUIScheme) &&
        url.DomainIs(kAsanCrashDomain) && url.has_path())) {
    return false;
  }

  if (url.path_piece() == kAsanHeapOverflow ||
      url.path_piece() == kAsanHeapUnderflow ||
      url.path_piece() == kAsanUseAfterFree) {
    return true;
  }

#if BUILDFLAG(IS_WIN)
  if (url.path_piece() == kAsanCorruptHeapBlock ||
      url.path_piece() == kAsanCorruptHeap) {
    return true;
  }
#endif

  return false;
}

bool HandleAsanDebugURL(const GURL& url) {
#if defined(ADDRESS_SANITIZER) || BUILDFLAG(IS_HWASAN)
#if BUILDFLAG(IS_WIN)
  if (url.path_piece() == kAsanCorruptHeapBlock) {
    base::debug::AsanCorruptHeapBlock();
    return true;
  } else if (url.path_piece() == kAsanCorruptHeap) {
    base::debug::AsanCorruptHeap();
    return true;
  }
#endif  // BUILDFLAG(IS_WIN)

  if (url.path_piece() == kAsanHeapOverflow) {
    base::debug::AsanHeapOverflow();
  } else if (url.path_piece() == kAsanHeapUnderflow) {
    base::debug::AsanHeapUnderflow();
  } else if (url.path_piece() == kAsanUseAfterFree) {
    base::debug::AsanHeapUseAfterFree();
  } else {
    return false;
  }
#endif

  return true;
}

NOINLINE void HangCurrentThread() {
  ScopedAllowWaitForDebugURL allow_wait;
  base::WaitableEvent(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                      base::WaitableEvent::InitialState::NOT_SIGNALED)
      .Wait();
}

NOINLINE void CrashBrowserProcessIntentionally() {
  // Don't fold so that crash reports will clearly show this method. This helps
  // with crash triage.
  NO_CODE_FOLDING();
  // Induce an intentional crash in the browser process.
  CHECK(false);
}

}  // namespace

bool HandleDebugURL(const GURL& url,
                    ui::PageTransition transition,
                    bool is_explicit_navigation) {
  // We want to handle the debug URL if the user explicitly navigated to this
  // URL, unless kEnableGpuBenchmarking is enabled by Telemetry.
  bool is_telemetry_navigation =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          cc::switches::kEnableGpuBenchmarking) &&
      (PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED));

  if (!is_explicit_navigation && !is_telemetry_navigation)
    return false;

  if (IsAsanDebugURL(url))
    return HandleAsanDebugURL(url);

  if (url == blink::kChromeUIBrowserCrashURL) {
    CrashBrowserProcessIntentionally();
    return true;
  }

  if (url == blink::kChromeUIBrowserDcheckURL) {
    // Induce an intentional DCHECK in the browser process. This is used to
    // see if a DCHECK will bring down the current process (is FATAL).
    DCHECK(false);
    return true;
  }

#if BUILDFLAG(IS_WIN)
  if (url == blink::kChromeUIBrowserHeapCorruptionURL) {
    // Induce an intentional heap corruption in the browser process.
    base::debug::win::TerminateWithHeapCorruption();
  }
#endif

  if (url == blink::kChromeUIBrowserUIHang) {
    HangCurrentThread();
    return true;
  }

  if (url == blink::kChromeUIDelayedBrowserUIHang) {
    // Webdriver-safe url to hang the ui thread. Webdriver waits for the onload
    // event in javascript which needs a little more time to fire.
    GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE, base::BindOnce(&HangCurrentThread), base::Seconds(2));
    return true;
  }

  if (url == blink::kChromeUIGpuCleanURL) {
    auto* host = GpuProcessHost::Get();
    if (host) {
      host->gpu_service()->DestroyAllChannels();
    }
    return true;
  }

  if (url == blink::kChromeUIGpuCrashURL) {
    auto* host = GpuProcessHost::Get();
    if (host) {
      host->gpu_service()->Crash();
    }
    return true;
  }

#if BUILDFLAG(IS_ANDROID)
  if (url == blink::kChromeUIGpuJavaCrashURL) {
    auto* host = GpuProcessHost::Get();
    if (host) {
      host->gpu_service()->ThrowJavaException();
    }
    return true;
  }
#endif

  if (url == blink::kChromeUIGpuHangURL) {
    auto* host = GpuProcessHost::Get();
    if (host) {
      host->gpu_service()->Hang();
    }
    return true;
  }

  if (url == blink::kChromeUIMemoryPressureCriticalURL) {
    base::MemoryPressureListener::NotifyMemoryPressure(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
    return true;
  }

  if (url == blink::kChromeUIMemoryPressureModerateURL) {
    base::MemoryPressureListener::NotifyMemoryPressure(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
    return true;
  }

  return false;
}

}  // namespace content
