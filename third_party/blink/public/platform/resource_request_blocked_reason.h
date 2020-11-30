// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_RESOURCE_REQUEST_BLOCKED_REASON_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_RESOURCE_REQUEST_BLOCKED_REASON_H_

namespace blink {
// If updating this enum, also update DevTools protocol usages.
// Contact devtools owners for help.
enum class ResourceRequestBlockedReason {
  kOther,
  kCSP,
  kMixedContent,
  kOrigin,
  kInspector,
  kSubresourceFilter,
  kContentType,
  kCollapsedByClient,
  kCoepFrameResourceNeedsCoepHeader,
  kCoopSandboxedIFrameCannotNavigateToCoopPage,
  kCorpNotSameOrigin,
  kCorpNotSameOriginAfterDefaultedToSameOriginByCoep,
  kCorpNotSameSite,
};
}  // namespace blink

#endif
