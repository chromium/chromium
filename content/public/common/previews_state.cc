// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/previews_state.h"

#include "third_party/blink/public/platform/web_url_request.h"

#define STATIC_ASSERT_PREVIEWS_ENUM(a, b)                   \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

namespace content {

// Ensure that content::PreviewsState and blink::WebURLRequest::PreviewsState
// are kept in sync.
STATIC_ASSERT_PREVIEWS_ENUM(PREVIEWS_UNSPECIFIED,
                            blink::WebURLRequest::kPreviewsUnspecified);
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
STATIC_ASSERT_PREVIEWS_ENUM(LAZY_IMAGE_AUTO_RELOAD,
                            blink::WebURLRequest::kLazyImageAutoReload);
STATIC_ASSERT_PREVIEWS_ENUM(SUBRESOURCE_REDIRECT_ON,
                            blink::WebURLRequest::kSubresourceRedirectOn);
STATIC_ASSERT_PREVIEWS_ENUM(PREVIEWS_STATE_LAST,
                            blink::WebURLRequest::kSubresourceRedirectOn);

}  // namespace content
