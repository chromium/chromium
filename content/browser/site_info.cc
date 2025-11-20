// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_info.h"

#include <algorithm>
#include <optional>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/safe_ref.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/origin_agent_cluster_isolation_state.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/security/coop/cross_origin_isolation_mode.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/common/features.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/process_selection_user_data.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_exposed_isolation_level.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

using WebUIDomains = std::vector<std::string>;

// Parses the TLD and any lower level domains for WebUI URLs of the form
// chrome://foo.bar/. Domains are returned in the same order they appear in the
// host.
WebUIDomains GetWebUIDomains(const GURL& url) {
  return base::SplitString(url.host(), ".", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_ALL);
}

// Checks if the `url` is a special case WebUI URL of the form
// chrome://foo.bar/. Such URLs will employ LockURLs based on their TLD (ie
// chome://bar/). This will allow WebUI URLs of the above form with common TLDs
// to share a process whilst maintaining independent SiteURLs to allow for
// WebUIType differentiation.
bool IsWebUIAndUsesTLDForProcessLockURL(const GURL& url) {
  if (!base::Contains(URLDataManagerBackend::GetWebUISchemes(), url.scheme())) {
    return false;
  }

  WebUIDomains domains = GetWebUIDomains(url);
  // This only applies to WebUI urls with two or more non-empty domains.
  return domains.size() >= 2 &&
         std::ranges::all_of(domains, [](const std::string& domain) {
           return !domain.empty();
         });
}

// URL used for the site URL and lock URL in error page SiteInfo objects.
GURL GetErrorPageSiteAndLockURL() {
  return GURL(kUnreachableWebDataURL);
}

GURL SchemeAndHostToSite(const std::string& scheme, const std::string& host) {
  return GURL(scheme + url::kStandardSchemeSeparator + host);
}

// Figure out which origin to use for computing site and process lock URLs for
// `url`. In most cases, this should just be `url`'s origin. However, there are
// some exceptions where an alternate origin must be used.
//   - data: URLs: The tentative origin to commit, stored in `overridden_origin`
//     should be used. We store the value there because it's an opaque origin
//     and this lets us use the same nonce throughout the navigation.
//   - LoadDataWithBaseURL: The origin of the base URL should be used, rather
//     than the data URL.
//   - about:blank: The initiator's origin should be inherited.
// In all these cases, we should use the alternate origin which will be passed
// through `overridden_origin`, ensuring to use its precursor in the about:blank
// case if the origin is opaque to still compute a meaningful site URL.
url::Origin GetPossiblyOverriddenOriginFromUrl(
    const GURL& url,
    std::optional<url::Origin> overridden_origin) {
  bool scheme_allows_origin_override =
      url.SchemeIs(url::kDataScheme) || url.IsAboutBlank();
  if (overridden_origin.has_value() && scheme_allows_origin_override) {
    auto precursor = overridden_origin->GetTupleOrPrecursorTupleIfOpaque();
    if (url.SchemeIs(url::kDataScheme)) {
      // data: URLs have an overridden origin so they can have the same nonce
      // over the course of a navigation.
      // This is checked first, since we don't want to use the precursor for
      // most data: URLs. For regular data: URLs, we should use the
      // overridden_origin value, not the precursor. Sandboxed data: URLs are an
      // exception and should use the precursor.
      // In the LoadDataWithBaseURL case, the base URL which is a real,
      // non-opaque origin is used, and should also not use the precursor. We
      // don't expect LoadDataWithBaseURL to have an opaque origin with a
      // precursor in any case. If there is no base URL, then it should be
      // treated as a regular data: URL.
      return overridden_origin.value();
    } else if (precursor.IsValid()) {
      // The precursor should only be used in the about:blank case.
      return url::Origin::CreateFromNormalizedTuple(
          precursor.scheme(), precursor.host(), precursor.port());
    } else {
      return url::Origin::Resolve(url, overridden_origin.value());
    }
  } else {
    return url::Origin::Create(url);
  }
}

// Returns true if `url_info` is sandboxed, and per-origin mode of
// kIsolateSandboxedIframes is active. This is a helper function for
// GetSiteForURLInternal() and CreateInternal().
bool IsOriginIsolatedSandboxedFrame(const UrlInfo& url_info) {
  return url_info.is_sandboxed &&
         blink::features::kIsolateSandboxedIframesGroupingParam.Get() ==
             blink::features::IsolateSandboxedIframesGrouping::kPerOrigin;
}

// Computes whether to disable v8-optimization for the
// (browsing_instance_id, process_lock_origin) pair.
bool CheckShouldDisableV8Optimization(
    BrowserContext* browser_context,
    const BrowsingInstanceId& browsing_instance_id,
    const std::optional<base::SafeRef<ProcessSelectionUserData>>&
        process_selection_user_data,
    const GURL& process_lock_url) {
  std::optional<bool> are_v8_optimizations_disabled_result =
      ChildProcessSecurityPolicyImpl::GetInstance()
          ->LookupAreV8OptimizationsDisabled(
              browsing_instance_id, url::Origin::Create(process_lock_url));
  if (are_v8_optimizations_disabled_result.has_value()) {
    return are_v8_optimizations_disabled_result.value();
  }

  return !GetContentClient()->browser()->AreV8OptimizationsEnabledForSite(
      browser_context, process_selection_user_data, process_lock_url);
}

}  // namespace

// static
SiteInfo SiteInfo::CreateForErrorPage(
    const StoragePartitionConfig storage_partition_config,
    bool is_guest,
    bool is_fenced,
    const WebExposedIsolationInfo& web_exposed_isolation_info,
    WebExposedIsolationLevel web_exposed_isolation_level,
    const std::optional<AgentClusterKey::CrossOriginIsolationKey>&
        cross_origin_isolation_key) {
  AgentClusterKey agent_cluster_key;
  if (cross_origin_isolation_key.has_value()) {
    agent_cluster_key = AgentClusterKey::CreateWithCrossOriginIsolationKey(
        url::Origin::Create(GetErrorPageSiteAndLockURL()),
        cross_origin_isolation_key.value(),
        AgentClusterKey::OACStatus::kSiteKeyedByDefault);
  } else {
    agent_cluster_key = AgentClusterKey::CreateSiteKeyed(
        GetErrorPageSiteAndLockURL(),
        AgentClusterKey::OACStatus::kSiteKeyedByDefault);
  }
  return SiteInfo(
      agent_cluster_key, GetErrorPageSiteAndLockURL() /* site_url */,
      false /* is_sandboxed */, UrlInfo::kInvalidUniqueSandboxId,
      storage_partition_config, web_exposed_isolation_info,
      web_exposed_isolation_level, is_guest,
      false /* does_site_request_dedicated_process_for_coop */,
      false /* is_jit_disabled */, false /* are_v8_optimizations_disabled */,
      false /* is_pdf */, is_fenced);
}

