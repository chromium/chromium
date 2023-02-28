// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FENCED_FRAME_FENCED_FRAME_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FENCED_FRAME_FENCED_FRAME_UTILS_H_

#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "third_party/blink/public/common/common_export.h"

class GURL;

namespace blink {

// To prevent overloading the RAM, limit the maximum automatic beacon length
// to 64KB.
const size_t kFencedFrameMaxBeaconLength = 64000;

// Histogram names for fenced frame.
inline constexpr char kFencedFrameCreationOrNavigationOutcomeHistogram[] =
    "Blink.FencedFrame.CreationOrNavigationOutcome";

inline constexpr char kIsOpaqueFencedFrameSizeCoercedHistogram[] =
    "Blink.FencedFrame.IsOpaqueFrameSizeCoerced";

inline constexpr char kIsFencedFrameResizedAfterSizeFrozen[] =
    "Blink.FencedFrame.IsFrameResizedAfterSizeFrozen";

inline constexpr char kFencedFrameMandatoryUnsandboxedFlagsSandboxed[] =
    "Blink.FencedFrame.MandatoryUnsandboxedFlagsSandboxed";

inline constexpr char kFencedFrameFailedSandboxLoadInTopLevelFrame[] =
    "Blink.FencedFrame.FailedSandboxLoadInTopLevelFrame";

// Corresponds to the "FencedFrameCreationOutcome" histogram enumeration type in
// tools/metrics/histograms/enums.xml.
//
// PLEASE DO NOT REORDER, REMOVE, OR CHANGE THE MEANING OF THESE VALUES.
enum class FencedFrameCreationOutcome {
  kSuccessDefault = 0,  // creates/navigates in default mode
  kSuccessOpaque = 1,   // creates/navigates in opaque ads mode
  kSandboxFlagsNotSet = 2,
  kIncompatibleMode = 3,
  kInsecureContext = 4,
  kIncompatibleURLDefault = 5,
  kIncompatibleURLOpaque = 6,
  kResponseHeaderNotOptIn = 7,  // HTTP response header Supports-Loading-Mode
                                // is not opted-in with 'fenced-frame'
  kMaxValue = kResponseHeaderNotOptIn
};

// Whether or not a fenced frame is allowed to be navigated to `url`. For now
// this is described by
// https://github.com/WICG/fenced-frame/blob/master/explainer/modes.md.
BLINK_COMMON_EXPORT bool IsValidFencedFrameURL(const GURL& url);

// Whether or not a URL is a valid "urn uuid URL" depends not only on just the
// scheme being "urn", but that the URL's prefix is "urn:uuid".
BLINK_COMMON_EXPORT bool IsValidUrnUuidURL(const GURL& url);

// Record fenced frame related UMAs.
BLINK_COMMON_EXPORT void RecordFencedFrameCreationOutcome(
    const FencedFrameCreationOutcome outcome);
BLINK_COMMON_EXPORT void RecordOpaqueFencedFrameSizeCoercion(bool did_coerce);
BLINK_COMMON_EXPORT void RecordFencedFrameResizedAfterSizeFrozen();
BLINK_COMMON_EXPORT void RecordFencedFrameUnsandboxedFlags(
    network::mojom::WebSandboxFlags flags);
BLINK_COMMON_EXPORT void RecordFencedFrameFailedSandboxLoadInTopLevelFrame(
    bool is_main_frame);

// Automatic beacon type definitions
inline constexpr char kFencedFrameTopNavigationBeaconType[] =
    "reserved.top_navigation";

// Prefix of reserved event types for private aggregation API
inline constexpr char kFencedFrameReservedPAEventPrefix[] = "reserved.";

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FENCED_FRAME_FENCED_FRAME_UTILS_H_
