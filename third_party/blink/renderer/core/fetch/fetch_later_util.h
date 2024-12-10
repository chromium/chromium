// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_LATER_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_LATER_UTIL_H_

#include <stdint.h>

#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {
class Frame;
class FrameOwner;

// The ResourceType of FetchLater requests.
inline constexpr ResourceType kFetchLaterResourceType = ResourceType::kRaw;

// 64 kibibytes.
inline constexpr uint32_t kInitialSubframeDeferredFetchBytes = 64 * 1024;

// 8 kibibytes.
inline constexpr uint32_t kInitialSubframeDeferredFetchMinimalBytes = 8 * 1024;

// Tells whether the FetchLater API should use subframe deferred fetch
// policy to decide whether a frame show allow using the API.
bool CORE_EXPORT IsFetchLaterUseDeferredFetchPolicyEnabled();

// Computes resource loader priority for a FetchLater request.
ResourceLoadPriority CORE_EXPORT
ComputeFetchLaterLoadPriority(const FetchParameters& params);

// Returns all frames that shares the same deferred fetch quota with `frame`,
// i.e. all same-origin same-process frames of `frame`.
// Note that the result includes the `frame` itself if not null.
// https://whatpr.org/fetch/1647.html#deferred-fetch-quota-sharing-navigables
HeapHashSet<Member<Frame>> CORE_EXPORT
GetDeferredFetchQuotaSharingFrames(Frame* frame);

// Determines the deferred fetch policy of a navigable container
// `container_frame`, e.g. iframe, when it navigates its content to a target
// URL, by the following algorithm:
// https://whatpr.org/fetch/1647.html#determine-subframe-deferred-fetch-policy
// This must be called after "inherited policy" for `container_frame` is
// available, i.e. after `PermissionsPolicy::CreateFromParentPolicy()` is
// already executed.
FramePolicy::DeferredFetchPolicy CORE_EXPORT
GetContainerDeferredFetchPolicyOnNavigation(FrameOwner* container_frame);

// For testing only:
uint32_t CORE_EXPORT CountFramesWithMinimalQuotaPolicyForTesting(
    FrameOwner* container_frame,
    const HeapHashSet<Member<Frame>>& top_level_relatives);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_LATER_UTIL_H_
