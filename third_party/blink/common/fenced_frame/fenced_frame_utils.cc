// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"

#include <cstring>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/frame/fenced_frame_sandbox_flags.h"
#include "url/gurl.h"

namespace blink {

bool IsValidFencedFrameURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  return (url.SchemeIs(url::kHttpsScheme) || url.IsAboutBlank() ||
          net::IsLocalhost(url)) &&
         !url.parsed_for_possibly_invalid_spec().potentially_dangling_markup;
}

const char kURNUUIDprefix[] = "urn:uuid:";

bool IsValidUrnUuidURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  const std::string& spec = url.spec();
  return base::StartsWith(spec, kURNUUIDprefix,
                          base::CompareCase::INSENSITIVE_ASCII) &&
         base::Uuid::ParseCaseInsensitive(
             base::StringPiece(spec).substr(std::strlen(kURNUUIDprefix)))
             .is_valid();
}

void RecordFencedFrameCreationOutcome(
    const FencedFrameCreationOutcome outcome) {
  base::UmaHistogramEnumeration(
      kFencedFrameCreationOrNavigationOutcomeHistogram, outcome);
}

void RecordOpaqueFencedFrameSizeCoercion(bool did_coerce) {
  base::UmaHistogramBoolean(kIsOpaqueFencedFrameSizeCoercedHistogram,
                            did_coerce);
}

void RecordFencedFrameResizedAfterSizeFrozen() {
  base::UmaHistogramBoolean(kIsFencedFrameResizedAfterSizeFrozen, true);
}

void RecordFencedFrameUnsandboxedFlags(network::mojom::WebSandboxFlags flags) {
  using WebSandboxFlags = network::mojom::WebSandboxFlags;
  for (int32_t i = 1; i <= static_cast<int32_t>(WebSandboxFlags::kMaxValue);
       i = i << 1) {
    WebSandboxFlags current_mask = static_cast<WebSandboxFlags>(i);
    if ((flags & kFencedFrameMandatoryUnsandboxedFlags & current_mask) !=
        WebSandboxFlags::kNone) {
      base::UmaHistogramSparse(kFencedFrameMandatoryUnsandboxedFlagsSandboxed,
                               i);
    }
  }
}

void RecordFencedFrameFailedSandboxLoadInTopLevelFrame(bool is_main_frame) {
  base::UmaHistogramBoolean(kFencedFrameFailedSandboxLoadInTopLevelFrame,
                            is_main_frame);
}

}  // namespace blink
