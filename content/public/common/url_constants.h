// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_URL_CONSTANTS_H_
#define CONTENT_PUBLIC_COMMON_URL_CONSTANTS_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "device/vr/buildflags/buildflags.h"
#include "url/url_constants.h"

// Contains constants for known URLs and portions thereof.

namespace content {

// Canonical schemes you can use as input to GURL.SchemeIs().
// TODO(jam): some of these don't below in the content layer, but are accessed
// from there.
inline constexpr char kChromeDevToolsScheme[] = "devtools";
inline constexpr char kChromeErrorScheme[] = "chrome-error";
inline constexpr char kChromeUIScheme[] = "chrome";  // Used for WebUIs.
inline constexpr char kChromeUIUntrustedScheme[] = "chrome-untrusted";
inline constexpr char kViewSourceScheme[] = "view-source";
#if BUILDFLAG(IS_CHROMEOS_ASH)
inline constexpr char kExternalFileScheme[] = "externalfile";
#endif
#if BUILDFLAG(IS_ANDROID)
inline constexpr char kAndroidAppScheme[] = "android-app";
#endif

// The `googlechrome:` scheme is registered on several platforms, and is
// both interesting and dangerous.
inline constexpr char kGoogleChromeScheme[] = "googlechrome";

inline constexpr char kChromeUIAttributionInternalsHost[] =
    "attribution-internals";
inline constexpr char kChromeUIBlobInternalsHost[] = "blob-internals";
inline constexpr char kChromeUIBrowserCrashHost[] =
    "inducebrowsercrashforrealz";
inline constexpr char kChromeUIDinoHost[] = "dino";
inline constexpr char kChromeUIGpuHost[] = "gpu";
inline constexpr char kChromeUIHistogramHost[] = "histograms";
inline constexpr char kChromeUIIndexedDBInternalsHost[] = "indexeddb-internals";
inline constexpr char kChromeUIMediaInternalsHost[] = "media-internals";
inline constexpr char kChromeUIMemoryExhaustHost[] = "memory-exhaust";
inline constexpr char kChromeUINetworkErrorHost[] = "network-error";
inline constexpr char kChromeUINetworkErrorsListingHost[] = "network-errors";
inline constexpr char kChromeUIPrivateAggregationInternalsHost[] =
    "private-aggregation-internals";
inline constexpr char kChromeUIProcessInternalsHost[] = "process-internals";
inline constexpr char kChromeUIQuotaInternalsHost[] = "quota-internals";
inline constexpr char kChromeUIResourcesHost[] = "resources";
inline constexpr char kChromeUIServiceWorkerInternalsHost[] =
    "serviceworker-internals";
inline constexpr char kChromeUITracesInternalsHost[] = "traces-internals";
inline constexpr char kChromeUITracingHost[] = "tracing";
inline constexpr char kChromeUIUkmHost[] = "ukm";
inline constexpr char kChromeUIUntrustedResourcesURL[] =
    "chrome-untrusted://resources/";
inline constexpr char kChromeUIWebRTCInternalsHost[] = "webrtc-internals";
#if BUILDFLAG(ENABLE_VR)
inline constexpr char kChromeUIWebXrInternalsHost[] = "webxr-internals";
#endif

// Special URL used to start a navigation to an error page.
// This error URL is loaded in normal web renderer processes, so it should not
// have a chrome:// scheme that might let it be confused with a WebUI page.
inline constexpr char kUnreachableWebDataURL[] =
    "chrome-error://chromewebdata/";

// Special URL used to rewrite URLs coming from untrusted processes, when the
// source process is not allowed access to the initially requested URL.
inline constexpr char kBlockedURL[] = "about:blank#blocked";

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_URL_CONSTANTS_H_
