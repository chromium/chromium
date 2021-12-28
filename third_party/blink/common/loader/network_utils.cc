// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/network_utils.h"

#include "media/media_buildflags.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/features.h"

namespace blink {
namespace network_utils {

bool AlwaysAccessNetwork(
    const scoped_refptr<net::HttpResponseHeaders>& headers) {
  if (!headers)
    return false;

  // RFC 2616, section 14.9.
  return headers->HasHeaderValue("cache-control", "no-cache") ||
         headers->HasHeaderValue("cache-control", "no-store") ||
         headers->HasHeaderValue("pragma", "no-cache") ||
         headers->HasHeaderValue("vary", "*");
}

const char* ImageAcceptHeader() {
#if BUILDFLAG(ENABLE_JXL_DECODER) && BUILDFLAG(ENABLE_AV1_DECODER)
  if (base::FeatureList::IsEnabled(blink::features::kJXL)) {
    return "image/jxl,image/avif,image/webp,image/apng,image/svg+xml,image/*,*/"
           "*;q=0.8";
  } else {
    return "image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
  }
#elif BUILDFLAG(ENABLE_JXL_DECODER)
  if (base::FeatureList::IsEnabled(blink::features::kJXL)) {
    return "image/jxl,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
  } else {
    return "image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
  }
#elif BUILDFLAG(ENABLE_AV1_DECODER)
  return "image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
#else
  return "image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
#endif
}

}  // namespace network_utils
}  // namespace blink