// static
SiteInfo SiteInfo::CreateForDefaultSiteInstance(
    const IsolationContext& isolation_context,
    const StoragePartitionConfig storage_partition_config,
    const WebExposedIsolationInfo& web_exposed_isolation_info,
    const std::optional<AgentClusterKey::CrossOriginIsolationKey>&
        cross_origin_isolation_key) {
  // Get default JIT policy for this browser_context by passing in an empty
  // site_url.
  BrowserContext* browser_context = isolation_context.browser_context();
  bool is_jit_disabled = GetContentClient()->browser()->IsJitDisabledForSite(
      browser_context, GURL());
  bool are_v8_optimizations_disabled = CheckShouldDisableV8Optimization(
      browser_context, isolation_context.browsing_instance_id(), std::nullopt,
      GURL());

  WebExposedIsolationLevel web_exposed_isolation_level =
      SiteInfo::ComputeWebExposedIsolationLevelForEmptySite(
          web_exposed_isolation_info);

  AgentClusterKey agent_cluster_key;
  if (cross_origin_isolation_key.has_value()) {
    agent_cluster_key = AgentClusterKey::CreateWithCrossOriginIsolationKey(
        url::Origin::Create(SiteInstanceImpl::GetDefaultSiteURL()),
        cross_origin_isolation_key.value(),
        AgentClusterKey::OACStatus::kSiteKeyedByDefault);
  } else {
    agent_cluster_key = AgentClusterKey::CreateSiteKeyed(
        SiteInstanceImpl::GetDefaultSiteURL(),
        AgentClusterKey::OACStatus::kSiteKeyedByDefault);
  }
  return SiteInfo(agent_cluster_key,
                  /*site_url=*/SiteInstanceImpl::GetDefaultSiteURL(),
                  /*is_sandboxed=*/false, UrlInfo::kInvalidUniqueSandboxId,
                  storage_partition_config, web_exposed_isolation_info,
                  web_exposed_isolation_level, isolation_context.is_guest(),
                  /*does_site_request_dedicated_process_for_coop=*/false,
                  is_jit_disabled, are_v8_optimizations_disabled,
                  /*is_pdf=*/false, isolation_context.is_fenced());
}

// static
SiteInfo SiteInfo::CreateForGuest(
    BrowserContext* browser_context,
    const StoragePartitionConfig& partition_config) {
  // Guests use regular site and lock URLs, and their StoragePartition
  // information is maintained in a separate SiteInfo field.  Since this
  // function is called when a guest SiteInstance is first created (prior to
  // any navigations), there is no URL at this point to compute proper site and
  // lock URLs, so leave them empty for now.  Future navigations (if any) in
  // the guest will follow the normal process selection paths and use
  // SiteInstances with real site and lock URLs.
  return SiteInfo(
      AgentClusterKey(),
      /*site_url=*/GURL(),
      /*is_sandboxed=*/false, UrlInfo::kInvalidUniqueSandboxId,
      partition_config, WebExposedIsolationInfo::CreateNonIsolated(),
      WebExposedIsolationLevel::kNotIsolated,
      /*is_guest=*/true,
      /*does_site_request_dedicated_process_for_coop=*/false,
      /*is_jit_disabled=*/false, /*are_v8_optimizations_disabled=*/false,
      /*is_pdf=*/false, /*is_fenced=*/false);
}

