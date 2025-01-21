// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_later_util.h"

#include "base/check.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_features.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/fetch/fetch_header_list.h"
#include "third_party/blink/renderer/core/fetch/fetch_request_data.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request_utils.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {
namespace {

// The max containers with minimal quota.
constexpr uint32_t kMaxContainersWithMinimalQuota =
    kQuotaReservedForDeferredFetchMinimal / kMinimalReservedDeferredFetchQuota;

}  // namespace

// static
uint32_t FetchLaterUtil::ToReservedDeferredFetchQuota(
    mojom::blink::DeferredFetchPolicy policy) {
  switch (policy) {
    case mojom::blink::DeferredFetchPolicy::kDisabled:
      return 0;
    case mojom::blink::DeferredFetchPolicy::kDeferredFetch:
      return kNormalReservedDeferredFetchQuota;
    case mojom::blink::DeferredFetchPolicy::kDeferredFetchMinimal:
      return kMinimalReservedDeferredFetchQuota;
  }
}

// static
bool FetchLaterUtil::AreSameOrigin(const Frame* frame_a, const Frame* frame_b) {
  CHECK(frame_a);
  CHECK(frame_b);

  return frame_a->GetSecurityContext()->GetSecurityOrigin()->IsSameOriginWith(
      frame_b->GetSecurityContext()->GetSecurityOrigin());
}

// static
uint32_t FetchLaterUtil::CountDescendantsWithReservedMinimalQuota(
    const Frame* control_frame) {
  CHECK(control_frame);
  uint32_t count = 0;

  // The size of controlDocument’s node navigable’s descendant navigables:
  for (const auto* navigable = control_frame->FirstChild(); navigable;
       navigable = navigable->Tree().TraverseNext(control_frame)) {
    // removing any navigable whose navigable container’s reserved
    // deferred-fetch quota is not minimal quota.
    if (auto* navigable_container = navigable->Owner();
        navigable_container &&
        navigable_container->GetFramePolicy().deferred_fetch_policy ==
            mojom::blink::DeferredFetchPolicy::kDeferredFetchMinimal) {
      count++;
    }
  }

  return count;
}

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

// static
Frame* FetchLaterUtil::GetDeferredFetchControlFrame(Frame* frame) {
  CHECK(frame);
  // If document’ node navigable’s container document is null or a Document
  // whose origin is not same origin with document, return document;
  if (!frame->Parent() || !AreSameOrigin(frame, frame->Parent())) {
    return frame;
  }

  // Otherwise return the deferred-fetch control document given document’ node
  // navigable’s container document.
  return GetDeferredFetchControlFrame(frame->Parent());
}

// static
mojom::blink::DeferredFetchPolicy
FetchLaterUtil::GetContainerDeferredFetchPolicyOnNavigation(
    FrameOwner* container_frame,
    const KURL& to_url) {
  CHECK(container_frame);
  // Must be called when "inherited policy" is available for container document.
  CHECK(container_frame->ContentFrame());
  auto to_url_origin = SecurityOrigin::Create(to_url)->ToUrlOrigin();
  // At this moment, the overall "inherited policies" for `container_frame`'s
  // features are not yet calculated, which only happens in the call to
  // `PermissionsPolicy::CreateFromParentPolicy()` made by
  // `DocumentLoader::CommitNavigation()` .
  // Hence, we manually trigger calculation for the deferred-fetch features.
  auto* parent_permissions_policy =
      container_frame->ContentFrame()->Tree().Parent()
          ? container_frame->ContentFrame()
                ->Tree()
                .Parent()
                ->GetSecurityContext()
                ->GetPermissionsPolicy()
          : nullptr;
  const auto& feature_list = GetPermissionsPolicyFeatureList(to_url_origin);

  // 1. Set container’s reserved deferred-fetch quota to 0.

  // 2. Let controlDocument be container’s node document’s deferred-fetch
  // control document.
  Frame* control_frame =
      GetDeferredFetchControlFrame(container_frame->ContentFrame()->Parent());

  // 3. If the "inherited policy" for "deferred-fetch", container and
  // originToNavigateTo is Enabled,
  // TODO(crbug.com/40276121): and the available deferred-fetch quota for
  // controlDocument is equal or greater than normal quota:
  auto deferred_fetch_it =
      feature_list.find(mojom::blink::PermissionsPolicyFeature::kDeferredFetch);
  CHECK(deferred_fetch_it != feature_list.end());
  if (PermissionsPolicy::InheritedValueForFeature(
          to_url_origin, parent_permissions_policy, *deferred_fetch_it,
          container_frame->GetFramePolicy().container_policy)) {
    // then set container’s reserved deferred-fetch quota to normal quota and
    // return.
    return mojom::blink::DeferredFetchPolicy::kDeferredFetch;
  }

  // 4. If all of the following conditions are true:

  // 4-1. controlDocument’s node navigable is a top-level traversable.
  if (!control_frame->IsOutermostMainFrame()) {
    return mojom::blink::DeferredFetchPolicy::kDisabled;
  }
  // 4-2. The "inherited policy" for "deferred-fetch-minimal", container and
  // originToNavigateTo is Enabled.
  auto deferred_fetch_minimal_it = feature_list.find(
      mojom::blink::PermissionsPolicyFeature::kDeferredFetchMinimal);
  CHECK(deferred_fetch_minimal_it != feature_list.end());
  if (!PermissionsPolicy::InheritedValueForFeature(
          to_url_origin, parent_permissions_policy, *deferred_fetch_minimal_it,
          container_frame->GetFramePolicy().container_policy)) {
    // then return.
    return mojom::blink::DeferredFetchPolicy::kDisabled;
  }

  // 4-3. The size of controlDocument’s ...
  // is less than quota reserved for deferred-fetch-minimal / minimal quota:
  // then set container’s reserved deferred-fetch quota to minimal quota.
  return CountDescendantsWithReservedMinimalQuota(control_frame) <
                 kMaxContainersWithMinimalQuota
             ? mojom::blink::DeferredFetchPolicy::kDeferredFetchMinimal
             : mojom::blink::DeferredFetchPolicy::kDisabled;
}

// static
bool FetchLaterUtil::ShouldClearDeferredFetchPolicy(Frame* frame) {
  CHECK(frame);
  // if document’s node navigable’s container document is not null, and its
  // origin is same origin with document:
  // then set document’s node navigable’s navigable container’s reserved
  // deferred-fetch quota to 0.
  return frame->Parent() && AreSameOrigin(frame, frame->Parent());
}

}  // namespace blink
