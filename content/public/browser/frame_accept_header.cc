// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/frame_accept_header.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "services/network/public/cpp/constants.h"
#include "third_party/blink/public/common/features.h"

namespace content {

std::string FrameAcceptHeaderValue(bool allow_sxg_responses,
                                   BrowserContext* browser_context) {
  std::string header_value = network::kFrameAcceptHeaderValue;
#if BUILDFLAG(ENABLE_AV1_DECODER)
  static const char kFrameAcceptHeaderValueWithAvif[] =
      "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,"
      "image/webp,image/apng,*/*;q=0.8";
  if (base::FeatureList::IsEnabled(blink::features::kAVIF))
    header_value = kFrameAcceptHeaderValueWithAvif;
#endif
  if (allow_sxg_responses &&
      content::signed_exchange_utils::IsSignedExchangeHandlingEnabled(
          browser_context)) {
    header_value.append(kAcceptHeaderSignedExchangeSuffix);
  }
  return header_value;
}

}  // namespace content
