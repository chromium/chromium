// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_KURL_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_KURL_MOJOM_TRAITS_H_

#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "url/mojom/url.mojom-blink-forward.h"
#include "url/url_constants.h"

namespace mojo {

template <>
struct StructTraits<url::mojom::blink::Url::DataView, ::blink::KURL> {
  static WTF::String url(const ::blink::KURL& blinkUrl) {
    if (!blinkUrl.IsValid() ||
        blinkUrl.GetString().length() > url::kMaxURLChars) {
      return g_empty_string;
    }

    return blinkUrl.GetString();
  }
  static bool Read(url::mojom::blink::Url::DataView data, ::blink::KURL* out) {
    WTF::String urlString;
    if (!data.ReadUrl(&urlString))
      return false;

    if (urlString.length() > url::kMaxURLChars)
      return false;

    *out = ::blink::KURL(::blink::KURL(), urlString);
    if (!urlString.IsEmpty() && !out->IsValid())
      return false;

    return true;
  }
};
}

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_KURL_MOJOM_TRAITS_H_