// static
SiteInfo SiteInfo::Create(const IsolationContext& isolation_context,
                          const UrlInfo& url_info) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(url_info.is_sandboxed ||
         url_info.unique_sandbox_id == UrlInfo::kInvalidUniqueSandboxId);
  AgentClusterKey agent_cluster_key =
      GetAgentClusterKeyForURL(isolation_context, url_info,
                               /*effective_url=*/std::nullopt);
  GURL site_url = agent_cluster_key.GetURL();

  // PDF content should live in JIT-less processes because it is inherently less
  // trusted.
  bool is_jitless = url_info.is_pdf;
  bool are_v8_optimizations_disabled = false;

  std::optional<StoragePartitionConfig> storage_partition_config =
      url_info.storage_partition_config;

  std::optional<GURL> effective_url =
      GetContentClient()->browser()->GetEffectiveURL(
          isolation_context.browser_context(), url_info.url);

  // In the case of WebUIs, pass the real URL as an effective URL. It will be
  // used to compute a SiteInfo's site URL which is the complete WebUI URL,
  // while the agent_cluster_key_ has a site URL which is the WebUI TLD. This
  // allows WebUI to continue to differentiate WebUIType via site URL while
  // allowing WebUI with a shared TLD to share a RenderProcessHost.
  // TODO(crbug.com/40176090): Remove this and replace it with
  // SiteInstanceGroups once the support lands.
  if (IsWebUIAndUsesTLDForProcessLockURL(url_info.url)) {
    CHECK(!effective_url.has_value());
    effective_url = url_info.url;
  }

  // If there is an effective URL, compute the effective site URL and override
  // the site_url computed from the AgentClusterKey.
  if (effective_url.has_value()) {
    site_url =
        GetAgentClusterKeyForURL(isolation_context, url_info, effective_url)
            .GetURL();
  }

  BrowserContext* browser_context = isolation_context.browser_context();

  // If the SiteInfo is for a site that does not require a dedicated process
  // (and will end up in the default SiteInstanceGroup), then we should use
  // the default JITless and V8 optimization values. Passing an empty URL into
  // the corresponding ContentBrowserClient functions returns the default
  // JITless/V8 values for the embedder.
  GURL agent_cluster_url_or_default =
      ShouldUseDefaultSiteInstanceGroup() &&
              !RequiresDedicatedProcessInternal(
                  site_url, isolation_context, browser_context,
                  url_info.requests_coop_isolation(),
                  !url_info.oac_header_request.has_value(),
                  url_info.is_sandboxed, url_info.is_pdf)
          ? GURL()
          : agent_cluster_key.GetURL();
  is_jitless =
      is_jitless || GetContentClient()->browser()->IsJitDisabledForSite(
                        browser_context, agent_cluster_url_or_default);
  are_v8_optimizations_disabled = CheckShouldDisableV8Optimization(
      browser_context, isolation_context.browsing_instance_id(),
      url_info.process_selection_user_data, agent_cluster_url_or_default);

  if (!storage_partition_config.has_value()) {
    storage_partition_config =
        GetStoragePartitionConfigForUrl(browser_context, site_url);
  }
  DCHECK(storage_partition_config.has_value());

  // Note: Well-formed UrlInfos can arrive here with null
  // WebExposedIsolationInfo. One example is, going through the process model
  // prior to having received response headers that determine the final
  // WebExposedIsolationInfo, and creating a new speculative SiteInstance. In
  // these cases we consider the SiteInfo to be non-isolated.
  //
  // Sometimes SiteInfos are built from UrlInfos for the purpose of using
  // SiteInfo comparisons. Sometimes we only want to compare some attributes and
  // do not care about WebExposedIsolationInfo. These cases should not rely on
  // the default WebExposedIsolationInfo value. Callers should specify why it is
  // appropriate to disregard WebExposedIsolationInfo and override it manually
  // to what they expect the other value to be.
  WebExposedIsolationInfo web_exposed_isolation_info =
      url_info.web_exposed_isolation_info.value_or(
          WebExposedIsolationInfo::CreateNonIsolated());
  WebExposedIsolationLevel web_exposed_isolation_level =
      ComputeWebExposedIsolationLevel(web_exposed_isolation_info, url_info);

  if (url_info.url.SchemeIs(kChromeErrorScheme)) {
    return CreateForErrorPage(
        storage_partition_config.value(),
        /*is_guest=*/isolation_context.is_guest(),
        /*is_fenced=*/isolation_context.is_fenced(), web_exposed_isolation_info,
        web_exposed_isolation_level, url_info.cross_origin_isolation_key);
  }

  // If there is a COOP isolation request, propagate it to SiteInfo.
  // This will be used later when determining a suitable SiteInstance
  // and BrowsingInstance for this SiteInfo.
  bool does_site_request_dedicated_process_for_coop =
      url_info.requests_coop_isolation();

  return SiteInfo(agent_cluster_key, site_url, url_info.is_sandboxed,
                  url_info.unique_sandbox_id, storage_partition_config.value(),
                  web_exposed_isolation_info, web_exposed_isolation_level,
                  isolation_context.is_guest(),
                  does_site_request_dedicated_process_for_coop, is_jitless,
                  are_v8_optimizations_disabled, url_info.is_pdf,
                  isolation_context.is_fenced());
}

// static
SiteInfo SiteInfo::CreateForTesting(const IsolationContext& isolation_context,
                                    const GURL& url) {
  return Create(isolation_context, UrlInfo::CreateForTesting(url));
}

SiteInfo::SiteInfo(const AgentClusterKey& agent_cluster_key,
                   const GURL& site_url,
                   bool is_sandboxed,
                   int unique_sandbox_id,
                   const StoragePartitionConfig storage_partition_config,
                   const WebExposedIsolationInfo& web_exposed_isolation_info,
                   WebExposedIsolationLevel web_exposed_isolation_level,
                   bool is_guest,
                   bool does_site_request_dedicated_process_for_coop,
                   bool is_jit_disabled,
                   bool are_v8_optimizations_disabled,
                   bool is_pdf,
                   bool is_fenced)
    : site_url_(site_url),
      agent_cluster_key_(agent_cluster_key),
      is_sandboxed_(is_sandboxed),
      unique_sandbox_id_(unique_sandbox_id),
      storage_partition_config_(storage_partition_config),
      web_exposed_isolation_info_(web_exposed_isolation_info),
      web_exposed_isolation_level_(web_exposed_isolation_level),
      is_guest_(is_guest),
      does_site_request_dedicated_process_for_coop_(
          does_site_request_dedicated_process_for_coop),
      is_jit_disabled_(is_jit_disabled),
      are_v8_optimizations_disabled_(are_v8_optimizations_disabled),
      is_pdf_(is_pdf),
      is_fenced_(is_fenced) {
  DCHECK(is_sandboxed_ ||
         unique_sandbox_id_ == UrlInfo::kInvalidUniqueSandboxId);
  DCHECK((oac_status() != AgentClusterKey::OACStatus::kOriginKeyedByHeader &&
          oac_status() != AgentClusterKey::OACStatus::kOriginKeyedByDefault) ||
         agent_cluster_key_.IsOriginKeyed());
}
SiteInfo::SiteInfo(const SiteInfo& rhs) = default;

SiteInfo::~SiteInfo() = default;

SiteInfo::SiteInfo(BrowserContext* browser_context)
    : SiteInfo(AgentClusterKey(),
               /*site_url=*/GURL(),
               /*is_sandboxed*/ false,
               UrlInfo::kInvalidUniqueSandboxId,
               StoragePartitionConfig::CreateDefault(browser_context),
               WebExposedIsolationInfo::CreateNonIsolated(),
               WebExposedIsolationLevel::kNotIsolated,
               /*is_guest=*/false,
               /*does_site_request_dedicated_process_for_coop=*/false,
               /*is_jit_disabled=*/false,
               /*are_v8_optimizations_disabled=*/false,
               /*is_pdf=*/false,
               /*is_fenced=*/false) {}

// static
auto SiteInfo::MakeSecurityPrincipalKey(const SiteInfo& site_info) {
  // Note: `does_site_request_dedicated_process_for_coop_` is intentionally
  // excluded here, as a difference solely in that field should not cause a
  // different SiteInstance to be created.  A document that has been
  // site-isolated due to COOP should still share a SiteInstance with other
  // same-site frames in the BrowsingInstance, even if those frames lack the
  // COOP isolation request.
  return std::tie(site_info.site_url_.possibly_invalid_spec(),
                  site_info.is_sandboxed_, site_info.unique_sandbox_id_,
                  site_info.storage_partition_config_,
                  site_info.web_exposed_isolation_info_,
                  site_info.web_exposed_isolation_level_, site_info.is_guest_,
                  site_info.is_jit_disabled_,
                  site_info.are_v8_optimizations_disabled_, site_info.is_pdf_,
                  site_info.is_fenced_, site_info.agent_cluster_key_);
}

