// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/origin_policy_throttle.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/origin_policy_error_reason.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/origin.h"

namespace {
// Constants derived from the spec, https://github.com/WICG/origin-policy
static const char* kDefaultPolicy = "1";
static const char* kDeletePolicy = "0";
static const char* kWellKnown = "/.well-known/origin-policy/";

// Maximum policy size (implementation-defined limit in bytes).
// (Limit copied from network::SimpleURLLoader::kMaxBoundedStringDownloadSize.)
static const size_t kMaxPolicySize = 1024 * 1024;
}  // namespace

namespace content {

// static
bool OriginPolicyThrottle::ShouldRequestOriginPolicy(
    const GURL& url,
    std::string* request_version) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool origin_policy_enabled =
      base::FeatureList::IsEnabled(features::kOriginPolicy) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures);
  if (!origin_policy_enabled)
    return false;

  if (!url.SchemeIs(url::kHttpsScheme))
    return false;

  if (request_version) {
    const KnownVersionMap& versions = GetKnownVersions();
    const auto iter = versions.find(url::Origin::Create(url));
    *request_version =
        iter == versions.end() ? std::string(kDefaultPolicy) : iter->second;
  }
  return true;
}

// static
std::unique_ptr<NavigationThrottle>
OriginPolicyThrottle::MaybeCreateThrottleFor(NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(handle);

  // We use presence of the origin policy request header to determine
  // whether we should create the throttle.
  if (!handle->GetRequestHeaders().HasHeader(
          net::HttpRequestHeaders::kSecOriginPolicy))
    return nullptr;

  // TODO(vogelheim): Rewrite & hoist up this DCHECK to ensure that ..HasHeader
  //     and ShouldRequestOriginPolicy are always equal on entry to the method.
  //     This depends on https://crbug.com/881234 being fixed.
  DCHECK(OriginPolicyThrottle::ShouldRequestOriginPolicy(handle->GetURL(),
                                                         nullptr));
  return base::WrapUnique(new OriginPolicyThrottle(handle));
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

  // Per spec, Origin Policies are only fetched for https:-requests. So we
  // should always have HTTP headers at this point.
  // However, some unit tests generate responses without headers, so we still
  // need to check.
  if (!navigation_handle()->GetResponseHeaders())
    return NavigationThrottle::PROCEED;

  // This determines whether and which policy version applies and fetches it.
  //
  // Inputs are the kSecOriginPolicy HTTP header, and the version
  // we've last seen from this particular origin.
  //
  // - header with kDeletePolicy received: No policy applies, and delete the
  //       last-known policy for this origin.
  // - header received: Use header version and update last-known policy.
  // - no header received, last-known version exists: Use last-known version
  // - no header, no last-known version: No policy applies.

  std::string response_version;
  bool header_found =
      navigation_handle()->GetResponseHeaders()->GetNormalizedHeader(
          net::HttpRequestHeaders::kSecOriginPolicy, &response_version);

  url::Origin origin = GetRequestOrigin();
  DCHECK(!origin.Serialize().empty());
  DCHECK(!origin.opaque());
  KnownVersionMap& versions = GetKnownVersions();
  auto iter = versions.find(origin);

  // Process policy deletion first!
  if (header_found && response_version == kDeletePolicy) {
    if (iter != versions.end())
      versions.erase(iter);
    return NavigationThrottle::PROCEED;
  }

  // No policy applies to this request?
  if (!header_found && iter == versions.end()) {
    return NavigationThrottle::PROCEED;
  }

  if (!header_found)
    response_version = iter->second;
  else if (iter == versions.end())
    versions.insert(std::make_pair(origin, response_version));
  else
    iter->second = response_version;

  GURL policy = GURL(origin.Serialize() + kWellKnown + response_version);
  FetchCallback done =
      base::BindOnce(&OriginPolicyThrottle::OnTheGloriousPolicyHasArrived,
                     base::Unretained(this));
  RedirectCallback redirect = base::BindRepeating(
      &OriginPolicyThrottle::OnRedirect, base::Unretained(this));
  FetchPolicy(policy, std::move(done), std::move(redirect));
  return NavigationThrottle::DEFER;
}

