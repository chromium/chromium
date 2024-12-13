// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_LATER_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_LATER_UTIL_H_

#include <stdint.h>

#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/renderer/core/core_export.h"
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

// The minimal quota is 8 kibibytes, one of the possible values for
// "reserved deferred-fetch quota".
// https://whatpr.org/fetch/1647.html#reserved-deferred-fetch-quota-minimal-quota
inline constexpr uint32_t kMinimalReservedDeferredFetchQuota = 8 * 1024;

// The normal quota is 64 kibibytes, one of the possible values for
// "reserved deferred-fetch quota".
// https://whatpr.org/fetch/1647.html#reserved-deferred-fetch-quota-normal-quota
inline constexpr uint32_t kNormalReservedDeferredFetchQuota = 64 * 1024;

// Tells whether the FetchLater API should use subframe deferred fetch
// policy to decide whether a frame show allow using the API.
bool CORE_EXPORT IsFetchLaterUseDeferredFetchPolicyEnabled();

// Computes resource loader priority for a FetchLater request.
ResourceLoadPriority CORE_EXPORT
ComputeFetchLaterLoadPriority(const FetchParameters& params);

// Determines the deferred fetch policy of a navigable container
// `container_frame`, e.g. iframe, when it navigates its content to a target
// URL, by the following algorithm:
// https://whatpr.org/fetch/1647.html#reserve-deferred-fetch-quota
// `container_frame` is a FrameOwner on navigation, i.e. an iframe.
// Returns an enum that can be mapped to a "reserved deferred-fetch quota" by
// calling `ToReservedDeferredFetchQuota()`.
//
// This must be called after "inherited policy" for `container_frame` is
// available, i.e. after `PermissionsPolicy::CreateFromParentPolicy()` is
// already executed.
FramePolicy::DeferredFetchPolicy CORE_EXPORT
GetContainerDeferredFetchPolicyOnNavigation(FrameOwner* container_frame);

// For testing only:
uint32_t CORE_EXPORT
ToReservedDeferredFetchQuotaForTesting(FramePolicy::DeferredFetchPolicy policy);
bool CORE_EXPORT AreSameOriginForTesting(const Frame* frame_a,
                                         const Frame* frame_b);
uint32_t CORE_EXPORT CountContainersWithReservedMinimalQuotaForTesting(
    const FrameOwner* container_frame);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_LATER_UTIL_H_
