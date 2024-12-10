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

// Calculates "framesWithMinimalQuotaPolicy" by running Step 7 of
// https://whatpr.org/fetch/1647.html#determine-subframe-deferred-fetch-policy
//
// `container_frame` is an iframe to decide deferred fetch policy for.
// `top_level_relatives` is from executing the following for the parent of
// `container_frame`:
// https://whatpr.org/fetch/1647.html#available-deferred-fetching-quota
uint32_t CountFramesWithMinimalQuotaPolicy(
    FrameOwner* container_frame,
    const HeapHashSet<Member<Frame>>& top_level_relatives) {
  CHECK(container_frame);
  uint32_t count = 0;

  for (const auto& relative : top_level_relatives) {
    //  7-2. topLevelRelatives contains navigable’s parent.
    for (Frame* navigable = relative->FirstChild(); navigable;
         navigable = navigable->NextSibling()) {
      //  7-1. navigable is not container’s content navigable.
      if (navigable == container_frame->ContentFrame()) {
        continue;
      }
      // 7-3. topLevelRelatives does not contain navigable.
      if (top_level_relatives.find(navigable) != top_level_relatives.end()) {
        continue;
      }
      // 7-4. navigable’s navigable container’s deferred fetch policy is
      // "deferred-fetch-minimal".
      auto* navigable_container = navigable->Owner();
      if (navigable_container &&
          navigable_container->GetFramePolicy().deferred_fetch_policy ==
              FramePolicy::DeferredFetchPolicy::kDeferredFetchMinimal) {
        count++;
      }
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

HeapHashSet<Member<Frame>> GetDeferredFetchQuotaSharingFrames(Frame* frame) {
  HeapHashSet<Member<Frame>> result;
  if (!frame) {
    return result;
  }

  auto* top_frame = frame->Top();
  for (auto* current_frame = top_frame; current_frame;
       current_frame = current_frame->Tree().TraverseNext(top_frame)) {
    if (!current_frame->IsLocalFrame()) {
      // Skips non-local frames.
      continue;
    }
    if (!frame->GetSecurityContext()->GetSecurityOrigin()->CanAccess(
            current_frame->GetSecurityContext()->GetSecurityOrigin())) {
      // Skips cross-origin frames.
      continue;
    }
    result.insert(current_frame);
  }

  return result;
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
  // 1. Set container’s deferred fetch policy to disabled.

  // 2. If the inherited policy for "deferred-fetch", container and
  // originToNavigateTo is Enabled,
  // TODO(crbug.com/40276121): and the available deferred fetching quota for
  // container’s container document is equal or greater than 64 kibibytes,
  if (permissions_policy->IsFeatureEnabledForOrigin(
          mojom::blink::PermissionsPolicyFeature::kDeferredFetch,
          to_url_origin)) {
    // then set container’s deferred fetch policy to "deferred-fetch" and
    // return.
    return FramePolicy::DeferredFetchPolicy::kDeferredFetch;
  }
  // 3. If the inherited policy for "deferred-fetch-minimal", container and
  // originToNavigateTo is Disabled, then set container’s deferred fetch policy
  // to disabled and return.
  if (!permissions_policy->IsFeatureEnabledForOrigin(
          mojom::blink::PermissionsPolicyFeature::kDeferredFetchMinimal,
          to_url_origin)) {
    return FramePolicy::DeferredFetchPolicy::kDisabled;
  }

  // 4. Let topLevelRelatives be container’s container document’s deferred
  // fetch quota-sharing navigables.
  auto top_level_relatives = GetDeferredFetchQuotaSharingFrames(
      container_frame->ContentFrame()->Parent());
  // 5. If topLevelRelatives does not contain container’s node navigable’s
  // top-level traversable, then set container’s deferred fetch policy to
  // disabled and return.
  if (top_level_relatives.find(container_frame->ContentFrame()->Top()) ==
      top_level_relatives.end()) {
    return FramePolicy::DeferredFetchPolicy::kDisabled;
  }

  // 7. For each navigable that matches the following conditions:
  uint32_t frames_with_minimal_quota_policy =
      CountFramesWithMinimalQuotaPolicy(container_frame, top_level_relatives);

  // 8. If framesWithMinimalQuotaPolicy is less than 16, then set container’s
  // deferred fetch policy to "deferred-fetch-minimal".
  if (frames_with_minimal_quota_policy < 16) {
    return FramePolicy::DeferredFetchPolicy::kDeferredFetchMinimal;
  }

  return FramePolicy::DeferredFetchPolicy::kDisabled;
}

uint32_t CountFramesWithMinimalQuotaPolicyForTesting(
    FrameOwner* container_frame,
    const HeapHashSet<Member<Frame>>& top_level_relatives) {
  return CountFramesWithMinimalQuotaPolicy(container_frame,
                                           top_level_relatives);
}

}  // namespace blink
