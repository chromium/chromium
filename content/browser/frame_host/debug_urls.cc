// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/debug_urls.h"

#include <vector>

#include "base/command_line.h"
#include "base/debug/asan_invalid_access.h"
#include "base/debug/profiler.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "ppapi/buildflags/buildflags.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/browser/ppapi_plugin_process_host.h"  // nogncheck
#include "ppapi/proxy/ppapi_messages.h"  // nogncheck
#endif

#if defined(OS_WIN)
#include "base/debug/invalid_access_win.h"
#endif

namespace content {

class ScopedAllowWaitForDebugURL {
 private:
  base::ThreadRestrictions::ScopedAllowWait wait;
};

namespace {

// Define the Asan debug URLs.
const char kAsanCrashDomain[] = "crash";
const char kAsanHeapOverflow[] = "/browser-heap-overflow";
const char kAsanHeapUnderflow[] = "/browser-heap-underflow";
const char kAsanUseAfterFree[] = "/browser-use-after-free";

#if defined(OS_WIN)
const char kAsanCorruptHeapBlock[] = "/browser-corrupt-heap-block";
const char kAsanCorruptHeap[] = "/browser-corrupt-heap";
#endif

void HandlePpapiFlashDebugURL(const GURL& url) {
#if BUILDFLAG(ENABLE_PLUGINS)
  bool crash = url == kChromeUIPpapiFlashCrashURL;

  std::vector<PpapiPluginProcessHost*> hosts;
  PpapiPluginProcessHost::FindByName(
      base::UTF8ToUTF16(kFlashPluginName), &hosts);
  for (auto iter = hosts.begin(); iter != hosts.end(); ++iter) {
    if (crash)
      (*iter)->Send(new PpapiMsg_Crash());
    else
      (*iter)->Send(new PpapiMsg_Hang());
  }
#endif
}

bool IsAsanDebugURL(const GURL& url) {
  if (!(url.is_valid() && url.SchemeIs(kChromeUIScheme) &&
        url.DomainIs(kAsanCrashDomain) &&
        url.has_path())) {
    return false;
  }

  if (url.path_piece() == kAsanHeapOverflow ||
      url.path_piece() == kAsanHeapUnderflow ||
      url.path_piece() == kAsanUseAfterFree) {
    return true;
  }

#if defined(OS_WIN)
  if (url.path_piece() == kAsanCorruptHeapBlock ||
      url.path_piece() == kAsanCorruptHeap) {
    return true;
  }
#endif

  return false;
}

bool HandleAsanDebugURL(const GURL& url) {
#if defined(ADDRESS_SANITIZER)
#if defined(OS_WIN)
  if (url.path_piece() == kAsanCorruptHeapBlock) {
    base::debug::AsanCorruptHeapBlock();
    return true;
  } else if (url.path_piece() == kAsanCorruptHeap) {
    base::debug::AsanCorruptHeap();
    return true;
  }
#endif  // OS_WIN

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

void HangCurrentThread() {
  ScopedAllowWaitForDebugURL allow_wait;
  base::WaitableEvent(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                      base::WaitableEvent::InitialState::NOT_SIGNALED)
      .Wait();
}

}  // namespace

bool HandleDebugURL(const GURL& url, ui::PageTransition transition) {
  // Ensure that the user explicitly navigated to this URL, unless
  // kEnableGpuBenchmarking is enabled by Telemetry.
  bool is_telemetry_navigation =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          cc::switches::kEnableGpuBenchmarking) &&
      (PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED));

  if (!(transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) &&
      !is_telemetry_navigation)
    return false;

  if (IsAsanDebugURL(url))
    return HandleAsanDebugURL(url);

  if (url == kChromeUIBrowserCrashURL) {
    // Induce an intentional crash in the browser process.
    CHECK(false);
    return true;
  }

#if defined(OS_WIN)
  if (url == kChromeUIBrowserHeapCorruptionURL) {
    // Induce an intentional heap corruption in the browser process.
    base::debug::win::TerminateWithHeapCorruption();
    return true;
  }
#endif

  if (url == kChromeUIBrowserUIHang) {
    HangCurrentThread();
    return true;
  }

  if (url == kChromeUIDelayedBrowserUIHang) {
    // Webdriver-safe url to hang the ui thread. Webdriver waits for the onload
    // event in javascript which needs a little more time to fire.
    base::PostDelayedTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                                    base::BindOnce(&HangCurrentThread),
                                    base::TimeDelta::FromSeconds(2));
    return true;
  }

  if (url == kChromeUIGpuCleanURL) {
    GpuProcessHost::CallOnIO(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                             false /* force_create */,
                             base::Bind([](GpuProcessHost* host) {
                               if (host)
                                 host->gpu_service()->DestroyAllChannels();
                             }));
    return true;
  }

  if (url == kChromeUIGpuCrashURL) {
    GpuProcessHost::CallOnIO(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                             false /* force_create */,
                             base::Bind([](GpuProcessHost* host) {
                               if (host)
                                 host->gpu_service()->Crash();
                             }));
    return true;
  }

#if defined(OS_ANDROID)
  if (url == kChromeUIGpuJavaCrashURL) {
    GpuProcessHost::CallOnIO(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                             false /* force_create */,
                             base::Bind([](GpuProcessHost* host) {
                               if (host)
                                 host->gpu_service()->ThrowJavaException();
                             }));
    return true;
  }
#endif

  if (url == kChromeUIGpuHangURL) {
    GpuProcessHost::CallOnIO(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                             false /* force_create */,
                             base::Bind([](GpuProcessHost* host) {
                               if (host)
                                 host->gpu_service()->Hang();
                             }));
    return true;
  }

  if (url == kChromeUIPpapiFlashCrashURL || url == kChromeUIPpapiFlashHangURL) {
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                             base::BindOnce(&HandlePpapiFlashDebugURL, url));
    return true;
  }

  return false;
}

}  // namespace content
