// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/public/common/url_constants.h"

namespace content {

// Before adding new chrome schemes please check with security@chromium.org.
// There are security implications associated with introducing new schemes.
const char kChromeDevToolsScheme[] = "devtools";
const char kChromeErrorScheme[] = "chrome-error";
const char kChromeUIScheme[] = "chrome";
const char kGuestScheme[] = "chrome-guest";
const char kViewSourceScheme[] = "view-source";
#if defined(OS_CHROMEOS)
const char kExternalFileScheme[] = "externalfile";
#endif
const char kGoogleChromeScheme[] = "googlechrome";

const char kChromeUIAppCacheInternalsHost[] = "appcache-internals";
const char kChromeUIIndexedDBInternalsHost[] = "indexeddb-internals";
const char kChromeUIBlobInternalsHost[] = "blob-internals";
const char kChromeUIBrowserCrashHost[] = "inducebrowsercrashforrealz";
const char kChromeUIDinoHost[] = "dino";
const char kChromeUIGpuHost[] = "gpu";
const char kChromeUIHistogramHost[] = "histograms";
const char kChromeUIMediaInternalsHost[] = "media-internals";
const char kChromeUIMemoryExhaustHost[] = "memory-exhaust";
const char kChromeUINetworkErrorHost[] = "network-error";
const char kChromeUINetworkErrorsListingHost[] = "network-errors";
const char kChromeUIProcessInternalsHost[] = "process-internals";
const char kChromeUIResourcesHost[] = "resources";
const char kChromeUIServiceWorkerInternalsHost[] = "serviceworker-internals";
const char kChromeUITracingHost[] = "tracing";
const char kChromeUIWebRTCInternalsHost[] = "webrtc-internals";

const char kChromeUIBadCastCrashURL[] = "chrome://badcastcrash/";
const char kChromeUICheckCrashURL[] = "chrome://checkcrash/";
const char kChromeUIBrowserCrashURL[] = "chrome://inducebrowsercrashforrealz/";
const char kChromeUIBrowserUIHang[] = "chrome://uithreadhang/";
const char kChromeUICrashURL[] = "chrome://crash/";
const char kChromeUIDelayedBrowserUIHang[] = "chrome://delayeduithreadhang/";
const char kChromeUIDumpURL[] = "chrome://crashdump/";
const char kChromeUIGpuCleanURL[] = "chrome://gpuclean/";
const char kChromeUIGpuCrashURL[] = "chrome://gpucrash/";
const char kChromeUIGpuHangURL[] = "chrome://gpuhang/";
const char kChromeUIHangURL[] = "chrome://hang/";
const char kChromeUIKillURL[] = "chrome://kill/";
const char kChromeUIMemoryExhaustURL[] = "chrome://memory-exhaust/";
const char kChromeUINetworkErrorURL[] = "chrome://network-error/";
const char kChromeUINetworkErrorsListingURL[] = "chrome://network-errors/";
const char kChromeUIPpapiFlashCrashURL[] = "chrome://ppapiflashcrash/";
const char kChromeUIPpapiFlashHangURL[] = "chrome://ppapiflashhang/";
const char kChromeUIProcessInternalsURL[] = "chrome://process-internals";
#if defined(OS_ANDROID)
const char kChromeUIGpuJavaCrashURL[] = "chrome://gpu-java-crash/";
#endif
#if defined(OS_WIN)
const char kChromeUIBrowserHeapCorruptionURL[] =
    "chrome://inducebrowserheapcorruption/";
const char kChromeUIHeapCorruptionCrashURL[] = "chrome://heapcorruptioncrash/";
#endif
#if defined(ADDRESS_SANITIZER)
const char kChromeUICrashHeapOverflowURL[] = "chrome://crash/heap-overflow";
const char kChromeUICrashHeapUnderflowURL[] = "chrome://crash/heap-underflow";
const char kChromeUICrashUseAfterFreeURL[] = "chrome://crash/use-after-free";

#if defined(OS_WIN)
const char kChromeUICrashCorruptHeapBlockURL[] =
    "chrome://crash/corrupt-heap-block";
const char kChromeUICrashCorruptHeapURL[] = "chrome://crash/corrupt-heap";
#endif  // OS_WIN
#endif  // ADDRESS_SANITIZER

#if DCHECK_IS_ON()
const char kChromeUICrashDcheckURL[] = "chrome://crash/dcheck";
#endif

// This error URL is loaded in normal web renderer processes, so it should not
// have a chrome:// scheme that might let it be confused with a WebUI page.
const char kUnreachableWebDataURL[] = "chrome-error://chromewebdata/";

const char kBlockedURL[] = "about:blank#blocked";

const char kChromeUIResourcesURL[] = "chrome://resources/";
const char kChromeUIShorthangURL[] = "chrome://shorthang/";

}  // namespace content
