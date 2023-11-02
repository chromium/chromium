// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/frame_accept_header.h"

#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/common/content_constants_internal.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/features.h"

namespace content {

std::string FrameAcceptHeaderValue(bool allow_sxg_responses,
                                   BrowserContext* browser_context) {
  std::string header_value = kFrameAcceptHeaderValue;

#if BUILDFLAG(ENABLE_JXL_DECODER)
  // In case the buildflag and the runtime flag are enables, we insert
  // "image/jxl," into the header value at the correct place.
  if (base::FeatureList::IsEnabled(blink::features::kJXL)) {
    std::string header_value_prefix = header_value.substr(0, 54);
    std::string header_value_suffix = header_value.substr(54);
    header_value = header_value_prefix;
    header_value.append("image/jxl,");
    header_value.append(header_value_suffix);
  }
#endif  // BUILDFLAG(ENABLE_JXL_DECODER)
  if (allow_sxg_responses &&
      content::signed_exchange_utils::IsSignedExchangeHandlingEnabled(
          browser_context)) {
    header_value.append(kAcceptHeaderSignedExchangeSuffix);
  }
  return header_value;
}

}  // namespace content
