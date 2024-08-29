// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_info.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/ranges/algorithm.h"
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
  return base::SplitString(url.host_piece(), ".", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_ALL);
}

// Checks if the `url` is a special case WebUI URL of the form
// chrome://foo.bar/. Such URLs will employ LockURLs based on their TLD (ie
// chome://bar/). This will allow WebUI URLs of the above form with common TLDs
// to share a process whilst maintaining independent SiteURLs to allow for
// WebUIType differentiation.
bool IsWebUIAndUsesTLDForProcessLockURL(const GURL& url) {
  if (!base::Contains(URLDataManagerBackend::GetWebUISchemes(), url.scheme()))
    return false;

  WebUIDomains domains = GetWebUIDomains(url);
  // This only applies to WebUI urls with two or more non-empty domains.
  return domains.size() >= 2 &&
         base::ranges::all_of(domains, [](const std::string& domain) {
           return !domain.empty();
         });
}

// For WebUI URLs of the form chrome://foo.bar/ creates the appropriate process
// lock URL. See comment for `IsWebUIAndUsesTLDForProcessLockURL()`.
GURL GetProcessLockForWebUIURL(const GURL& url) {
  DCHECK(IsWebUIAndUsesTLDForProcessLockURL(url));
  WebUIDomains host_domains = GetWebUIDomains(url);
  return GURL(url.scheme() + url::kStandardSchemeSeparator +
              host_domains.back());
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

}  // namespace

// static
SiteInfo SiteInfo::CreateForErrorPage(
    const StoragePartitionConfig storage_partition_config,
    bool is_guest,
    bool is_fenced,
    const WebExposedIsolationInfo& web_exposed_isolation_info,
    WebExposedIsolationLevel web_exposed_isolation_level) {
  return SiteInfo(GetErrorPageSiteAndLockURL() /* site_url */,
                  GetErrorPageSiteAndLockURL() /* process_lock_url */,
                  false /* requires_origin_keyed_process */,
                  false /* requires_origin_keyed_process_by_default */,
                  false /* is_sandboxed */, UrlInfo::kInvalidUniqueSandboxId,
                  storage_partition_config, web_exposed_isolation_info,
                  web_exposed_isolation_level, is_guest,
                  false /* does_site_request_dedicated_process_for_coop */,
                  false /* is_jit_disabled */,
                  false /* are_v8_optimizations_disabled */, false /* is_pdf */,
                  is_fenced, std::nullopt);
}

// static
SiteInfo SiteInfo::CreateForDefaultSiteInstance(
    const IsolationContext& isolation_context,
    const StoragePartitionConfig storage_partition_config,
    const WebExposedIsolationInfo& web_exposed_isolation_info) {
  // Get default JIT policy for this browser_context by passing in an empty
  // site_url.
  BrowserContext* browser_context =
      isolation_context.browser_or_resource_context().ToBrowserContext();
  bool is_jit_disabled = GetContentClient()->browser()->IsJitDisabledForSite(
      browser_context, GURL());
  bool are_v8_optimizations_disabled =
      GetContentClient()->browser()->AreV8OptimizationsDisabledForSite(
          browser_context, GURL());

  WebExposedIsolationLevel web_exposed_isolation_level =
      SiteInfo::ComputeWebExposedIsolationLevelForEmptySite(
          web_exposed_isolation_info);

  return SiteInfo(
      /*site_url=*/SiteInstanceImpl::GetDefaultSiteURL(),
      /*process_lock_url=*/SiteInstanceImpl::GetDefaultSiteURL(),
      /*requires_origin_keyed_process=*/false,
      /*requires_origin_keyed_process_by_default=*/false,
      /*is_sandboxed=*/false, UrlInfo::kInvalidUniqueSandboxId,
      storage_partition_config, web_exposed_isolation_info,
      web_exposed_isolation_level, isolation_context.is_guest(),
      /*does_site_request_dedicated_process_for_coop=*/false, is_jit_disabled,
      are_v8_optimizations_disabled, /*is_pdf=*/false,
      isolation_context.is_fenced(), std::nullopt);
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
      /*site_url=*/GURL(), /*process_lock_url=*/GURL(),
      /*requires_origin_keyed_process=*/false,
      /*requires_origin_keyed_process_by_default=*/false,
      /*is_sandboxed=*/false, UrlInfo::kInvalidUniqueSandboxId,
      partition_config, WebExposedIsolationInfo::CreateNonIsolated(),
      WebExposedIsolationLevel::kNotIsolated,
      /*is_guest=*/true,
      /*does_site_request_dedicated_process_for_coop=*/false,
      /*is_jit_disabled=*/false, /*are_v8_optimizations_disabled=*/false,
      /*is_pdf=*/false, /*is_fenced=*/false, std::nullopt);
}

