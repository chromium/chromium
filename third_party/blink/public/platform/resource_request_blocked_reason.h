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
  // The request was blocked by the WebRequest extensions API's
  // onBeforeSendHeaders, onBeforeRequest, and onHeadersReceived stages. (This
  // doesn't cover some other cases, like onAuthRequired and WebSocket
  // requests.)
  //
  // This is a short-term addition to help investigate some Trust Tokens
  // issuance request failures: see https://crbug.com/1128174.
  //
  // TODO(crbug.com/1133944): Remove this once the investigation finishes.
  kBlockedByExtensionCrbug1128174Investigation,
};
}  // namespace blink

#endif
