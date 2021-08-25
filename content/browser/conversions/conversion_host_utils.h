// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_HOST_UTILS_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_HOST_UTILS_H_

#include "content/browser/conversions/storable_impression.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/navigation/impression.h"

namespace url {
class Origin;
}

namespace content {

class BrowserContext;
class ConversionManager;

namespace conversion_host_utils {

// Performs required checks on an incoming impression's data (trustworthy
// origins, etc), and if verified, generates a StorableImpression and persists
// it. Returns false if the ContentClient blocked this measurement operation.
// Note that a return value of true does not guarantee the impression was
// stored.
bool VerifyAndStoreImpression(StorableImpression::SourceType source_type,
                              const url::Origin& impression_origin,
                              const blink::Impression& impression,
                              BrowserContext* browser_context,
                              ConversionManager& conversion_manager);

CONTENT_EXPORT absl::optional<blink::Impression> ParseImpressionFromApp(
    const std::string& attribution_source_event_id,
    const std::string& attribution_destination,
    const std::string& attribution_report_to,
    int64_t attribution_expiry) WARN_UNUSED_RESULT;

}  // namespace conversion_host_utils

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_HOST_UTILS_H_