// static
SiteInfo SiteInfo::Create(const IsolationContext& isolation_context,
                          const UrlInfo& url_info) {
  // The call to GetSiteForURL() below is only allowed on the UI thread, due to
  // its possible use of effective urls.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return CreateInternal(isolation_context, url_info,
                        /*compute_site_url=*/true);
}

// static
SiteInfo SiteInfo::CreateOnIOThread(const IsolationContext& isolation_context,
                                    const UrlInfo& url_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(url_info.storage_partition_config.has_value());
  return CreateInternal(isolation_context, url_info,
                        /*compute_site_url=*/false);
}

// static
SiteInfo SiteInfo::CreateInternal(const IsolationContext& isolation_context,
                                  const UrlInfo& url_info,
                                  bool compute_site_url) {
  DCHECK(url_info.is_sandboxed ||
         url_info.unique_sandbox_id == UrlInfo::kInvalidUniqueSandboxId);
  GURL lock_url = DetermineProcessLockURL(isolation_context, url_info);
  GURL site_url = lock_url;

  // PDF content should live in JIT-less processes because it is inherently less
  // trusted.
  bool is_jitless = url_info.is_pdf;
  bool are_v8_optimizations_disabled = false;

  std::optional<StoragePartitionConfig> storage_partition_config =
      url_info.storage_partition_config;

  bool use_origin_keyed_process_for_sandbox_data_url = false;
  if (compute_site_url) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    site_url = GetSiteForURLInternal(isolation_context, url_info,
                                     true /* should_use_effective_urls */);
    // If we have a sandboxed data url, and IsolateSandboxedIframes is enabled
    // in per-origin mode, then GetSiteForURLInternal() above will use the
    // precursor information to set the initiator's origin as the site url,
    // instead of an opaque data: <nonce> origin. In that case, we need to be
    // consistent and use the same url for computing the origin-keyed status,
    // via the call to DetermineOriginAgentClusterIsolation() below.
    use_origin_keyed_process_for_sandbox_data_url =
        url_info.url.SchemeIs(url::kDataScheme) &&
        IsOriginIsolatedSandboxedFrame(url_info);

    BrowserContext* browser_context =
        isolation_context.browser_or_resource_context().ToBrowserContext();
    is_jitless =
        is_jitless || GetContentClient()->browser()->IsJitDisabledForSite(
                          browser_context, lock_url);
    are_v8_optimizations_disabled =
        GetContentClient()->browser()->AreV8OptimizationsDisabledForSite(
            browser_context, lock_url);

    if (!storage_partition_config.has_value()) {
      storage_partition_config =
          GetStoragePartitionConfigForUrl(browser_context, site_url);
    }
  }
  DCHECK(storage_partition_config.has_value());

  WebExposedIsolationInfo web_exposed_isolation_info =
      url_info.web_exposed_isolation_info.value_or(
          WebExposedIsolationInfo::CreateNonIsolated());
  WebExposedIsolationLevel web_exposed_isolation_level =
      ComputeWebExposedIsolationLevel(web_exposed_isolation_info, url_info);

  if (url_info.url.SchemeIs(kChromeErrorScheme)) {
    return CreateForErrorPage(storage_partition_config.value(),
                              /*is_guest=*/isolation_context.is_guest(),
                              /*is_fenced=*/isolation_context.is_fenced(),
                              web_exposed_isolation_info,
                              web_exposed_isolation_level);
  }
  // We should only set |requires_origin_keyed_process| if we are actually
  // creating separate SiteInstances for OAC isolation. When we use site-keyed
  // processes for OAC, we don't do that at present.
  // TODO(wjmaclean): Once SiteInstanceGroups are fully implemented, we should
  // be able to give all OAC origins their own SiteInstance.
  // https://crbug.com/1195535
  OriginAgentClusterIsolationState requested_isolation_state =
      isolation_context.default_isolation_state();
  if (!url_info.requests_default_origin_agent_cluster_isolation()) {
    // In this case, url_info is not using OAC by default, so we only need to
    // check the by_header() functions to determine the isolation state.
    // (RequestsOriginKeyedProcess(isolation_context) only behaves differently
    // in the non-header / by-default case.)
    requested_isolation_state =
        url_info.requests_origin_agent_cluster_by_header()
            ? OriginAgentClusterIsolationState::CreateForOriginAgentCluster(
                  url_info.requests_origin_keyed_process_by_header())
            : OriginAgentClusterIsolationState::CreateNonIsolated();
  }
  // An origin-keyed process can only be used for origin-keyed agent clusters.
  CHECK(!requested_isolation_state.requires_origin_keyed_process() ||
        requested_isolation_state.is_origin_agent_cluster());

  bool requires_origin_keyed_process = false;

  if (SiteIsolationPolicy::IsProcessIsolationForOriginAgentClusterEnabled()) {
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    url::Origin origin;
    if (use_origin_keyed_process_for_sandbox_data_url) {
      origin = url::Origin::Create(site_url);
    } else {
      origin =
          GetPossiblyOverriddenOriginFromUrl(url_info.url, url_info.origin);
    }
    requires_origin_keyed_process =
        policy
            ->DetermineOriginAgentClusterIsolation(isolation_context, origin,
                                                   requested_isolation_state)
            .requires_origin_keyed_process();
  }
  // If after the call to `DetermineOriginAgentClusterIsolation` the returned
  // isolation state has `requires_origin_keyed_process() == true`, and if the
  // requested `url_info` was for default isolation, then we know that
  // `requires_origin_keyed_process` is true by default; we track that in
  // `requires_origin_keyed_process_by_default` so that later we know not to
  // add the isolation state to the per-BrowsingInstance tracking.
  bool requires_origin_keyed_process_by_default =
      requires_origin_keyed_process &&
      url_info.requests_default_origin_agent_cluster_isolation();

  // If there is a COOP isolation request, propagate it to SiteInfo.
  // This will be used later when determining a suitable SiteInstance
  // and BrowsingInstance for this SiteInfo.
  bool does_site_request_dedicated_process_for_coop =
      url_info.requests_coop_isolation();

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
  return SiteInfo(site_url, lock_url, requires_origin_keyed_process,
                  requires_origin_keyed_process_by_default,
                  url_info.is_sandboxed, url_info.unique_sandbox_id,
                  storage_partition_config.value(), web_exposed_isolation_info,
                  web_exposed_isolation_level, isolation_context.is_guest(),
                  does_site_request_dedicated_process_for_coop, is_jitless,
                  are_v8_optimizations_disabled, url_info.is_pdf,
                  isolation_context.is_fenced(),
                  url_info.cross_origin_isolation_key);
}

