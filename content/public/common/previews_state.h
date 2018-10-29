// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PREVIEWS_STATE_H_
#define CONTENT_PUBLIC_COMMON_PREVIEWS_STATE_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/platform/web_url_request.h"

#define STATIC_ASSERT_PREVIEWS_ENUM(a, b)                   \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

namespace content {

typedef int PreviewsState;

// The Previews types which determines whether to request a Preview version of
// the resource. Previews are optimizations that change the format and
// content of web pages to improve data savings and / or performance. This enum
// determines which Previews types to request.
enum PreviewsTypes {
  PREVIEWS_UNSPECIFIED = 0,  // Let the browser process decide whether or
                             // not to request Preview types.
  SERVER_LOFI_ON = 1 << 0,   // Request a Lo-Fi version of the resource
                             // from the server.
  CLIENT_LOFI_ON = 1 << 1,   // Request a Lo-Fi version of the resource
                             // from the client.
  CLIENT_LOFI_AUTO_RELOAD = 1 << 2,  // Request the original version of the
                                     // resource after a decoding error occurred
                                     // when attempting to use Client Lo-Fi.
  SERVER_LITE_PAGE_ON = 1 << 3,      // Request a Lite Page version of the
                                     // resource from the server.
  PREVIEWS_NO_TRANSFORM = 1 << 4,    // Explicitly forbid Previews
                                     // transformations.
  PREVIEWS_OFF = 1 << 5,  // Request a normal (non-Preview) version of
                          // the resource. Server transformations may
                          // still happen if the page is heavy.
  NOSCRIPT_ON = 1 << 6,   // Request that script be disabled for page load.
  RESOURCE_LOADING_HINTS_ON =
      1 << 7,  // Request that resource loading hints be used during pageload.
  OFFLINE_PAGE_ON =
      1 << 8,  // Request that an offline page be used if one is stored.
  LITE_PAGE_REDIRECT_ON = 1 << 9,  // Allow the browser to redirect the resource
                                   // to a Lite Page server.
  LAZY_IMAGE_LOAD_DEFERRED = 1 << 10,  // Request the placeholder version of an
                                       // image that was deferred by lazyload.
  PREVIEWS_STATE_LAST = LAZY_IMAGE_LOAD_DEFERRED
};

// Combination of all previews that are guaranteed not to provide partial
// content.
const PreviewsState PARTIAL_CONTENT_SAFE_PREVIEWS = SERVER_LOFI_ON;

// Ensure that content::PreviewsState and blink::WebURLRequest::PreviewsState
// are kept in sync.
STATIC_ASSERT_PREVIEWS_ENUM(PREVIEWS_UNSPECIFIED,
                            blink::WebURLRequest::kPreviewsUnspecified);
STATIC_ASSERT_PREVIEWS_ENUM(SERVER_LOFI_ON,
                            blink::WebURLRequest::kServerLoFiOn);
STATIC_ASSERT_PREVIEWS_ENUM(CLIENT_LOFI_ON,
                            blink::WebURLRequest::kClientLoFiOn);
STATIC_ASSERT_PREVIEWS_ENUM(CLIENT_LOFI_AUTO_RELOAD,
                            blink::WebURLRequest::kClientLoFiAutoReload);
STATIC_ASSERT_PREVIEWS_ENUM(SERVER_LITE_PAGE_ON,
                            blink::WebURLRequest::kServerLitePageOn);
STATIC_ASSERT_PREVIEWS_ENUM(PREVIEWS_NO_TRANSFORM,
                            blink::WebURLRequest::kPreviewsNoTransform);
STATIC_ASSERT_PREVIEWS_ENUM(PREVIEWS_OFF, blink::WebURLRequest::kPreviewsOff);
STATIC_ASSERT_PREVIEWS_ENUM(NOSCRIPT_ON, blink::WebURLRequest::kNoScriptOn);
STATIC_ASSERT_PREVIEWS_ENUM(RESOURCE_LOADING_HINTS_ON,
                            blink::WebURLRequest::kResourceLoadingHintsOn);
STATIC_ASSERT_PREVIEWS_ENUM(OFFLINE_PAGE_ON,
                            blink::WebURLRequest::kOfflinePageOn);
STATIC_ASSERT_PREVIEWS_ENUM(LITE_PAGE_REDIRECT_ON,
                            blink::WebURLRequest::kLitePageRedirectOn);
STATIC_ASSERT_PREVIEWS_ENUM(LAZY_IMAGE_LOAD_DEFERRED,
                            blink::WebURLRequest::kLazyImageLoadDeferred);
STATIC_ASSERT_PREVIEWS_ENUM(PREVIEWS_STATE_LAST,
                            blink::WebURLRequest::kPreviewsStateLast);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PREVIEWS_STATE_H_
