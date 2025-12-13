// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_later_util.h"

#include <algorithm>

#include "base/check.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_features.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/fetch/fetch_header_list.h"
#include "third_party/blink/renderer/core/fetch/fetch_request_data.h"
#include "third_party/blink/renderer/core/fetch/global_fetch.h"
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

// A convenient helper to tell if `feature` is enabled for `frame`.
bool IsFeatureEnabledForFrame(
    const Frame* frame,
    network::mojom::PermissionsPolicyFeature feature) {
  CHECK(frame);
  CHECK(frame->DomWindow());
  CHECK(frame->DomWindow()->IsLocalDOMWindow());
  auto* ec = frame->DomWindow()->ToLocalDOMWindow()->GetExecutionContext();
  return ec->IsFeatureEnabled(feature);
}

}  // namespace

// static
uint64_t FetchLaterUtil::GetUrlLengthWithoutFragment(const KURL& url) {
  KURL cloned_url = url;
  cloned_url.RemoveFragmentIdentifier();
  return cloned_url.GetString().length();
}

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

// static
uint32_t FetchLaterUtil::GetReservedDeferredFetchQuota(const Frame* frame) {
  CHECK(frame);

  // 3. Let isTopLevel be true if controlDocument’s node navigable is a
  // top-level traversable; otherwise false.
  bool is_top_level = frame->IsOutermostMainFrame();

  // 4. Let deferredFetchAllowed be true if controlDocument is allowed to use
  // the policy-controlled feature "deferred-fetch"; otherwise false.
  bool is_deferred_fetch_allowed = IsFeatureEnabledForFrame(
      frame, network::mojom::PermissionsPolicyFeature::kDeferredFetch);

  // 5. Let deferredFetchMinimalAllowed be true if controlDocument is allowed to
  // use the policy-controlled feature "deferred-fetch-minimal"; otherwise
  // false.
  bool is_deferred_fetch_minimal_allowed = IsFeatureEnabledForFrame(
      frame, network::mojom::PermissionsPolicyFeature::kDeferredFetchMinimal);

  // 6. Let quota be the result of the first matching statement:

  // 6-1. isTopLevel is true and deferredFetchAllowed is false => 0
  if (is_top_level && !is_deferred_fetch_allowed) {
    return 0;
  }
  // 6-2. isTopLevel is true, and deferredFetchMinimalAllowed is false => 0
  if (is_top_level && !is_deferred_fetch_minimal_allowed) {
    return 0;
  }
  // 6-3. isTopLevel is true => 512 kibibytes.
  if (is_top_level) {
    return kMaxScheduledDeferredBytes - kQuotaReservedForDeferredFetchMinimal;
  }

  // TODO(crbug.com/408106277): Until the spec is fixed, a temporarily 0 quota
  // is returned for non top-level control frames where they also don't have a
  // frame owner (iframe).
  if (!frame->Owner()) {
    return 0;
  }

  // `frame` must not be top-level and have a frame owner.
  auto container_policy =
      frame->Owner()->GetFramePolicy().deferred_fetch_policy;
  uint32_t container_reserved_quota =
      ToReservedDeferredFetchQuota(container_policy);

  // 6-4. deferredFetchAllowed is true, and navigable’s navigable container’s
  // reserved deferred-fetch quota is normal quota => normal quota.
  if (is_deferred_fetch_allowed &&
      container_reserved_quota == kNormalReservedDeferredFetchQuota) {
    return kNormalReservedDeferredFetchQuota;
  }

  // 6-5. deferredFetchMinimalAllowed is true, and navigable’s navigable
  // container’s reserved deferred-fetch quota is minimal quota => minimal
  // quota.
  if (is_deferred_fetch_minimal_allowed &&
      container_reserved_quota == kMinimalReservedDeferredFetchQuota) {
    return kMinimalReservedDeferredFetchQuota;
  }

  // 6-6. Otherwise => 0.
  return 0;
}