// static
SiteInfo SiteInfo::CreateForTesting(const IsolationContext& isolation_context,
                                    const GURL& url) {
  return Create(isolation_context, UrlInfo::CreateForTesting(url));
}

SiteInfo::SiteInfo(
    const GURL& site_url,
    const GURL& process_lock_url,
    bool requires_origin_keyed_process,
    bool requires_origin_keyed_process_by_default,
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
    bool is_fenced,
    const std::optional<AgentClusterKey::CrossOriginIsolationKey>&
        cross_origin_isolation_key)
    : site_url_(site_url),
      process_lock_url_(process_lock_url),
      requires_origin_keyed_process_(requires_origin_keyed_process),
      requires_origin_keyed_process_by_default_(
          requires_origin_keyed_process_by_default),
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
  DCHECK(!requires_origin_keyed_process_by_default_ ||
         requires_origin_keyed_process_);

  // Compute the AgentClusterKey matching this SiteInfo. Currently, this is only
  // computed when DocumentIsolationPolicy is enabled and
  // CrossOriginIsolationKey is passed.
  // TODO(crbug.com/342572253): Return a site-keyed AgentClusterKey when the
  // agent cluster cannot be origin-keyed.
  // TODO(crbug.com/342365078): Return an origin-keyed AgentClusterKey when the
  // navigation has Origin-Agent-Cluster: ?1.
  // TODO(crbug.com/342366372): Return an origin-keyed AgentClusterKey code by
  // default once SiteInstanceGroup has shipped and different SiteInstances can
  // share the same process.
  if (cross_origin_isolation_key.has_value()) {
    // Note: because we only get a CrossOriginIsolationKey when
    // DocumentIsolationPolicy is enabled, the origin of CrossOriginIsolationKey
    // is the same as the origin that should be used for the AgentClusterKey, so
    // we can use it to create the AgentClusterKey.
    //
    // This will not be true when COOP + COEP also passes a
    // CrossOriginIsolationKey, and the actual origin will need to be passed
    // along.
    agent_cluster_key_ = AgentClusterKey::CreateWithCrossOriginIsolationKey(
        cross_origin_isolation_key->common_coi_origin,
        cross_origin_isolation_key.value());
  }
}
SiteInfo::SiteInfo(const SiteInfo& rhs) = default;