SiteInfo SiteInfo::GetNonOriginKeyedEquivalentForMetrics(
    const IsolationContext& isolation_context) const {
  SiteInfo non_oac_site_info(*this);

  // Do not convert cross-origin isolated SiteInfos back into site keyed ones as
  // cross-origin isolated agent clusters are required by spec to be
  // origin-keyed, regardless of the Origin-Agent-Cluster header.
  if ((oac_status() == AgentClusterKey::OACStatus::kOriginKeyedByHeader ||
       oac_status() == AgentClusterKey::OACStatus::kOriginKeyedByDefault) &&
      !agent_cluster_key_.GetCrossOriginIsolationKey().has_value()) {
    CHECK(agent_cluster_key_.IsOriginKeyed());
    DCHECK(agent_cluster_key_.GetOrigin().scheme() == url::kHttpsScheme);

    // TODO(wjmaclean): It would probably be better if we just changed
    // SiteInstanceImpl::original_url_ to be SiteInfo::original_url_info_ and
    // use that to recreate the SiteInfo with origin keying turned off. But
    // that's a largish refactor in its own, since it would require making all
    // SiteInfo creation go through SiteInfo::CreateInternal.
    // We'll do the following for now and do the refactor separately.
    // The code below creates a simple non-origin-keyed equivalent for this
    // SiteInfo by (1) Converting the process lock to its equivalent by either
    // seeing if it has a command-line isolated-origin it should use, and if not
    // then just using GetSiteForOrigin to convert it, and (2) doing the same
    // for the SiteUrl, but only if the SiteUrl and ProcessLockUrl match
    // prior to the conversion, otherwise leave the SiteUrl as is.
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    url::Origin result_origin;
    // We need to make the following call with a 'null' IsolationContext,
    // otherwise the OAC history will just opt us back into an origin-keyed
    // SiteInfo.
    GURL process_lock_url;
    if (policy->GetMatchingProcessIsolatedOrigin(
            IsolationContext(
                BrowsingInstanceId(0), isolation_context.browser_context(),
                isolation_context.is_guest(), isolation_context.is_fenced(),
                isolation_context.default_isolation_state()),
            agent_cluster_key_.GetOrigin(),
            false /* origin_requests_isolation */, &result_origin)) {
      process_lock_url = result_origin.GetURL();
    } else {
      process_lock_url = GetSiteForOrigin(agent_cluster_key_.GetOrigin());
    }
    // Only convert the site_url_ if it matches the agent_cluster_key_,
    // otherwise leave it alone. This will only matter for hosted apps, and we
    // only expect them to differ if an effective URL is defined.
    if (site_url_ == agent_cluster_key_.GetOrigin().GetURL()) {
      non_oac_site_info.site_url_ = process_lock_url;
    }

    // Convert the AgentClusterKey from an origin-keyed one into a site-keyed
    // one.
    non_oac_site_info.agent_cluster_key_ = AgentClusterKey::CreateSiteKeyed(
        process_lock_url, AgentClusterKey::OACStatus::kSiteKeyedByDefault);
  }
  return non_oac_site_info;
}

GURL SiteInfo::GetProcessLockURL() const {
  return agent_cluster_key_.GetURL();
}

SiteInfo& SiteInfo::operator=(const SiteInfo& rhs) = default;

bool SiteInfo::IsSamePrincipalWith(const SiteInfo& other) const {
  return MakeSecurityPrincipalKey(*this) == MakeSecurityPrincipalKey(other);
}

bool SiteInfo::IsExactMatch(const SiteInfo& other) const {
  bool is_match =
      site_url_ == other.site_url_ && is_sandboxed_ == other.is_sandboxed_ &&
      unique_sandbox_id_ == other.unique_sandbox_id_ &&
      storage_partition_config_ == other.storage_partition_config_ &&
      web_exposed_isolation_info_ == other.web_exposed_isolation_info_ &&
      web_exposed_isolation_level_ == other.web_exposed_isolation_level_ &&
      is_guest_ == other.is_guest_ &&
      does_site_request_dedicated_process_for_coop_ ==
          other.does_site_request_dedicated_process_for_coop_ &&
      is_jit_disabled_ == other.is_jit_disabled_ &&
      are_v8_optimizations_disabled_ == other.are_v8_optimizations_disabled_ &&
      is_pdf_ == other.is_pdf_ && is_fenced_ == other.is_fenced_ &&
      agent_cluster_key_ == other.agent_cluster_key_;

  if (is_match) {
    // If all the fields match, then the "same principal" subset must also
    // match. This is used to ensure these 2 methods stay in sync and all fields
    // used by IsSamePrincipalWith() are used by this function.
    DCHECK(IsSamePrincipalWith(other));
  }
  return is_match;
}

auto SiteInfo::MakeProcessLockComparisonKey() const {
  // As we add additional features to SiteInfo, we'll expand this comparison.
  // Note that this should *not* compare site_url() values from the SiteInfo,
  // since those include effective URLs which may differ even if the actual
  // document origins match. We use agent_cluster_key() comparisons to account
  // for this.
  //
  // TODO(wjmaclean, alexmos): Figure out why including `is_jit_disabled_` here
  // leads to crashes in https://crbug.com/1279453.
  // TODO(ellyjones): Same as above, but about are_v8_optimizations_disabled_
  // (presumably).
  return std::tie(is_sandboxed_, unique_sandbox_id_, is_pdf_, is_guest_,
                  web_exposed_isolation_info_, web_exposed_isolation_level_,
                  storage_partition_config_, is_fenced_, agent_cluster_key_);
}

int SiteInfo::ProcessLockCompareTo(const SiteInfo& other) const {
  auto a = MakeProcessLockComparisonKey();
  auto b = other.MakeProcessLockComparisonKey();
  if (a < b) {
    return -1;
  }
  if (b < a) {
    return 1;
  }
  return 0;
}

bool SiteInfo::operator==(const SiteInfo& other) const {
  return IsSamePrincipalWith(other);
}

std::weak_ordering SiteInfo::operator<=>(const SiteInfo& other) const {
  return MakeSecurityPrincipalKey(*this) <=> MakeSecurityPrincipalKey(other);
}

