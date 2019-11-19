// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PREVIEWS_STATE_H_
#define CONTENT_PUBLIC_COMMON_PREVIEWS_STATE_H_

namespace content {

typedef int PreviewsState;

// The Previews types which determines whether to request a Preview version of
// the resource. Previews are optimizations that change the format and
// content of web pages to improve data savings and / or performance. This enum
// determines which Previews types to request.
enum PreviewsTypes {
  PREVIEWS_UNSPECIFIED = 0,  // Let the browser process decide whether or
                             // not to request Preview types.
  // DEPRECATED: SERVER_LOFI_ON = 1 << 0, Request a Lo-Fi version of the
  // resource from the server. This preview type has been deprecated and should
  // no longer be used.
  // DEPRECATED: CLIENT_LOFI_ON = 1 << 1, Request a Lo-Fi version of the
  // resource from the client. This preview type has been deprecated and should
  // no longer be used.
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
  LAZY_IMAGE_AUTO_RELOAD = 1 << 11,  // Request the full image after previously
                                     // getting a lazy load placeholder.
  DEFER_ALL_SCRIPT_ON = 1 << 12,  // Request that script execution be deferred
                                  // until parsing completes.
  SUBRESOURCE_REDIRECT_ON =
      1 << 13,  // Allow the subresources in the page to be redirected to
                // serve better optimized resources. Set on subresources.
  PREVIEWS_STATE_LAST = SUBRESOURCE_REDIRECT_ON
};

// Combination of all previews that are guaranteed not to provide partial
// content.
// const PreviewsState PARTIAL_CONTENT_SAFE_PREVIEWS = SERVER_LOFI_ON;
// deprecated

// Combination of all currently supported previews.
const PreviewsState ALL_SUPPORTED_PREVIEWS =
    SERVER_LITE_PAGE_ON | NOSCRIPT_ON | RESOURCE_LOADING_HINTS_ON |
    OFFLINE_PAGE_ON | LITE_PAGE_REDIRECT_ON;

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PREVIEWS_STATE_H_