SiteInfo::~SiteInfo() = default;

SiteInfo::SiteInfo(BrowserContext* browser_context)
    : SiteInfo(
          /*site_url=*/GURL(),
          /*process_lock_url=*/GURL(),
          /*requires_origin_keyed_process=*/false,
          /*requires_origin_keyed_process_by_default=*/false,
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
          /*is_fenced=*/false,
          /*cross_origin_isolation_key=*/std::nullopt) {}

// static
auto SiteInfo::MakeSecurityPrincipalKey(const SiteInfo& site_info) {
  // Note: `does_site_request_dedicated_process_for_coop_` is intentionally
  // excluded here, as a difference solely in that field should not cause a
  // different SiteInstance to be created.  A document that has been
  // site-isolated due to COOP should still share a SiteInstance with other
  // same-site frames in the BrowsingInstance, even if those frames lack the
  // COOP isolation request.
  return std::tie(
      site_info.site_url_.possibly_invalid_spec(),
      site_info.process_lock_url_.possibly_invalid_spec(),
      // Here we only compare |requires_origin_keyed_process_| since
      // we currently don't create SiteInfos where
      // |is_origin_agent_cluster_| differs from
      // |requires_origin_keyed_process_|. In fact, we don't even
      // have |is_origin_agent_cluster| in SiteInfo at this time,
      // but that could change.
      // TODO(wjmaclean): Update this if we ever start to create
      // separate SiteInfos for same-process OriginAgentCluster.
      site_info.requires_origin_keyed_process_, site_info.is_sandboxed_,
      site_info.unique_sandbox_id_, site_info.storage_partition_config_,
      site_info.web_exposed_isolation_info_,
      site_info.web_exposed_isolation_level_, site_info.is_guest_,
      site_info.is_jit_disabled_, site_info.are_v8_optimizations_disabled_,
      site_info.is_pdf_, site_info.is_fenced_, site_info.agent_cluster_key_);
}

SiteInfo SiteInfo::GetNonOriginKeyedEquivalentForMetrics(
    const IsolationContext& isolation_context) const {
  SiteInfo non_oac_site_info(*this);
  if (requires_origin_keyed_process()) {
    DCHECK(process_lock_url_.SchemeIs(url::kHttpsScheme));
    non_oac_site_info.requires_origin_keyed_process_ = false;

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
    if (policy->GetMatchingProcessIsolatedOrigin(
            IsolationContext(BrowsingInstanceId(0),
                             isolation_context.browser_or_resource_context(),
                             isolation_context.is_guest(),
                             isolation_context.is_fenced(),
                             isolation_context.default_isolation_state()),
            url::Origin::Create(process_lock_url_),
            false /* origin_requests_isolation */, &result_origin)) {
      non_oac_site_info.process_lock_url_ = result_origin.GetURL();
    } else {
      non_oac_site_info.process_lock_url_ =
          GetSiteForOrigin(url::Origin::Create(process_lock_url_));
    }
    // Only convert the site_url_ if it matches the process_lock_url_, otherwise
    // leave it alone. This will only matter for hosted apps, and we only expect
    // them to differ if an effective URL is defined.
    if (site_url_ == process_lock_url_)
      non_oac_site_info.site_url_ = non_oac_site_info.process_lock_url_;
  }
  return non_oac_site_info;
}

SiteInfo& SiteInfo::operator=(const SiteInfo& rhs) = default;

bool SiteInfo::IsSamePrincipalWith(const SiteInfo& other) const {
  return MakeSecurityPrincipalKey(*this) == MakeSecurityPrincipalKey(other);
}

