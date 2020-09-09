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
void OriginPolicyAddExceptionFor(BrowserContext* browser_context,
                                 const GURL& url) {
  OriginPolicyThrottle::AddExceptionFor(browser_context, url);
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
void OriginPolicyThrottle::AddExceptionFor(BrowserContext* browser_context,
                                           const GURL& url) {
  DCHECK(browser_context);
  StoragePartitionImpl* storage_partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetStoragePartitionForSite(browser_context, url));
  network::mojom::OriginPolicyManager* origin_policy_manager =
      storage_partition->GetOriginPolicyManagerForBrowserProcess();

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
  const base::Optional<network::OriginPolicy>& origin_policy =
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
  base::Optional<network::OriginPolicy> new_test_origin_policy = origin_policy;
  GetTestOriginPolicy().swap(new_test_origin_policy);
}

// static
void OriginPolicyThrottle::ResetOriginPolicyForTesting() {
  GetTestOriginPolicy().reset();
}

OriginPolicyThrottle::OriginPolicyThrottle(NavigationHandle* handle)
    : NavigationThrottle(handle) {}

// static
base::Optional<network::OriginPolicy>&
OriginPolicyThrottle::GetTestOriginPolicy() {
  static base::NoDestructor<base::Optional<network::OriginPolicy>>
      test_origin_policy;
  return *test_origin_policy;
}

}  // namespace content
