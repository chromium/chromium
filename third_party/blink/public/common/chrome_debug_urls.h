// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_CHROME_DEBUG_URLS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_CHROME_DEBUG_URLS_H_

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/common_export.h"

class GURL;

namespace blink {

// TODO(https://crbug.com/1197375): This file defines ChromeUI URLs that support
// triggering debug functionalities. These functionalities do not need to be
// Chrome-specific, and therefore defined in Blink, while they use ChromeUI
// specific URLs. Revisit to see if these URLs (and therefore functionalities
// too) should rather be implemented by embedders.

// Full about URLs (including schemes).
BLINK_COMMON_EXPORT extern const char kChromeUIBadCastCrashURL[];
BLINK_COMMON_EXPORT extern const char kChromeUICheckCrashURL[];
BLINK_COMMON_EXPORT extern const char kChromeUIBrowserCrashURL[];
BLINK_COMMON_EXPORT extern const char kChromeUIBrowserDcheckURL[];
BLINK_COMMON_EXPORT extern const char kChromeUIBrowserUIHang[];
BLINK_COMMON_EXPORT extern const char kChromeUICrashURL[];
#if BUILDFLAG(BUILD_RUST_CRASH)
BLINK_COMMON_EXPORT extern const char kChromeUICrashRustURL[];
#endif
BLINK_COMMON_EXPORT extern const char kChromeUIDelayedBrowserUIHang[];
BLINK_COMMON_EXPORT extern const char kChromeUIDumpURL[];
BLINK_COMMON_EXPORT extern const char kChromeUIGpuCleanURL[];
BLINK_COMMON_EXPORT extern const char kChromeUIGpuCrashURL[];
BLINK_COMMON_EXPORT extern const char kChromeUIGpuHangURL[];
BLINK_COMMON_EXPORT extern const char kChromeUIHangURL[];
BLINK_COMMON_EXPORT extern const char kChromeUIKillURL[];
BLINK_COMMON_EXPORT extern const char kChromeUIMemoryExhaustURL[];
BLINK_COMMON_EXPORT extern const char kChromeUIMemoryPressureCriticalURL[];
BLINK_COMMON_EXPORT extern const char kChromeUIMemoryPressureModerateURL[];
BLINK_COMMON_EXPORT extern const char kChromeUINetworkErrorsListingURL[];
BLINK_COMMON_EXPORT extern const char kChromeUINetworkErrorURL[];
BLINK_COMMON_EXPORT extern const char kChromeUIProcessInternalsURL[];
#if BUILDFLAG(IS_ANDROID)
BLINK_COMMON_EXPORT extern const char kChromeUIGpuJavaCrashURL[];
#endif
#if BUILDFLAG(IS_WIN)
BLINK_COMMON_EXPORT extern const char kChromeUIBrowserHeapCorruptionURL[];
BLINK_COMMON_EXPORT extern const char kChromeUICfgViolationCrashURL[];
BLINK_COMMON_EXPORT extern const char kChromeUIHeapCorruptionCrashURL[];
#endif

#if defined(ADDRESS_SANITIZER)
BLINK_COMMON_EXPORT extern const char kChromeUICrashHeapOverflowURL[];
BLINK_COMMON_EXPORT extern const char kChromeUICrashHeapUnderflowURL[];
BLINK_COMMON_EXPORT extern const char kChromeUICrashUseAfterFreeURL[];
#if BUILDFLAG(IS_WIN)
BLINK_COMMON_EXPORT extern const char kChromeUICrashCorruptHeapBlockURL[];
BLINK_COMMON_EXPORT extern const char kChromeUICrashCorruptHeapURL[];
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(BUILD_RUST_CRASH)
BLINK_COMMON_EXPORT extern const char kChromeUICrashRustOverflowURL[];
#endif  // BUILDFLAG(BUILD_RUST_CRASH)
#endif  // ADDRESS_SANITIZER

#if DCHECK_IS_ON()
BLINK_COMMON_EXPORT extern const char kChromeUICrashDcheckURL[];
#endif

// Full about URLs (including schemes).
BLINK_COMMON_EXPORT extern const char kChromeUIResourcesURL[];
BLINK_COMMON_EXPORT extern const char kChromeUIShorthangURL[];

// Returns whether the given url is either a debugging url handled in the
// renderer process, such as one that crashes or hangs the renderer, or a
// javascript: URL that operates on the current page in the renderer.  Such URLs
// do not represent actual navigations and can be loaded in any SiteInstance.
BLINK_COMMON_EXPORT bool IsRendererDebugURL(const GURL& url);

// Handles the given debug URL. These URLs do not commit, though they are
// intentionally left in the address bar above the effect they cause
// (e.g., a sad tab).
BLINK_COMMON_EXPORT void HandleChromeDebugURL(const GURL& url);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_CHROME_DEBUG_URLS_H_