bool SiteInfo::IsExactMatch(const SiteInfo& other) const {
  bool is_match =
      site_url_ == other.site_url_ &&
      process_lock_url_ == other.process_lock_url_ &&
      requires_origin_keyed_process_ == other.requires_origin_keyed_process_ &&
      is_sandboxed_ == other.is_sandboxed_ &&
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
  // document origins match. We use process_lock_url() comparisons to account
  // for this.
  //
  // TODO(wjmaclean, alexmos): Figure out why including `is_jit_disabled_` here
  // leads to crashes in https://crbug.com/1279453.
  // TODO(ellyjones): Same as above, but about are_v8_optimizations_disabled_
  // (presumably).
  return std::tie(process_lock_url_, requires_origin_keyed_process_,
                  is_sandboxed_, unique_sandbox_id_, is_pdf_, is_guest_,
                  web_exposed_isolation_info_, web_exposed_isolation_level_,
                  storage_partition_config_, is_fenced_, agent_cluster_key_);
}

int SiteInfo::ProcessLockCompareTo(const SiteInfo& other) const {
  auto a = MakeProcessLockComparisonKey();
  auto b = other.MakeProcessLockComparisonKey();
  if (a < b)
    return -1;
  if (b < a)
    return 1;
  return 0;
}

bool SiteInfo::operator==(const SiteInfo& other) const {
  return IsSamePrincipalWith(other);
}

bool SiteInfo::operator!=(const SiteInfo& other) const {
  return !IsSamePrincipalWith(other);
}

bool SiteInfo::operator<(const SiteInfo& other) const {
  return MakeSecurityPrincipalKey(*this) < MakeSecurityPrincipalKey(other);
}

std::string SiteInfo::GetDebugString() const {
  std::string debug_string =
      site_url_.is_empty() ? "empty site" : site_url_.possibly_invalid_spec();

  if (process_lock_url_.is_empty())
    debug_string += ", empty lock";
  else if (process_lock_url_ != site_url_)
    debug_string += ", locked to " + process_lock_url_.possibly_invalid_spec();

  if (requires_origin_keyed_process_)
    debug_string += ", origin-keyed";

  if (is_sandboxed_) {
    debug_string += ", sandboxed";
    if (unique_sandbox_id_ != UrlInfo::kInvalidUniqueSandboxId)
      debug_string += base::StringPrintf(" (id=%d)", unique_sandbox_id_);
  }

  if (web_exposed_isolation_info_.is_isolated()) {
    debug_string += ", cross-origin isolated";
    if (web_exposed_isolation_info_.is_isolated_application())
      debug_string += " application";
    debug_string += ", coi-origin='" +
                    web_exposed_isolation_info_.origin().GetDebugString() + "'";
  }

  if (web_exposed_isolation_info_.is_isolated_application() &&
      web_exposed_isolation_level_ <
          WebExposedIsolationLevel::kIsolatedApplication) {
    debug_string += ", application isolation not inherited";
  }

  if (is_guest_)
    debug_string += ", guest";

  if (does_site_request_dedicated_process_for_coop_)
    debug_string += ", requests coop isolation";

  if (is_jit_disabled_)
    debug_string += ", jitless";

  if (are_v8_optimizations_disabled_) {
    debug_string += ", noopt";
  }

  if (is_pdf_)
    debug_string += ", pdf";

  if (!storage_partition_config_.is_default()) {
    debug_string +=
        ", partition=" + storage_partition_config_.partition_domain() + "." +
        storage_partition_config_.partition_name();
    if (storage_partition_config_.in_memory())
      debug_string += ", in-memory";
  }

  if (is_fenced_)
    debug_string += ", is_fenced";

  if (agent_cluster_key_ && agent_cluster_key_->IsOriginKeyed()) {
    debug_string += ", origin-keyed agent cluster";
  }

  if (agent_cluster_key_ &&
      agent_cluster_key_->GetCrossOriginIsolationKey().has_value()) {
    debug_string += ", coi agent cluster origin=" +
                    agent_cluster_key_->GetCrossOriginIsolationKey()
                        ->common_coi_origin.GetDebugString();
    if (agent_cluster_key_->GetCrossOriginIsolationKey()
            ->cross_origin_isolation_mode ==
        CrossOriginIsolationMode::kConcrete) {
      debug_string += ", concrete coi";
    } else if (agent_cluster_key_->GetCrossOriginIsolationKey()
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
  DCHECK(isolation_context.browser_or_resource_context());

  // If --site-per-process is enabled, site isolation is enabled everywhere.
  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
    return true;

  // If there is a COOP header request to require a dedicated process for this
  // SiteInfo, honor it.  Note that we have already checked other eligibility
  // criteria such as memory thresholds prior to setting this bit on SiteInfo.
  if (does_site_request_dedicated_process_for_coop_)
    return true;

  // Always require a dedicated process for isolated origins.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (policy->IsIsolatedOrigin(isolation_context,
                               url::Origin::Create(site_url_),
                               requires_origin_keyed_process_)) {
    return true;
  }

  // Require a dedicated process for all sandboxed frames. Note: If this
  // SiteInstance is a sandboxed child of a sandboxed parent, then the logic in
  // RenderFrameHostManager::CanUseSourceSiteInstance will assign the child to
  // the parent's SiteInstance, so we don't need to worry about the parent's
  // sandbox status here.
  if (is_sandboxed_)
    return true;

  // Error pages in main frames do require isolation, however since this is
  // missing the context whether this is for a main frame or not, that part
  // is enforced in RenderFrameHostManager.
  if (is_error_page())
    return true;

  // Isolate PDF content.
  if (is_pdf_)
    return true;

  // Isolate WebUI pages from one another and from other kinds of schemes.
  for (const auto& webui_scheme : URLDataManagerBackend::GetWebUISchemes()) {
    if (site_url_.SchemeIs(webui_scheme))
      return true;
  }

  // Let the content embedder enable site isolation for specific URLs. Use the
  // canonical site url for this check, so that schemes with nested origins
  // (blob and filesystem) work properly.
  if (GetContentClient()->browser()->DoesSiteRequireDedicatedProcess(
          isolation_context.browser_or_resource_context().ToBrowserContext(),
          site_url_)) {
    return true;
  }

  return false;
}