std::string SiteInfo::GetDebugString() const {
  std::string debug_string =
      site_url_.is_empty() ? "empty site URL"
                           : ("site URL: " + site_url_.possibly_invalid_spec());

  if (agent_cluster_key_.IsOriginKeyed()) {
    if (agent_cluster_key_.GetOrigin() == GetOriginForUnlockedProcess()) {
      debug_string += " , empty lock";
    } else {
      debug_string +=
          ", locked to " + agent_cluster_key_.GetOrigin().GetDebugString();
    }
    debug_string += ", origin-keyed";
  } else {
    if (agent_cluster_key_.GetSite().is_empty()) {
      debug_string += " , empty lock";
    } else if (agent_cluster_key_.GetSite() != site_url_) {
      debug_string +=
          ", locked to " + agent_cluster_key_.GetSite().possibly_invalid_spec();
    }
    debug_string += ", site-keyed";
  }

  if (is_sandboxed_) {
    debug_string += ", sandboxed";
    if (unique_sandbox_id_ != UrlInfo::kInvalidUniqueSandboxId) {
      debug_string += base::StringPrintf(" (id=%d)", unique_sandbox_id_);
    }
  }

  if (web_exposed_isolation_info_.is_isolated()) {
    debug_string += ", cross-origin isolated";
    if (web_exposed_isolation_info_.is_isolated_application()) {
      debug_string += " application";
    }
    debug_string += ", coi-origin='" +
                    web_exposed_isolation_info_.origin().GetDebugString() + "'";
  }

  if (web_exposed_isolation_info_.is_isolated_application() &&
      web_exposed_isolation_level_ <
          WebExposedIsolationLevel::kIsolatedApplication) {
    debug_string += ", application isolation not inherited";
  }

  if (is_guest_) {
    debug_string += ", guest";
  }

  if (does_site_request_dedicated_process_for_coop_) {
    debug_string += ", requests coop isolation";
  }

  if (is_jit_disabled_) {
    debug_string += ", jitless";
  }

  if (are_v8_optimizations_disabled_) {
    debug_string += ", noopt";
  }

  if (is_pdf_) {
    debug_string += ", pdf";
  }

  if (!storage_partition_config_.is_default()) {
    debug_string +=
        ", partition=" + storage_partition_config_.partition_domain() + "." +
        storage_partition_config_.partition_name();
    if (storage_partition_config_.in_memory()) {
      debug_string += ", in-memory";
    }
  }

  if (is_fenced_) {
    debug_string += ", is_fenced";
  }

  if (agent_cluster_key_.GetCrossOriginIsolationKey().has_value()) {
    debug_string += ", coi agent cluster origin=" +
                    agent_cluster_key_.GetCrossOriginIsolationKey()
                        ->common_coi_origin.GetDebugString();
    if (agent_cluster_key_.GetCrossOriginIsolationKey()
            ->cross_origin_isolation_mode ==
        CrossOriginIsolationMode::kConcrete) {
      debug_string += ", concrete coi";
    } else if (agent_cluster_key_.GetCrossOriginIsolationKey()
                   ->cross_origin_isolation_mode ==
               CrossOriginIsolationMode::kLogical) {
      debug_string += ", logical coi";
    }
  }

  return debug_string;
}

std::ostream& operator<<(std::ostream& out, const SiteInfo& site_info) {
  return out << site_info.GetDebugString();
}

bool SiteInfo::RequiresDedicatedProcess(
    const IsolationContext& isolation_context) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserContext* browser_context = isolation_context.browser_context();
  DCHECK(browser_context);
  return RequiresDedicatedProcessInternal(
      site_url_, isolation_context, browser_context,
      does_site_request_dedicated_process_for_coop_,
      agent_cluster_key_.IsOriginKeyed(), is_sandboxed_, is_pdf_);
}

bool SiteInfo::ShouldLockProcessToSite(
    const IsolationContext& isolation_context) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserContext* browser_context = isolation_context.browser_context();
  DCHECK(browser_context);

  // Don't lock to origin in --single-process mode, since this mode puts
  // cross-site pages into the same process.  Note that this also covers the
  // single-process mode in Android Webview.
  if (RenderProcessHost::run_renderer_in_process()) {
    return false;
  }

  if (!RequiresDedicatedProcess(isolation_context)) {
    return false;
  }

  // Most WebUI processes should be locked on all platforms.  The only exception
  // is NTP, handled via the separate callout to the embedder.
  const auto& webui_schemes = URLDataManagerBackend::GetWebUISchemes();
  if (base::Contains(webui_schemes, site_url_.scheme())) {
    return GetContentClient()->browser()->DoesWebUIUrlRequireProcessLock(
        site_url_);
  }

  // Allow the embedder to prevent process locking so that multiple sites
  // can share a process.
  if (!GetContentClient()->browser()->ShouldLockProcessToSite(browser_context,
                                                              site_url_)) {
    return false;
  }

  return true;
}

bool SiteInfo::ShouldUseProcessPerSite(BrowserContext* browser_context) const {
  // Returns true if we should use the process-per-site model.  This will be
  // the case if the --process-per-site switch is specified, or in
  // process-per-site-instance for particular sites (e.g., NTP). Note that
  // --single-process is handled in ShouldTryToUseExistingProcessHost.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kProcessPerSite)) {
    return true;
  }

  // Error pages should use process-per-site model, as it is useful to
  // consolidate them to minimize resource usage and there is no security
  // drawback to combining them all in the same process.
  if (is_error_page()) {
    return true;
  }

  // Otherwise let the content client decide, defaulting to false.
  return GetContentClient()->browser()->ShouldUseProcessPerSite(browser_context,
                                                                site_url_);
}

// static
StoragePartitionConfig SiteInfo::GetStoragePartitionConfigForUrl(
    BrowserContext* browser_context,
    const GURL& site_or_regular_url) {
  if (site_or_regular_url.is_empty()) {
    return StoragePartitionConfig::CreateDefault(browser_context);
  }

  return GetContentClient()->browser()->GetStoragePartitionConfigForSite(
      browser_context, site_or_regular_url);
}

