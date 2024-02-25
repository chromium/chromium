// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_URL_CONSTANTS_H_
#define CONTENT_PUBLIC_COMMON_URL_CONSTANTS_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/common/content_export.h"
#include "device/vr/buildflags/buildflags.h"
#include "url/url_constants.h"

// Contains constants for known URLs and portions thereof.

namespace content {

// Canonical schemes you can use as input to GURL.SchemeIs().
// TODO(jam): some of these don't below in the content layer, but are accessed
// from there.
CONTENT_EXPORT extern const char kChromeDevToolsScheme[];
CONTENT_EXPORT extern const char kChromeErrorScheme[];
CONTENT_EXPORT extern const char kChromeUIScheme[];  // Used for WebUIs.
CONTENT_EXPORT extern const char kChromeUIUntrustedScheme[];
CONTENT_EXPORT extern const char kViewSourceScheme[];
#if BUILDFLAG(IS_CHROMEOS_ASH)
CONTENT_EXPORT extern const char kExternalFileScheme[];
#endif
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT extern const char kAndroidAppScheme[];
#endif

// The `googlechrome:` scheme is registered on several platforms, and is
// both interesting and dangerous.
CONTENT_EXPORT extern const char kGoogleChromeScheme[];

CONTENT_EXPORT extern const char kChromeUIAccessibilityHost[];
CONTENT_EXPORT extern const char kChromeUIAttributionInternalsHost[];
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
CONTENT_EXPORT extern const char kChromeUIPrivateAggregationInternalsHost[];
CONTENT_EXPORT extern const char kChromeUIProcessInternalsHost[];
CONTENT_EXPORT extern const char kChromeUIQuotaInternalsHost[];
CONTENT_EXPORT extern const char kChromeUIResourcesHost[];
CONTENT_EXPORT extern const char kChromeUIServiceWorkerInternalsHost[];
CONTENT_EXPORT extern const char kChromeUITracesInternalsHost[];
CONTENT_EXPORT extern const char kChromeUITracingHost[];
CONTENT_EXPORT extern const char kChromeUIUkmHost[];
CONTENT_EXPORT extern const char kChromeUIUntrustedResourcesURL[];
CONTENT_EXPORT extern const char kChromeUIWebRTCInternalsHost[];
#if BUILDFLAG(ENABLE_VR)
CONTENT_EXPORT extern const char kChromeUIWebXrInternalsHost[];
#endif

// Special URL used to start a navigation to an error page.
CONTENT_EXPORT extern const char kUnreachableWebDataURL[];

// Special URL used to rewrite URLs coming from untrusted processes, when the
// source process is not allowed access to the initially requested URL.
CONTENT_EXPORT extern const char kBlockedURL[];

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_URL_CONSTANTS_H_
