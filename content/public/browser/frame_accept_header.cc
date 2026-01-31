// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/frame_accept_header.h"

#include "base/feature_list.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/common/content_constants_internal.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/features.h"

namespace content {

std::string FrameAcceptHeaderValue(bool allow_sxg_responses,
                                   BrowserContext* browser_context) {
#if BUILDFLAG(ENABLE_JXL_DECODER)
  std::string header_value =
      base::FeatureList::IsEnabled(blink::features::kJXLImageFormat)
          ? kFrameAcceptHeaderValueWithJxl
          : kFrameAcceptHeaderValue;
#else
  std::string header_value = kFrameAcceptHeaderValue;
#endif

  if (allow_sxg_responses &&
      content::signed_exchange_utils::IsSignedExchangeHandlingEnabled(
          browser_context)) {
    header_value.append(kAcceptHeaderSignedExchangeSuffix);
  }
  return header_value;
}

}  // namespace content