bool IsFetchLaterUseDeferredFetchPolicyEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kFetchLaterAPI, "use_deferred_fetch_policy", true);
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
    scoped_refptr<const SecurityOrigin> to_origin) {
  CHECK(container_frame);
  // Must be called when "inherited policy" is available for container document.
  CHECK(container_frame->ContentFrame());
  const auto to_url_origin = to_origin->ToUrlOrigin();
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
  const auto& feature_list =
      network::GetPermissionsPolicyFeatureList(to_url_origin);

  // 1. Set container’s reserved deferred-fetch quota to 0.

  // 2. Let controlDocument be container’s node document’s deferred-fetch
  // control document.
  Frame* control_frame =
      GetDeferredFetchControlFrame(container_frame->ContentFrame()->Parent());

  // 3. If the "inherited policy" for "deferred-fetch", container and
  // originToNavigateTo is Enabled, and the available deferred-fetch quota for
  // controlDocument is equal or greater than normal quota:
  auto deferred_fetch_it = feature_list.find(
      network::mojom::PermissionsPolicyFeature::kDeferredFetch);
  CHECK(deferred_fetch_it != feature_list.end());
  if (network::PermissionsPolicy::InheritedValueForFeature(
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
      network::mojom::PermissionsPolicyFeature::kDeferredFetchMinimal);
  CHECK(deferred_fetch_minimal_it != feature_list.end());
  if (!network::PermissionsPolicy::InheritedValueForFeature(
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

// static
uint64_t FetchLaterUtil::GetAvailableDeferredFetchQuota(Frame* frame,
                                                        const KURL& url) {
  CHECK(frame);
  // 1. Let controlDocument be document’s deferred-fetch control document.
  // 2. Let navigable be controlDocument’s node navigable.
  // NOTE: The wording "controlDocument" means `control_frame->GetDocument()`.
  Frame* control_frame = FetchLaterUtil::GetDeferredFetchControlFrame(frame);

  uint64_t quota = GetReservedDeferredFetchQuota(control_frame);

  // 7. Let quotaForRequestOrigin be 64 kibibytes.
  uint64_t quota_for_request_origin =
      kMaxPerRequestOriginScheduledDeferredBytes;

  // 8. For each navigable in controlDocument’s node navigable’s inclusive
  // descendant navigables
  for (Frame* navigable = control_frame; navigable;
       navigable = navigable->Tree().TraverseNext(control_frame)) {
    // whose active document’s deferred-fetch control document is
    // controlDocument:
    if (GetDeferredFetchControlFrame(navigable) != control_frame) {
      continue;
    }

    // 8-1. For each container in navigable’s active document’s shadow-including
    // inclusive descendants:
    // NOTE: these don't include `navigable`'s container itself.
    for (auto* container = navigable->Tree().TraverseNext(navigable); container;
         container = container->Tree().TraverseNext(navigable)) {
      // which is a navigable container
      if (container->Owner()) {
        // decrement quota by container’s reserved deferred-fetch quota.
        quota -= std::min(
            quota,
            static_cast<uint64_t>(ToReservedDeferredFetchQuota(
                container->Owner()->GetFramePolicy().deferred_fetch_policy)));
      }
    }

    // 8-2. For each deferred fetch record deferredRecord of controlDocument’s
    // fetch group’s deferred fetch records:
    auto* dom_window = DynamicTo<LocalDOMWindow>(navigable->DomWindow());
    if (!dom_window || !dom_window->GetExecutionContext()) {
      continue;
    }
    auto* scoped_fetcher = GlobalFetch::ScopedFetcher::From(*dom_window);
    if (!scoped_fetcher) {
      continue;
    }
    scoped_fetcher->UpdateDeferredBytesQuota(url, quota_for_request_origin,
                                             quota);
  }

  // 9. If quota is equal or less than 0, then return 0.
  if (quota == 0) {
    return 0;
  }

  // 10. If quota is less than quotaForRequestOrigin, then return quota.
  // 11. Return quotaForRequestOrigin.
  return quota < quota_for_request_origin ? quota : quota_for_request_origin;
}

uint64_t FetchLaterUtil::CalculateRequestSize(const FetchRequestData& request) {
  CHECK(!request.Buffer() || request.BufferByteLength() != 0);

  // 1. Let totalRequestLength be the length of request’s URL, serialized
  // with exclude fragment set to true.
  uint64_t total_request_length = GetUrlLengthWithoutFragment(request.Url());

  // 2. Increment totalRequestLength by the length of request’s referrer,
  // serialized.
  total_request_length += request.ReferrerString().Ascii().length();

  // 3. For each (name, value) in header list, increment totalRequestLength
  // by name’s length + value’s length.
  for (const auto& header : request.HeaderList()->List()) {
    total_request_length += header.first.length() + header.second.length();
  }

  // 4. Increment totalRequestLength by request’s body’s length.
  total_request_length += request.BufferByteLength();

  // 5. Return totalRequestLength.
  return total_request_length;
}

}  // namespace blink
