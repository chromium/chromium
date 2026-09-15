// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_CHROME_DEBUG_URLS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_CHROME_DEBUG_URLS_H_

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/common_export.h"

class GURL;

namespace blink {

// TODO(https://crbug.com/1197375): This file defines ChromeUI URLs that support
// triggering debug functionalities. These functionalities do not need to be
// Chrome-specific, and therefore defined in Blink, while they use ChromeUI
// specific URLs. Revisit to see if these URLs (and therefore functionalities
// too) should rather be implemented by embedders.

// Full about URLs (including schemes).
inline constexpr char kChromeUIBadCastCrashURL[] = "chrome://badcastcrash/";
inline constexpr char kChromeUIBrowserCrashURL[] =
    "chrome://inducebrowsercrashforrealz/";
inline constexpr char kChromeUIBrowserDcheckURL[] =
    "chrome://inducebrowserdcheckforrealz/";
inline constexpr char kChromeUIBrowserHeapUaFURL[] =
    "chrome://crash/browser/use-after-free";
inline constexpr char kChromeUIBrowserHeapOverflowURL[] =
    "chrome://crash/browser/heap-overflow";
inline constexpr char kChromeUIBrowserHeapUnderflowURL[] =
    "chrome://crash/browser/heap-underflow";
inline constexpr char kChromeUIBrowserHeapMemberDerefAfterFreeURL[] =
    "chrome://crash/browser/member-dereference-after-free";
inline constexpr char kChromeUIGpuHeapUaFURL[] =
    "chrome://crash/gpu/use-after-free";
inline constexpr char kChromeUIGpuHeapOverflowURL[] =
    "chrome://crash/gpu/heap-overflow";
inline constexpr char kChromeUIGpuHeapUnderflowURL[] =
    "chrome://crash/gpu/heap-underflow";
inline constexpr char kChromeUIGpuHeapMemberDerefAfterFreeURL[] =
    "chrome://crash/gpu/member-dereference-after-free";
inline constexpr char kChromeUIRendererHeapUaFURL[] =
    "chrome://crash/renderer/use-after-free";
inline constexpr char kChromeUIRendererHeapOverflowURL[] =
    "chrome://crash/renderer/heap-overflow";
inline constexpr char kChromeUIRendererHeapUnderflowURL[] =
    "chrome://crash/renderer/heap-underflow";
inline constexpr char kChromeUIRendererHeapMemberDerefAfterFreeURL[] =
    "chrome://crash/renderer/member-dereference-after-free";
inline constexpr char kChromeUIBrowserUIHang[] = "chrome://uithreadhang/";
inline constexpr char kChromeUICheckCrashURL[] = "chrome://checkcrash/";
inline constexpr char kChromeUICrashURL[] = "chrome://crash/";
inline constexpr char kChromeUIDelayedBrowserUIHang[] =
    "chrome://delayeduithreadhang/";
inline constexpr char kChromeUIDumpURL[] = "chrome://crashdump/";
inline constexpr char kChromeUIGpuCleanURL[] = "chrome://gpuclean/";
inline constexpr char kChromeUIGpuCrashURL[] = "chrome://gpucrash/";
inline constexpr char kChromeUIGpuHangURL[] = "chrome://gpuhang/";
inline constexpr char kChromeUIHangURL[] = "chrome://hang/";
inline constexpr char kChromeUIKillURL[] = "chrome://kill/";
inline constexpr char kChromeUIMemoryExhaustURL[] = "chrome://memory-exhaust/";
inline constexpr char kChromeUIMemoryPressureCriticalURL[] =
    "chrome://memory-pressure-critical/";
inline constexpr char kChromeUIMemoryPressureModerateURL[] =
    "chrome://memory-pressure-moderate/";
inline constexpr char kChromeUINetworkErrorsListingURL[] =
    "chrome://network-errors/";
inline constexpr char kChromeUINetworkErrorURL[] = "chrome://network-error/";
inline constexpr char kChromeUIProcessInternalsURL[] =
    "chrome://process-internals/";
inline constexpr char kChromeUIV8OOMURL[] = "chrome://v8-oom/";

#if BUILDFLAG(IS_ANDROID)
inline constexpr char kChromeUIGpuJavaCrashURL[] = "chrome://gpu-java-crash/";
#endif
inline constexpr char kChromeUICrashRustURL[] = "chrome://crash/rust";
#if BUILDFLAG(IS_WIN)
inline constexpr char kChromeUIBrowserHeapCorruptionURL[] =
    "chrome://inducebrowserheapcorruption/";
inline constexpr char kChromeUICfgViolationCrashURL[] = "chrome://crash/cfg";
inline constexpr char kChromeUIHeapCorruptionCrashURL[] =
    "chrome://heapcorruptioncrash/";
#endif

#if defined(ADDRESS_SANITIZER)
#if BUILDFLAG(IS_WIN)
inline constexpr char kChromeUICrashCorruptHeapBlockURL[] =
    "chrome://crash/corrupt-heap-block";
inline constexpr char kChromeUICrashCorruptHeapURL[] =
    "chrome://crash/corrupt-heap";
#endif  // BUILDFLAG(IS_WIN)
inline constexpr char kChromeUICrashRustOverflowURL[] =
    "chrome://crash/rust-overflow";
#endif  // ADDRESS_SANITIZER

#if DCHECK_IS_ON()
inline constexpr char kChromeUICrashDcheckURL[] = "chrome://crash/dcheck";
#endif

// Full about URLs (including schemes).
inline constexpr char kChromeUIResourcesURL[] = "chrome://resources/";
inline constexpr char kChromeUIShorthangURL[] = "chrome://shorthang/";

// Action names for generalized crash URLs.
inline constexpr char kAsanHeapOverflowAction[] = "heap-overflow";
inline constexpr char kAsanHeapUnderflowAction[] = "heap-underflow";
inline constexpr char kAsanUseAfterFreeAction[] = "use-after-free";
inline constexpr char kAsanMemberDereferenceAfterFreeAction[] =
    "member-dereference-after-free";

// Process names for generalized crash URLs.
inline constexpr char kAsanBrowserProcess[] = "browser";
inline constexpr char kAsanGpuProcess[] = "gpu";
inline constexpr char kAsanRendererProcess[] = "renderer";

// Returns whether the given url is either a debugging url handled in the
// renderer process, such as one that crashes or hangs the renderer, or a
// javascript: URL that operates on the current page in the renderer.  Such URLs
// do not represent actual navigations and can be loaded in any SiteInstance.
// Parses a generalized crash URL in the format chrome://crash/<process>/<type>.
// Returns true if parsing was successful and populates `process` and
// `crash_type`.
BLINK_COMMON_EXPORT bool ParseCrashURL(const GURL& url,
                                       std::string* process,
                                       std::string* crash_type);

BLINK_COMMON_EXPORT bool IsRendererDebugURL(const GURL& url);

// Handles the given debug URL. These URLs do not commit, though they are
// intentionally left in the address bar above the effect they cause
// (e.g., a sad tab).
BLINK_COMMON_EXPORT void HandleChromeDebugURL(const GURL& url);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_CHROME_DEBUG_URLS_H_
