// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_URL_CONSTANTS_H_
#define CONTENT_PUBLIC_COMMON_URL_CONSTANTS_H_

#include "base/logging.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "url/url_constants.h"

// Contains constants for known URLs and portions thereof.

namespace content {

// Canonical schemes you can use as input to GURL.SchemeIs().
// TODO(jam): some of these don't below in the content layer, but are accessed
// from there.
CONTENT_EXPORT extern const char kChromeDevToolsScheme[];
CONTENT_EXPORT extern const char kChromeErrorScheme[];
CONTENT_EXPORT extern const char kChromeUIScheme[];  // Used for WebUIs.
CONTENT_EXPORT extern const char kGuestScheme[];
CONTENT_EXPORT extern const char kViewSourceScheme[];
#if defined(OS_CHROMEOS)
CONTENT_EXPORT extern const char kExternalFileScheme[];
#endif

// The `googlechrome:` scheme is registered on several platforms, and is
// both interesting and dangerous.
CONTENT_EXPORT extern const char kGoogleChromeScheme[];

CONTENT_EXPORT extern const char kChromeUIAccessibilityHost[];
CONTENT_EXPORT extern const char kChromeUIAppCacheInternalsHost[];
CONTENT_EXPORT extern const char kChromeUIBlobInternalsHost[];
CONTENT_EXPORT extern const char kChromeUIBrowserCrashHost[];
CONTENT_EXPORT extern const char kChromeUIDinoHost[];
CONTENT_EXPORT extern const char kChromeUIGpuHost[];
CONTENT_EXPORT extern const char kChromeUIHistogramHost[];
CONTENT_EXPORT extern const char kChromeUIIndexedDBInternalsHost[];
CONTENT_EXPORT extern const char kChromeUIMediaInternalsHost[];
CONTENT_EXPORT extern const char kChromeUIMemoryExhaustHost[];
CONTENT_EXPORT extern const char kChromeUINetworkErrorHost[];
CONTENT_EXPORT extern const char kChromeUINetworkErrorsListingHost[];
CONTENT_EXPORT extern const char kChromeUIProcessInternalsHost[];
CONTENT_EXPORT extern const char kChromeUIResourcesHost[];
CONTENT_EXPORT extern const char kChromeUIServiceWorkerInternalsHost[];
CONTENT_EXPORT extern const char kChromeUITracingHost[];
CONTENT_EXPORT extern const char kChromeUIWebRTCInternalsHost[];

// Full about URLs (including schemes).
CONTENT_EXPORT extern const char kChromeUIBadCastCrashURL[];
CONTENT_EXPORT extern const char kChromeUICheckCrashURL[];
CONTENT_EXPORT extern const char kChromeUIBrowserCrashURL[];
CONTENT_EXPORT extern const char kChromeUIBrowserUIHang[];
CONTENT_EXPORT extern const char kChromeUICrashURL[];
CONTENT_EXPORT extern const char kChromeUIDelayedBrowserUIHang[];
CONTENT_EXPORT extern const char kChromeUIDumpURL[];
CONTENT_EXPORT extern const char kChromeUIGpuCleanURL[];
CONTENT_EXPORT extern const char kChromeUIGpuCrashURL[];
CONTENT_EXPORT extern const char kChromeUIGpuHangURL[];
CONTENT_EXPORT extern const char kChromeUIHangURL[];
CONTENT_EXPORT extern const char kChromeUIKillURL[];
CONTENT_EXPORT extern const char kChromeUIMemoryExhaustURL[];
CONTENT_EXPORT extern const char kChromeUINetworkErrorsListingURL[];
CONTENT_EXPORT extern const char kChromeUINetworkErrorURL[];
CONTENT_EXPORT extern const char kChromeUIPpapiFlashCrashURL[];
CONTENT_EXPORT extern const char kChromeUIPpapiFlashHangURL[];
CONTENT_EXPORT extern const char kChromeUIProcessInternalsURL[];
#if defined(OS_ANDROID)
CONTENT_EXPORT extern const char kChromeUIGpuJavaCrashURL[];
#endif
#if defined(OS_WIN)
CONTENT_EXPORT extern const char kChromeUIBrowserHeapCorruptionURL[];
CONTENT_EXPORT extern const char kChromeUIHeapCorruptionCrashURL[];
#endif
#if defined(ADDRESS_SANITIZER)
CONTENT_EXPORT extern const char kChromeUICrashHeapOverflowURL[];
CONTENT_EXPORT extern const char kChromeUICrashHeapUnderflowURL[];
CONTENT_EXPORT extern const char kChromeUICrashUseAfterFreeURL[];
#if defined(OS_WIN)
CONTENT_EXPORT extern const char kChromeUICrashCorruptHeapBlockURL[];
CONTENT_EXPORT extern const char kChromeUICrashCorruptHeapURL[];
#endif  // OS_WIN
#endif  // ADDRESS_SANITIZER

#if DCHECK_IS_ON()
CONTENT_EXPORT extern const char kChromeUICrashDcheckURL[];
#endif

// Special URL used to start a navigation to an error page.
CONTENT_EXPORT extern const char kUnreachableWebDataURL[];

// Special URL used to rewrite URLs coming from untrusted processes, when the
// source process is not allowed access to the initially requested URL.
CONTENT_EXPORT extern const char kBlockedURL[];

// Full about URLs (including schemes).
CONTENT_EXPORT extern const char kChromeUINetworkViewCacheURL[];
CONTENT_EXPORT extern const char kChromeUIResourcesURL[];
CONTENT_EXPORT extern const char kChromeUIShorthangURL[];

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_URL_CONSTANTS_H_