void SiteInfo::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("site_url", site_url());
  dict.Add("agent_cluster_site_keyed", agent_cluster_key_.IsSiteKeyed());
  dict.Add("agent_cluster_site", agent_cluster_key_.IsSiteKeyed()
                                     ? agent_cluster_key_.GetSite()
                                     : GURL());
  dict.Add("agent_cluster_origin", agent_cluster_key_.IsOriginKeyed()
                                       ? agent_cluster_key_.GetOrigin()
                                       : url::Origin());
  dict.Add("is_sandboxed", is_sandboxed_);
  dict.Add("is_guest", is_guest_);
  dict.Add("is_fenced", is_fenced_);
}

bool SiteInfo::is_error_page() const {
  return site_url_ == GetErrorPageSiteAndLockURL();
}

// static
AgentClusterKey SiteInfo::GetAgentClusterKeyForURL(
    const IsolationContext& isolation_context,
    const UrlInfo& url_info,
    std::optional<GURL> effective_url) {
  GURL url = effective_url.value_or(url_info.url);

  // Explicitly map all chrome-error: URLs to a single URL so that they all
  // end up in a dedicated error process. Note that the AgentClusterKey returned
  // here is always site keyed. However, it will not be used because
  // SiteInfo::CreateInternal will trigger a call to
  // SiteInfo::CreateForErrorPage which will recompute the AgentClusterKey based
  // on the CrossOriginIsolationKey in the URLinfo.
  if (url.SchemeIs(kChromeErrorScheme)) {
    return AgentClusterKey::CreateSiteKeyed(
        GetErrorPageSiteAndLockURL(),
        AgentClusterKey::OACStatus::kSiteKeyedByDefault);
  }

  // For WebUI URLs of the form chrome://foo.bar/, return a site-keyed
  // AgentClusterKey with a URL based on the TLD (ie chrome://bar/) when
  // there is no effective URL passed. When an effective URL is passed, which
  // will be the case when computing the site URL for the WebUI's SiteInfo,
  // proceed with regular computations. This allows WebUI to continue to
  // differentiate WebUIType via SiteURL while allowing WebUI with a shared TLD
  // to share a RenderProcessHost.
  // TODO(crbug.com/40176090): Remove this and replace it with SiteInstance
  // groups once the support lands.
  if (IsWebUIAndUsesTLDForProcessLockURL(url_info.url) &&
      !effective_url.has_value()) {
    WebUIDomains host_domains = GetWebUIDomains(url_info.url);
    return AgentClusterKey::CreateSiteKeyed(
        GURL(url_info.url.GetScheme() + url::kStandardSchemeSeparator +
             host_domains.back()),
        AgentClusterKey::OACStatus::kSiteKeyedByDefault);
  }

  // Ideally, we should check that the origin we've received corresponds to a
  // data URL with an opaque origin when setting the following boolean
  // is_origin_isolated_sandboxed_data_iframe to true. However, doing so will
  // make the ChildProcessSecurity::CanAccessOrigin check called from
  // NavigationRequest::GetOriginForURLLoaderFactoryAfterResponse() fail.
  //
  // ChildProcessSecurity::CanAccessOrigin eventually calls
  // ChildProcessSecurityPolicyImpl::CanAccessMaybeOpaqueOrigin, but the latter
  // only takes a GURL as an argument and not an origin. So for a data URL with
  // an opaque origin, the GURL passed to the function is the precursor origin.
  // This GURL is passed to
  // ChildProcessSecurityPolicyImpl::PerformJailAndCitadelChecks. The function
  // then creates an expected ProcessLock based on this GURL, that is the
  // precursor URL for the data URL and not the actual data URL. However, it
  // does pass the correct sandbox flags and ids identifying it as an origin
  // isolated sandboxed data URL. If we restrict origin isolation to actual data
  // URLs in that case, we end up with an expected process lock that is
  // site-keyed (since it was created with the precursor URL aka a regular URL),
  // while the actual process lock is origin-keyed (since it is created with an
  // actual data URL). This causes the jail and citadel checks to fail. However,
  // if we only base ourselves on IsOriginIsolatedSandboxedFrame (which does not
  // check that we have a data URL), the expected ProcessLock from
  // PerformJailAndCitadelChecks will be identified as an origin isolated
  // sandboxed data iframe, and will be given an origin keyed AgentClusterKey as
  // expected.
  //
  // Note that we will set the AgentClusterKey origin/URL and the effective site
  // URL to the precursor origin of the data URL just below.
  bool is_origin_isolated_sandboxed_data_iframe =
      IsOriginIsolatedSandboxedFrame(url_info);

  // Sandboxed data: subframes should be in the process of their
  // precursor origin. Replace their URL and origin by the precursor URL and
  // precursor origin so that they are appropriately taken into account by the
  // OAC code below.
  url::Origin origin = GetPossiblyOverriddenOriginFromUrl(url, url_info.origin);
  if (is_origin_isolated_sandboxed_data_iframe && origin.opaque() &&
      url.SchemeIs(url::kDataScheme)) {
    DUMP_WILL_BE_CHECK(origin.GetTupleOrPrecursorTupleIfOpaque().IsValid());
    url = origin.GetTupleOrPrecursorTupleIfOpaque().GetURL();
    origin = url::Origin::Create(url);
  }

  // Compute what the OAC isolation state should be for the SiteInfo, starting
  // with the requested state stored in the UrlInfo (that is based on the OAC
  // headers for the navigation). If there were no OAC headers, there is no OAC
  // request in the URLInfo. In this case, we use the current default isolation
  // state for the IsolationContext. This default isolation state might differ
  // from what OriginAgentClusterIsolationState::CreateForDefaultIsolation
  // returns when enterprise policies dynamically modify whether origin
  // isolation by default is enabled or not. An existing BrowsingInstance's
  // default OAC isolation state is not updated to reflect the new policy and
  // will keep the old policy.
  OriginAgentClusterIsolationState oac_isolation_state =
      url_info.oac_header_request.value_or(
          isolation_context.default_isolation_state());

  // Now check if the requested isolation state should be overridden by an OAC
  // isolation state already stored for the BrowsingInstance. This happens when
  // the origin has already requested an opt-in or an opt-out for origin
  // isolation in a previous navigation in the BrowsingInstance.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (SiteIsolationPolicy::IsProcessIsolationForOriginAgentClusterEnabled()) {
    oac_isolation_state = policy->DetermineOriginAgentClusterIsolation(
        isolation_context, origin, oac_isolation_state);
  }

  // Here, we're only interested in OAC isolation when it results in actual
  // process isolation.
  // TODO(crbug.com/40176090): Once SiteInstanceGroups are fully implemented, we
  // should be able to give all OAC origins their own SiteInstance.
  AgentClusterKey::OACStatus oac_status =
      oac_isolation_state.process_isolation_oac_status();

  // Now compute the correct AgentClusterKey for the SiteInfo.

  // Cross-origin isolated contexts should always be given an origin-keyed
  // AgentClusterKey with a CrossOriginIsolationKey.
  if (url_info.cross_origin_isolation_key.has_value()) {
    return AgentClusterKey::CreateWithCrossOriginIsolationKey(
        origin, url_info.cross_origin_isolation_key.value(), oac_status);
  }

  bool requires_origin_keyed_process =
      oac_status == AgentClusterKey::OACStatus::kOriginKeyedByHeader ||
      oac_status == AgentClusterKey::OACStatus::kOriginKeyedByDefault;

  // We should only set |requires_origin_keyed_process| if we are actually
  // creating separate SiteInstances for OAC isolation. When we use site-keyed
  // processes for OAC, we don't do that at present.
  CHECK(!requires_origin_keyed_process ||
        SiteIsolationPolicy::IsProcessIsolationForOriginAgentClusterEnabled());

  if (requires_origin_keyed_process) {
    return AgentClusterKey::CreateOriginKeyed(origin, oac_status);
  }

  // If the url has a host, then determine the AgentClusterKey. Skip file URLs
  // to avoid a situation where site URL of file://localhost/ would mismatch
  // Blink's origin (which ignores the hostname in this case - see
  // https://crbug.com/776160).
  if (!origin.host().empty() && origin.scheme() != url::kFileScheme) {
    // For Strict Origin Isolation, use the full origin instead of site for all
    // HTTP/HTTPS URLs.
    // TODO(crbug.com/433443082): This should return an origin-keyed
    // AgentClusterKey instead of a site-keyed one.
    if (SiteIsolationPolicy::IsStrictOriginIsolationEnabled() &&
        origin.GetURL().SchemeIsHTTPOrHTTPS()) {
      return AgentClusterKey::CreateSiteKeyed(origin.GetURL(), oac_status);
    }

    // For isolated sandboxed iframes in per-origin mode we just return a
    // site-keyed AgentClusterKey with a site URL that is the origin, as we
    // should be using the full origin for the SiteInstance, but we don't need
    // to track the origin like we do for OriginAgentCluster. Note that the
    // origin is actually the precursor origin of the data URL, as it was
    // rewritten at the beginning of this function.
    // TODO(crbug.com/433443082): This should return an origin-keyed
    // AgentClusterKey instead of a site-keyed one.
    if (is_origin_isolated_sandboxed_data_iframe) {
      return AgentClusterKey::CreateSiteKeyed(origin.GetURL(), oac_status);
    }

    // Isolated origins should use the full origin as their site URL. For the
    // non-OAC isolated origins mechanism in
    // ChildProcessSecurityPolicy::AddFutureIsolatedOrigins(), a subdomain of an
    // isolated origin should also use that isolated origin's site URL.
    // Note: Here we check that the OAC status computed for the navigation is
    // false. It might be different from the state requested in the UrlInfo.
    // This is the case for example when a navigation requests OAC 1? in a
    // BrowsingInstance that already has a site-keyed SiteInfo for this origin.
    // In this case, the OAC request is not granted. There are similar cases
    // with OriginIsolationByDefault involving opt-outs, such as a regular
    // navigation in an origin that already opted out or the computation of the
    // expected process lock in
    // ChildProcessSecurityPolicyImpl::PerformJailAndCitadelChecks (which is
    // always created from an UrlInfo with default OAC status but picks up the
    // correct OAC status from the OAC status stored in the BrowsingInstance).
    // In any case, it is still safe to pass |requests_origin_keyed_process| =
    // false in GetMatchingProcessIsolatedOrigin because we have already
    // computed that the appropriate OAC status for this SiteInfo is site-keyed,
    // regardless of what the UrlInfo was asking for.
    CHECK(!requires_origin_keyed_process);
    url::Origin isolated_origin;
    GURL site_url = GetSiteForOrigin(origin);
    if (policy->GetMatchingProcessIsolatedOrigin(
            isolation_context, origin, /*requests_origin_keyed_process=*/false,
            site_url, &isolated_origin)) {
      return AgentClusterKey::CreateSiteKeyed(isolated_origin.GetURL(),
                                              oac_status);
    }

    // All other cases where the origin has a host should get a site-keyed
    // AgentClusterKey based on the Site URL for the origin.
    // TODO(crbug.com/342366372): Return an origin-keyed AgentClusterKey by
    // default once SiteInstanceGroup has shipped and different SiteInstances
    // can share the same process.
    return AgentClusterKey::CreateSiteKeyed(site_url, oac_status);
  }

  // If there is no host but there is a scheme, return a site-keyed
  // AgentClusterKey whose URL is the scheme. This is useful for cases like file
  // URLs.
  if (!origin.opaque()) {
    // Prefer to use the scheme of |origin| rather than |url|, to correctly
    // cover blob:file: and filesystem:file: URIs (see also
    // https://crbug.com/697111).
    DCHECK(!origin.scheme().empty());
    GURL site_url = GURL(origin.scheme() + ":");
    return AgentClusterKey::CreateSiteKeyed(site_url, oac_status);
  }

  // If the URL is invalid, return a site-keyed AgentClusterKey with an empty
  // URL.
  if (!url.has_scheme()) {
    DCHECK(!url.is_valid()) << url;
    return AgentClusterKey();
  }

  if (url.SchemeIs(url::kDataScheme)) {
    CHECK(!is_origin_isolated_sandboxed_data_iframe);
    // We get here for browser-initiated navigations to data URLs, as sandboxed
    // data iframes have their origin rewritten to their precursor origin before
    // determining their Origin-Agent-Cluster status, and should already have
    // been handled above.
    //
    // We do not want to use the complete data URL as a site URL, because it
    // contains the entire body of the data: URL. At the same time, this data
    // URL does not have a precursor origin because it's a browser-initiated
    // navigation to a data URL. So the only practical way to have a site URL in
    // this case is to serialize the nonce of the opaque origin provided to the
    // data URL. In addition, using the entire data URL as a site URL wouldn't
    // distinguish between two instances of the same data URL in two independent
    // tabs. However nonces do distinguish between these two instances. Since
    // each browser-initiated data: URL is given a different opaque origin with
    // a different nonce, this means each browser-initiated data: URL will get
    // its own process. See https://crbug.com/863069.
    // TODO(crbug.com/433443082): This should return an origin-keyed
    // AgentClusterKey instead of a site-keyed one.
    GURL site_url = GetOriginBasedSiteURLForDataURL(origin);
    return AgentClusterKey::CreateSiteKeyed(site_url, oac_status);
  }

  if (url.SchemeIsBlob()) {
    // In some cases, it is not safe to use just the scheme as a site URL,
    // as that might allow two URLs created by different sites to share a
    // process. See https://crbug.com/863623.
    //
    // TODO(alexmos,creis): This should eventually be expanded to certain
    // other schemes, such as file:.
    // We get here for blob URLs of form blob:null/guid.  Use the full URL
    // with the GUID in that case, which isolates all blob URLs with unique
    // origins from each other.  Remove hash from the URL since
    // same-document navigations shouldn't use a different site URL.
    GURL site_url = url;
    if (url.has_ref()) {
      GURL::Replacements replacements;
      replacements.ClearRef();
      site_url = url.ReplaceComponents(replacements);
    }
    return AgentClusterKey::CreateSiteKeyed(site_url, oac_status);
  }

  // All other URLs use a site-keyed agent cluster based on their scheme.
  DCHECK(!url.GetScheme().empty());
  GURL site_url = GURL(url.GetScheme() + ":");
  return AgentClusterKey::CreateSiteKeyed(site_url, oac_status);
}

