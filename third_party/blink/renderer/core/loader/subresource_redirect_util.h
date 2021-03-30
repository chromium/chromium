// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_SUBRESOURCE_REDIRECT_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_SUBRESOURCE_REDIRECT_UTIL_H_

#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

// Enumerates the different ineligibility reasons for why a subresource
// redirection was disabled in blink. This is recorded in metrics and should not
// be reordered or removed. Should be in sync with the same name in enums.xml
enum class BlinkSubresourceRedirectIneligibility {
  // Created by javascript and is same-origin with the document.
  kJavascriptCreatedSameOrigin = 0,

  // Created by javascript and is cross-origin with the document.
  kJavascriptCreatedCrossOrigin = 1,

  // Restricted by Content-Security-Policy default-src directive.
  kContentSecurityPolicyDefaultSrcRestricted = 2,

  // Restricted by Content-Security-Policy img-src directive.
  kContentSecurityPolicyImgSrcRestricted = 3,

  // Crossorigin attribute was set which indicates CORS validation will happen
  // for the subresource.
  kCrossOriginAttributeSet = 4,

  // New enum entries should be added here.

  kMaxValue = kCrossOriginAttributeSet
};

// Returns whether subresource redirect can be attempted for fetching the image
// with |url|. Redirect to other origins could be disabled due to CSP or CORS
// restrictions.
bool ShouldEnableSubresourceRedirect(HTMLImageElement* image_element,
                                     const KURL& url);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_SUBRESOURCE_REDIRECT_UTIL_H_