bool SiteInfo::ShouldLockProcessToSite(
    const IsolationContext& isolation_context) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserContext* browser_context =
      isolation_context.browser_or_resource_context().ToBrowserContext();
  DCHECK(browser_context);

  // Don't lock to origin in --single-process mode, since this mode puts
  // cross-site pages into the same process.  Note that this also covers the
  // single-process mode in Android Webview.
  if (RenderProcessHost::run_renderer_in_process())
    return false;

  if (!RequiresDedicatedProcess(isolation_context))
    return false;

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
  if (command_line.HasSwitch(switches::kProcessPerSite))
    return true;

  // Error pages should use process-per-site model, as it is useful to
  // consolidate them to minimize resource usage and there is no security
  // drawback to combining them all in the same process.
  if (is_error_page())
    return true;

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
  dict.Add("process_lock_url", process_lock_url());
  dict.Add("requires_origin_keyed_process", requires_origin_keyed_process_);
  dict.Add("is_sandboxed", is_sandboxed_);
  dict.Add("is_guest", is_guest_);
  dict.Add("is_fenced", is_fenced_);
}

bool SiteInfo::is_error_page() const {
  return site_url_ == GetErrorPageSiteAndLockURL();
}

// static
GURL SiteInfo::DetermineProcessLockURL(
    const IsolationContext& isolation_context,
    const UrlInfo& url_info) {
  // For WebUI URLs of the form chrome://foo.bar/ compute the LockURL based on
  // the TLD (ie chrome://bar/). This allows WebUI to continue to differentiate
  // WebUIType via SiteURL while allowing WebUI with a shared TLD to share a
  // RenderProcessHost.
  // TODO(tluk): Remove this and replace it with SiteInstance groups once the
  // support lands.
  if (IsWebUIAndUsesTLDForProcessLockURL(url_info.url))
    return GetProcessLockForWebUIURL(url_info.url);

  // For the process lock URL, convert |url| to a site without resolving |url|
  // to an effective URL.
  return GetSiteForURLInternal(isolation_context, url_info,
                               false /* should_use_effective_urls */);
}

