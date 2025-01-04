// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_later_util.h"

#include "base/check.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/fetch/fetch_header_list.h"
#include "third_party/blink/renderer/core/fetch/fetch_request_data.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request_utils.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {
namespace {

// The max containers with minimal quota is 16.
// https://whatpr.org/fetch/1647.html#max-containers-with-minimal-quota
constexpr uint32_t kMaxContainersWithMinimalQuota = 16;

// Converts `policy` to one of possible values of reserved deferred-fetch quota.
// https://whatpr.org/fetch/1647.html#reserved-deferred-fetch-quota
uint32_t ToReservedDeferredFetchQuota(FramePolicy::DeferredFetchPolicy policy) {
  switch (policy) {
    case FramePolicy::DeferredFetchPolicy::kDisabled:
      return 0;
    case FramePolicy::DeferredFetchPolicy::kDeferredFetch:
      return kNormalReservedDeferredFetchQuota;
    case FramePolicy::DeferredFetchPolicy::kDeferredFetchMinimal:
      return kMinimalReservedDeferredFetchQuota;
  }
}

// Tells if the given two frames shares the same origin.
// https://html.spec.whatwg.org/multipage/browsers.html#same-origin
bool AreSameOrigin(const Frame* frame_a, const Frame* frame_b) {
  CHECK(frame_a);
  CHECK(frame_b);

  return frame_a->GetSecurityContext()->GetSecurityOrigin()->IsSameOriginWith(
      frame_b->GetSecurityContext()->GetSecurityOrigin());
}

// Calculates the total number of frames according to Step 5 of
// https://whatpr.org/fetch/1647.html#reserve-deferred-fetch-quota
//
// `container_frame` is an iframe to count the result for. Note that it must be
// an iframe with `PermissionsPolicyFeature::kDeferredFetchMinimal` enabled
// before calling this function.
//
// Example (with default Permissions Policy on every origin):
//    root (a.com) -> frame-1 (a.com)
//                 -> frame-2 (a.com)
//                 -> frame-3 (b.com)
//                 -> frame-4 (b.com)
// * `container_frame` cannot be frame-1, frame-2.
// * When `container_frame` = frame-3 or frame-4, the result is 2.
uint32_t CountContainersWithReservedMinimalQuota(
    const FrameOwner* container_frame) {
  CHECK(container_frame);
  uint32_t count = 0;

  // 5. Let containersWithReservedMinimalQuota be container’s node navigable’s
  // top-level traversable’s descendant navigables
  auto* top_frame = container_frame->ContentFrame()->Top();
  CHECK(top_frame);
  for (const auto* navigable = top_frame; navigable;
       navigable = navigable->Tree().TraverseNext(top_frame)) {
    // removing any navigable whose reserved deferred-fetch quota is not minimal
    // quota.
    if (auto* navigable_container = navigable->Owner();
        navigable_container &&
        navigable_container->GetFramePolicy().deferred_fetch_policy ==
            FramePolicy::DeferredFetchPolicy::kDeferredFetchMinimal) {
      count++;
    }
  }

  return count;
}

}  // namespace

bool IsFetchLaterUseDeferredFetchPolicyEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kFetchLaterAPI, "use_deferred_fetch_policy", false);
}

ResourceLoadPriority ComputeFetchLaterLoadPriority(
    const FetchParameters& params) {
  // FetchLater's ResourceType is ResourceType::kRaw, which should default to
  // ResourceLoadPriority::kHigh priority. See also TypeToPriority() in
  // resource_fetcher.cc
  return AdjustPriorityWithPriorityHintAndRenderBlocking(
      ResourceLoadPriority::kHigh, kFetchLaterResourceType,
      params.GetResourceRequest().GetFetchPriorityHint(),
      params.GetRenderBlockingBehavior());
  // TODO(crbug.com/40276121): Apply kLow when
  // IsSubframeDeprioritizationEnabled.
}

FramePolicy::DeferredFetchPolicy GetContainerDeferredFetchPolicyOnNavigation(
    FrameOwner* container_frame) {
  CHECK(container_frame);
  // Must be called when "inherited policy" is available for container document.
  CHECK(container_frame->ContentFrame());
  CHECK(container_frame->ContentFrame()->IsLocalFrame());
  // Called after committing navigation, so the frame must be local.
  KURL to_url =
      To<LocalFrame>(container_frame->ContentFrame())->GetDocument()->Url();
  auto to_url_origin = SecurityOrigin::Create(to_url)->ToUrlOrigin();
  auto* permissions_policy = container_frame->ContentFrame()
                                 ->GetSecurityContext()
                                 ->GetPermissionsPolicy();
  // 1. Set container’s reserved deferred-fetch quota to 0.

  // 2. If the inherited policy for "deferred-fetch", container and
  // originToNavigateTo is Enabled,
  // TODO(crbug.com/40276121): and the available deferred fetching quota for
  // container’s container document is equal or greater than normal quota:
  if (permissions_policy->IsFeatureEnabledForOrigin(
          mojom::blink::PermissionsPolicyFeature::kDeferredFetch,
          to_url_origin)) {
    // then set container’s reserved deferred-fetch quota to normal quota and
    // return.
    return FramePolicy::DeferredFetchPolicy::kDeferredFetch;
  }
  // 3. If the inherited policy for "deferred-fetch-minimal", container and
  // originToNavigateTo is Disabled:
  if (!permissions_policy->IsFeatureEnabledForOrigin(
          mojom::blink::PermissionsPolicyFeature::kDeferredFetchMinimal,
          to_url_origin)) {
    // then return.
    return FramePolicy::DeferredFetchPolicy::kDisabled;
  }
  // 4. If container’s node document's origin is not same origin with
  // container’s node navigable’s top-level traversable’s active document’s
  // origin:
  if (!AreSameOrigin(container_frame->ContentFrame()->Parent(),
                     container_frame->ContentFrame()->Top())) {
    // then return.
    return FramePolicy::DeferredFetchPolicy::kDisabled;
  }

  // 5. Let containersWithReservedMinimalQuota be ...
  // 6. If containersWithReservedMinimalQuota’s size is less than max containers
  // with minimal quota, then set container’s reserved deferred-fetch quota to
  // minimal quota.
  return CountContainersWithReservedMinimalQuota(container_frame) <
                 kMaxContainersWithMinimalQuota
             ? FramePolicy::DeferredFetchPolicy::kDeferredFetchMinimal
             : FramePolicy::DeferredFetchPolicy::kDisabled;
}

// For testing only:
uint32_t ToReservedDeferredFetchQuotaForTesting(
    FramePolicy::DeferredFetchPolicy policy) {
  return ToReservedDeferredFetchQuota(policy);
}

bool AreSameOriginForTesting(const Frame* frame_a, const Frame* frame_b) {
  return AreSameOrigin(frame_a, frame_b);
}

uint32_t CountContainersWithReservedMinimalQuotaForTesting(
    const FrameOwner* container_frame) {
  return CountContainersWithReservedMinimalQuota(container_frame);
}

}  // namespace blink
