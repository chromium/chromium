// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/origin_policy_throttle.h"
#include "content/public/browser/origin_policy_commands.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/mojom/origin_policy_manager.mojom.h"
#include "url/origin.h"

namespace content {

// Implement the public "API" from
// content/public/browser/origin_policy_commands.h:
void OriginPolicyAddExceptionFor(StoragePartition* storage_partition,
                                 const GURL& url) {
  OriginPolicyThrottle::AddExceptionFor(storage_partition, url);
}

// static
bool OriginPolicyThrottle::ShouldRequestOriginPolicy(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!base::FeatureList::IsEnabled(features::kOriginPolicy))
    return false;

  if (!url.SchemeIs(url::kHttpsScheme))
    return false;

  return true;
}

// static
std::unique_ptr<NavigationThrottle>
OriginPolicyThrottle::MaybeCreateThrottleFor(NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(handle);

  if (!ShouldRequestOriginPolicy(handle->GetURL()))
    return nullptr;

  return base::WrapUnique(new OriginPolicyThrottle(handle));
}

// static
void OriginPolicyThrottle::AddExceptionFor(StoragePartition* storage_partition,
                                           const GURL& url) {
  auto* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(storage_partition);
  network::mojom::OriginPolicyManager* origin_policy_manager =
      storage_partition_impl->GetOriginPolicyManagerForBrowserProcess();

  origin_policy_manager->AddExceptionFor(url::Origin::Create(url));
}

OriginPolicyThrottle::~OriginPolicyThrottle() {}

NavigationThrottle::ThrottleCheckResult
OriginPolicyThrottle::WillStartRequest() {
  // TODO(vogelheim): It might be faster in the common case to optimistically
  //     fetch the policy indicated in the request already here. This would
  //     be faster if the last known version is the current version, but
  //     slower (and wasteful of bandwidth) if the server sends us a new
  //     version. It's unclear how much the upside is, though.
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
OriginPolicyThrottle::WillProcessResponse() {
  DCHECK(navigation_handle());

  // If no test origin policy is set, look at the actual origin policy from the
  // response.
  const absl::optional<network::OriginPolicy>& origin_policy =
      GetTestOriginPolicy().has_value()
          ? GetTestOriginPolicy()
          : NavigationRequest::From(navigation_handle())
                ->response()
                ->origin_policy;

  // If there is no origin_policy, treat this case as
  // network::OriginPolicyState::kNoPolicyApplies.
  if (!origin_policy.has_value()) {
    return NavigationThrottle::PROCEED;
  }

  switch (origin_policy->state) {
    case network::OriginPolicyState::kCannotLoadPolicy:
    case network::OriginPolicyState::kCannotParseHeader:
      // Don't show an interstitial for iframes or non-primary pages;
      // Origin-Policy errors in iframes should be treated as normal network
      // errors.
      if (!navigation_handle()->IsInPrimaryMainFrame()) {
        return NavigationThrottle::ThrottleCheckResult(
            NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_CLIENT);
      }
      return NavigationThrottle::ThrottleCheckResult(
          NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_CLIENT,
          GetContentClient()->browser()->GetOriginPolicyErrorPage(
              origin_policy->state, navigation_handle()));

    case network::OriginPolicyState::kNoPolicyApplies:
    case network::OriginPolicyState::kLoaded:
      return NavigationThrottle::PROCEED;
  }
}

const char* OriginPolicyThrottle::GetNameForLogging() {
  return "OriginPolicyThrottle";
}

// static
void OriginPolicyThrottle::SetOriginPolicyForTesting(
    const network::OriginPolicy& origin_policy) {
  absl::optional<network::OriginPolicy> new_test_origin_policy = origin_policy;
  GetTestOriginPolicy().swap(new_test_origin_policy);
}

// static
void OriginPolicyThrottle::ResetOriginPolicyForTesting() {
  GetTestOriginPolicy().reset();
}

OriginPolicyThrottle::OriginPolicyThrottle(NavigationHandle* handle)
    : NavigationThrottle(handle) {}

// static
absl::optional<network::OriginPolicy>&
OriginPolicyThrottle::GetTestOriginPolicy() {
  static base::NoDestructor<absl::optional<network::OriginPolicy>>
      test_origin_policy;
  return *test_origin_policy;
}

}  // namespace content