// static
GURL SiteInfo::GetSiteForOrigin(const url::Origin& origin) {
  // Only keep the scheme and registered domain of |origin|.
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return SchemeAndHostToSite(origin.scheme(),
                             domain.empty() ? origin.host() : domain);
}

// static
WebExposedIsolationLevel SiteInfo::ComputeWebExposedIsolationLevel(
    const WebExposedIsolationInfo& web_exposed_isolation_info,
    const UrlInfo& url_info) {
  if (!web_exposed_isolation_info.is_isolated()) {
    return WebExposedIsolationLevel::kNotIsolated;
  }
  if (!web_exposed_isolation_info.is_isolated_application()) {
    return WebExposedIsolationLevel::kIsolated;
  }
  // The "application isolation" level cannot be delegated to processes locked
  // to other origins. Sandboxed frames are always considered cross-origin.
  if (url_info.is_sandboxed) {
    return WebExposedIsolationLevel::kIsolated;
  }
  url::Origin origin =
      GetPossiblyOverriddenOriginFromUrl(url_info.url, url_info.origin);
  return web_exposed_isolation_info.origin() == origin
             ? WebExposedIsolationLevel::kIsolatedApplication
             : WebExposedIsolationLevel::kIsolated;
}

// static
WebExposedIsolationLevel SiteInfo::ComputeWebExposedIsolationLevelForEmptySite(
    const WebExposedIsolationInfo& web_exposed_isolation_info) {
  return web_exposed_isolation_info.is_isolated()
             ? WebExposedIsolationLevel::kIsolated
             : WebExposedIsolationLevel::kNotIsolated;
}