const char* OriginPolicyThrottle::GetNameForLogging() {
  return "OriginPolicyThrottle";
}

// static
OriginPolicyThrottle::KnownVersionMap&
OriginPolicyThrottle::GetKnownVersionsForTesting() {
  return GetKnownVersions();
}

OriginPolicyThrottle::OriginPolicyThrottle(NavigationHandle* handle)
    : NavigationThrottle(handle) {}

OriginPolicyThrottle::KnownVersionMap&
OriginPolicyThrottle::GetKnownVersions() {
  static base::NoDestructor<KnownVersionMap> map_instance;
  return *map_instance;
}

const url::Origin OriginPolicyThrottle::GetRequestOrigin() {
  return url::Origin::Create(navigation_handle()->GetURL());
}

void OriginPolicyThrottle::FetchPolicy(const GURL& url,
                                       FetchCallback done,
                                       RedirectCallback redirect) {
  // Create the traffic annotation
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("origin_policy_loader", R"(
        semantics {
          sender: "Origin Policy URL Loader Throttle"
          description:
            "Fetches the Origin Policy with a given version from an origin."
          trigger:
            "In case the Origin Policy with a given version does not "
            "exist in the cache, it is fetched from the origin at a "
            "well-known location."
          data:
            "None, the URL itself contains the origin and Origin Policy "
            "version."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings. Server "
            "opt-in or out of this mechanism."
          policy_exception_justification:
            "Not implemented, considered not useful."})");

  // Create and configure the SimpleURLLoader for the policy.
  std::unique_ptr<network::ResourceRequest> policy_request =
      std::make_unique<network::ResourceRequest>();
  policy_request->url = url;
  policy_request->request_initiator = url::Origin::Create(url);
  policy_request->load_flags = net::LOAD_DO_NOT_SEND_COOKIES |
                               net::LOAD_DO_NOT_SAVE_COOKIES |
                               net::LOAD_DO_NOT_SEND_AUTH_DATA;
  url_loader_ = network::SimpleURLLoader::Create(std::move(policy_request),
                                                 traffic_annotation);
  url_loader_->SetOnRedirectCallback(std::move(redirect));

  // Obtain the URLLoaderFactory from the NavigationHandle.
  SiteInstance* site_instance = navigation_handle()->GetStartingSiteInstance();
  StoragePartition* storage_partition = BrowserContext::GetStoragePartition(
      site_instance->GetBrowserContext(), site_instance);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      storage_partition->GetURLLoaderFactoryForBrowserProcess();

  // Start the download, and pass the callback for when we're finished.
  url_loader_->DownloadToString(url_loader_factory.get(), std::move(done),
                                kMaxPolicySize);
}

void OriginPolicyThrottle::InjectPolicyForTesting(
    const std::string& policy_content) {
  OnTheGloriousPolicyHasArrived(std::make_unique<std::string>(policy_content));
}

void OriginPolicyThrottle::OnTheGloriousPolicyHasArrived(
    std::unique_ptr<std::string> policy_content) {
  // Release resources associated with the loading.
  url_loader_.reset();

  // Fail hard if the policy could not be loaded.
  if (!policy_content) {
    CancelNavigation(OriginPolicyErrorReason::kCannotLoadPolicy);
    return;
  }

  // TODO(vogelheim): Determine whether we need to parse or sanity check
  //                  the policy content at this point.

  static_cast<NavigationHandleImpl*>(navigation_handle())
      ->set_origin_policy(*policy_content);
  Resume();
}

void OriginPolicyThrottle::OnRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head,
    std::vector<std::string>* to_be_removed_headers) {
  // Fail hard if the policy response follows a redirect.
  url_loader_.reset();  // Cancel the request while it's ongoing.
  CancelNavigation(OriginPolicyErrorReason::kPolicyShouldNotRedirect);
}

void OriginPolicyThrottle::CancelNavigation(OriginPolicyErrorReason reason) {
  base::Optional<std::string> error_page =
      GetContentClient()->browser()->GetOriginPolicyErrorPage(
          reason, GetRequestOrigin(), navigation_handle()->GetURL());
  CancelDeferredNavigation(NavigationThrottle::ThrottleCheckResult(
      NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_CLIENT, error_page));
}

}  // namespace content