// static
GURL SiteInfo::GetSiteForURLInternal(const IsolationContext& isolation_context,
                                     const UrlInfo& real_url_info,
                                     bool should_use_effective_urls) {
  const GURL& real_url = real_url_info.url;
  // Explicitly map all chrome-error: URLs to a single URL so that they all
  // end up in a dedicated error process.
  if (real_url.SchemeIs(kChromeErrorScheme))
    return GetErrorPageSiteAndLockURL();

  if (should_use_effective_urls)
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GURL url = should_use_effective_urls
                 ? SiteInstanceImpl::GetEffectiveURL(
                       isolation_context.browser_or_resource_context()
                           .ToBrowserContext(),
                       real_url)
                 : real_url;

  url::Origin origin =
      GetPossiblyOverriddenOriginFromUrl(url, real_url_info.origin);

  // If the url has a host, then determine the site.  Skip file URLs to avoid a
  // situation where site URL of file://localhost/ would mismatch Blink's origin
  // (which ignores the hostname in this case - see https://crbug.com/776160).
  GURL site_url;
  bool use_origin_keyed_process = IsOriginIsolatedSandboxedFrame(real_url_info);
  if (!origin.host().empty() && origin.scheme() != url::kFileScheme) {
    // For Strict Origin Isolation, use the full origin instead of site for all
    // HTTP/HTTPS URLs.  Note that the HTTP/HTTPS restriction guarantees that
    // we won't hit this for hosted app effective URLs (see
    // https://crbug.com/961386).
    if (SiteIsolationPolicy::IsStrictOriginIsolationEnabled() &&
        origin.GetURL().SchemeIsHTTPOrHTTPS()) {
      return origin.GetURL();
    }

    // For isolated sandboxed iframes in per-origin mode we also just return the
    // origin, as we should be using the full origin for the SiteInstance, but
    // we don't need to track the origin like we do for OriginAgentCluster.
    if (use_origin_keyed_process) {
      return origin.GetURL();
    }

    site_url = GetSiteForOrigin(origin);

    // Isolated origins should use the full origin as their site URL. A
    // subdomain of an isolated origin should also use that isolated origin's
    // site URL. It is important to check |origin| (based on |url|) rather than
    // |real_url| here, since some effective URLs (such as for NTP) need to be
    // resolved prior to the isolated origin lookup.
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    url::Origin isolated_origin;
    if (policy->GetMatchingProcessIsolatedOrigin(
            isolation_context, origin,
            real_url_info.RequestsOriginKeyedProcess(isolation_context),
            site_url, &isolated_origin)) {
      return isolated_origin.GetURL();
    }
  } else {
    // If there is no host but there is a scheme, return the scheme.
    // This is useful for cases like file URLs.
    if (!origin.opaque()) {
      // Prefer to use the scheme of |origin| rather than |url|, to correctly
      // cover blob:file: and filesystem:file: URIs (see also
      // https://crbug.com/697111).
      DCHECK(!origin.scheme().empty());
      site_url = GURL(origin.scheme() + ":");
    } else if (url.has_scheme()) {
      if (url.SchemeIs(url::kDataScheme)) {
        if (use_origin_keyed_process) {
          // Sandboxed data: subframes should be in the process of their
          // precursor origin.
          DUMP_WILL_BE_CHECK(real_url_info.origin->opaque());
          DUMP_WILL_BE_CHECK(
              real_url_info.origin->GetTupleOrPrecursorTupleIfOpaque()
                  .IsValid());
          site_url =
              real_url_info.origin->GetTupleOrPrecursorTupleIfOpaque().GetURL();
        } else {
          // We get here for browser-initiated navigations to data URLs.
          // We use the serialized opaque origin as the body of the data: URL to
          // avoid storing the entire data: URL multiple times, and to use the
          // origin's nonce to distinguish between instances of the same URL.
          // This means each browser-initiated data: URL will get its own
          // process. See https://crbug.com/863069.
          site_url = GetOriginBasedSiteURLForDataURL(origin);
        }
      } else if (url.SchemeIsBlob()) {
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
        if (url.has_ref()) {
          GURL::Replacements replacements;
          replacements.ClearRef();
          url = url.ReplaceComponents(replacements);
        }
        site_url = url;
      } else {
        DCHECK(!url.scheme().empty());
        site_url = GURL(url.scheme() + ":");
      }
    } else {
      // Otherwise the URL should be invalid; return an empty site.
      DCHECK(!url.is_valid()) << url;
      return GURL();
    }
  }

  return site_url;
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

}  // namespace content