// static
GURL SiteInfo::GetOriginBasedSiteURLForDataURL(const url::Origin& origin) {
  CHECK(origin.opaque());
  return GURL(url::kDataScheme + std::string(":") +
              origin.GetNonceForSerialization()->ToString());
}

// static
bool SiteInfo::RequiresDedicatedProcessInternal(
    const GURL& site_url,
    const IsolationContext& isolation_context,
    BrowserContext* browser_context,
    bool does_site_request_dedicated_process_for_coop,
    bool requires_origin_keyed_process,
    bool is_sandboxed,
    bool is_pdf) {
  // If --site-per-process is enabled, site isolation is enabled everywhere.
  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites()) {
    return true;
  }

  // If there is a COOP header request to require a dedicated process for this
  // SiteInfo, honor it.  Note that we have already checked other eligibility
  // criteria such as memory thresholds prior to setting this bit on SiteInfo.
  if (does_site_request_dedicated_process_for_coop) {
    return true;
  }

  // Always require a dedicated process for isolated origins.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (policy->IsIsolatedOrigin(isolation_context, url::Origin::Create(site_url),
                               requires_origin_keyed_process)) {
    return true;
  }

  // Require a dedicated process for all sandboxed frames. Note: If this
  // SiteInstance is a sandboxed child of a sandboxed parent, then the logic in
  // RenderFrameHostManager::CanUseSourceSiteInstance will assign the child to
  // the parent's SiteInstance, so we don't need to worry about the parent's
  // sandbox status here.
  if (is_sandboxed) {
    return true;
  }

  // Error pages in main frames do require isolation, however since this is
  // missing the context whether this is for a main frame or not, that part
  // is enforced in RenderFrameHostManager.
  if (site_url == GetErrorPageSiteAndLockURL()) {
    return true;
  }

  // Isolate PDF content.
  if (is_pdf) {
    return true;
  }

  // Isolate WebUI pages from one another and from other kinds of schemes.
  for (const auto& webui_scheme : URLDataManagerBackend::GetWebUISchemes()) {
    if (site_url.SchemeIs(webui_scheme)) {
      return true;
    }
  }

  // Let the content embedder enable site isolation for specific URLs. Use the
  // canonical site url for this check, so that schemes with nested origins
  // (blob and filesystem) work properly.
  if (GetContentClient()->browser()->DoesSiteRequireDedicatedProcess(
          browser_context, site_url)) {
    return true;
  }

  return false;
}

// static
GURL SiteInfo::GetSiteForURLForTest(const IsolationContext& isolation_context,
                                    const UrlInfo& url_info,
                                    bool should_use_effective_urls) {
  std::optional<GURL> effective_url;
  if (should_use_effective_urls) {
    effective_url = GetContentClient()->browser()->GetEffectiveURL(
        isolation_context.browser_context(), url_info.url);
  }
  return GetAgentClusterKeyForURL(isolation_context, url_info, effective_url)
      .GetURL();
}

// static
const url::Origin& SiteInfo::GetOriginForUnlockedProcess() {
  static base::NoDestructor<url::Origin> unlocked_process_origin;
  return *unlocked_process_origin;
}

}  // namespace content
