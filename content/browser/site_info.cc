// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_info.h"

#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/strings/string_split.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

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
         std::all_of(domains.begin(), domains.end(),
                     [](const std::string& domain) { return !domain.empty(); });
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

}  // namespace

// static
SiteInfo SiteInfo::CreateForErrorPage(
    const StoragePartitionConfig storage_partition_config) {
  return SiteInfo(GetErrorPageSiteAndLockURL(), GetErrorPageSiteAndLockURL(),
                  false /* is_origin_keyed */, storage_partition_config,
                  WebExposedIsolationInfo::CreateNonIsolated(),
                  false /* is_guest */,
                  false /* does_site_request_dedicated_process_for_coop */,
                  false /* is_jit_disabled */, false /* is_pdf */);
}

// static
SiteInfo SiteInfo::CreateForDefaultSiteInstance(
    BrowserContext* browser_context,
    const StoragePartitionConfig storage_partition_config,
    const WebExposedIsolationInfo& web_exposed_isolation_info) {
  // Get default JIT policy for this browser_context by passing in an empty
  // site_url.
  bool is_jit_disabled = GetContentClient()->browser()->IsJitDisabledForSite(
      browser_context, GURL());

  return SiteInfo(SiteInstanceImpl::GetDefaultSiteURL(),
                  SiteInstanceImpl::GetDefaultSiteURL(),
                  false /* is_origin_keyed */, storage_partition_config,
                  web_exposed_isolation_info, false /* is_guest */,
                  false /* does_site_request_dedicated_process_for_coop */,
                  is_jit_disabled, false /* is_pdf */);
}

// static
SiteInfo SiteInfo::CreateForGuest(BrowserContext* browser_context,
                                  const GURL& guest_site_url) {
  // Setting site and lock directly without the site URL conversions we
  // do for user provided URLs. Callers expect GetSiteURL() to return the
  // value they provide in |guest_site_url|.
  return SiteInfo(
      guest_site_url, guest_site_url, false /* is_origin_keyed */,
      GetStoragePartitionConfigForUrl(browser_context, guest_site_url,
                                      /*is_site_url=*/true),
      WebExposedIsolationInfo::CreateNonIsolated(), true /* is_guest */,
      false /* does_site_request_dedicated_process_for_coop */,
      false /* is_jit_disabled */, false /* is_pdf */);
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
  GURL lock_url = DetermineProcessLockURL(isolation_context, url_info);
  GURL site_url = lock_url;

  // PDF content should live in JIT-less processes because it is inherently less
  // trusted.
  bool is_jitless = url_info.is_pdf;

  absl::optional<StoragePartitionConfig> storage_partition_config =
      url_info.storage_partition_config;

  if (compute_site_url) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    site_url = GetSiteForURLInternal(isolation_context, url_info,
                                     true /* should_use_effective_urls */);

    BrowserContext* browser_context =
        isolation_context.browser_or_resource_context().ToBrowserContext();
    is_jitless =
        is_jitless || GetContentClient()->browser()->IsJitDisabledForSite(
                          browser_context, lock_url);

    if (!storage_partition_config.has_value()) {
      storage_partition_config =
          GetStoragePartitionConfigForUrl(browser_context, site_url,
                                          /*is_site_url=*/true);
    }
  }
  DCHECK(storage_partition_config.has_value());

  if (url_info.url.SchemeIs(kChromeErrorScheme)) {
    // Error pages should never be cross origin isolated.
    DCHECK(!url_info.web_exposed_isolation_info.is_isolated());
    return CreateForErrorPage(storage_partition_config.value());
  }
  // We should only set |is_origin_keyed| if we are actually creating separate
  // SiteInstances for OAC isolation. When we do same-process OAC, we don't do
  // that at present.
  // TODO(wjmaclean): Once SiteInstanceGroups are fully implemented, we should
  // be able to give spOAC origins their own SiteInstance.
  // https://crbug.com/1195535
  bool is_origin_keyed =
      SiteIsolationPolicy::IsProcessIsolationForOriginAgentClusterEnabled() &&
      ChildProcessSecurityPolicyImpl::GetInstance()
          ->ShouldOriginGetOptInIsolation(
              isolation_context, url::Origin::Create(url_info.url),
              url_info.requests_origin_agent_cluster_isolation());

  // If there is a COOP isolation request, propagate it to SiteInfo.  This will
  // be used later when determining a suitable SiteInstance and
  // BrowsingInstance for this SiteInfo.
  bool does_site_request_dedicated_process_for_coop =
      url_info.requests_coop_isolation();

  return SiteInfo(site_url, lock_url, is_origin_keyed,
                  storage_partition_config.value(),
                  url_info.web_exposed_isolation_info, false /* is_guest */,
                  does_site_request_dedicated_process_for_coop, is_jitless,
                  url_info.is_pdf);
}

// static
SiteInfo SiteInfo::CreateForTesting(const IsolationContext& isolation_context,
                                    const GURL& url) {
  return Create(isolation_context, UrlInfo::CreateForTesting(url));
}

SiteInfo::SiteInfo(const GURL& site_url,
                   const GURL& process_lock_url,
                   bool is_origin_keyed,
                   const StoragePartitionConfig storage_partition_config,
                   const WebExposedIsolationInfo& web_exposed_isolation_info,
                   bool is_guest,
                   bool does_site_request_dedicated_process_for_coop,
                   bool is_jit_disabled,
                   bool is_pdf)
    : site_url_(site_url),
      process_lock_url_(process_lock_url),
      is_origin_keyed_(is_origin_keyed),
      storage_partition_config_(storage_partition_config),
      web_exposed_isolation_info_(web_exposed_isolation_info),
      is_guest_(is_guest),
      does_site_request_dedicated_process_for_coop_(
          does_site_request_dedicated_process_for_coop),
      is_jit_disabled_(is_jit_disabled),
      is_pdf_(is_pdf) {}
SiteInfo::SiteInfo(const SiteInfo& rhs) = default;

SiteInfo::~SiteInfo() = default;

SiteInfo::SiteInfo(BrowserContext* browser_context)
    : SiteInfo(
          /*site_url=*/GURL(),
          /*process_lock_url=*/GURL(),
          /*is_origin_keyed=*/false,
          StoragePartitionConfig::CreateDefault(browser_context),
          WebExposedIsolationInfo::CreateNonIsolated(),
          /*is_guest=*/false,
          /*does_site_request_dedicated_process_for_coop=*/false,
          /*is_jit_disabled=*/false,
          /*is_pdf=*/false) {}

// static
auto SiteInfo::MakeSecurityPrincipalKey(const SiteInfo& site_info) {
  // Note: `does_site_request_dedicated_process_for_coop_` is intentionally
  // excluded here, as a difference solely in that field should not cause a
  // different SiteInstance to be created.  A document that has been
  // site-isolated due to COOP should still share a SiteInstance with other
  // same-site frames in the BrowsingInstance, even if those frames lack the
  // COOP isolation request.
  return std::tie(site_info.site_url_.possibly_invalid_spec(),
                  site_info.process_lock_url_.possibly_invalid_spec(),
                  site_info.is_origin_keyed_,
                  site_info.storage_partition_config_,
                  site_info.web_exposed_isolation_info_, site_info.is_guest_,
                  site_info.is_jit_disabled_, site_info.is_pdf_);
}

SiteInfo& SiteInfo::operator=(const SiteInfo& rhs) = default;

bool SiteInfo::IsSamePrincipalWith(const SiteInfo& other) const {
  return MakeSecurityPrincipalKey(*this) == MakeSecurityPrincipalKey(other);
}

bool SiteInfo::IsExactMatch(const SiteInfo& other) const {
  bool is_match =
      site_url_ == other.site_url_ &&
      process_lock_url_ == other.process_lock_url_ &&
      is_origin_keyed_ == other.is_origin_keyed_ &&
      storage_partition_config_ == other.storage_partition_config_ &&
      web_exposed_isolation_info_ == other.web_exposed_isolation_info_ &&
      is_guest_ == other.is_guest_ &&
      does_site_request_dedicated_process_for_coop_ ==
          other.does_site_request_dedicated_process_for_coop_ &&
      is_jit_disabled_ == other.is_jit_disabled_ && is_pdf_ == other.is_pdf_;

  if (is_match) {
    // If all the fields match, then the "same principal" subset must also
    // match. This is used to ensure these 2 methods stay in sync and all fields
    // used by IsSamePrincipalWith() are used by this function.
    DCHECK(IsSamePrincipalWith(other));
  }
  return is_match;
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

  if (is_origin_keyed_)
    debug_string += ", origin-keyed";

  if (web_exposed_isolation_info_.is_isolated()) {
    debug_string += ", cross-origin isolated";
    if (web_exposed_isolation_info_.is_isolated_application())
      debug_string += " application";
    debug_string += ", coi-origin='" +
                    web_exposed_isolation_info_.origin().GetDebugString() + "'";
  }

  if (is_guest_)
    debug_string += ", guest";

  if (does_site_request_dedicated_process_for_coop_)
    debug_string += ", requests coop isolation";

  if (is_jit_disabled_)
    debug_string += ", jitless";

  if (is_pdf_)
    debug_string += ", pdf";

  if (!storage_partition_config_.is_default()) {
    debug_string +=
        ", partition=" + storage_partition_config_.partition_domain() + "." +
        storage_partition_config_.partition_name();
    if (storage_partition_config_.in_memory())
      debug_string += ", in-memory";
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
                               is_origin_keyed_)) {
    return true;
  }

  // Error pages in main frames do require isolation, however since this is
  // missing the context whether this is for a main frame or not, that part
  // is enforced in RenderFrameHostManager.
  if (is_error_page())
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

  // Guest processes cannot be locked to a specific site because guests always
  // use a single SiteInstance for all URLs it loads. The SiteInfo for those
  // URLs do not match the SiteInfo of the guest SiteInstance so we skip
  // locking the guest process.
  // TODO(acolwell): Revisit this once we have the ability to store guest state
  // and StoragePartition information in SiteInfo instead of packing this info
  // into the guest site URL. Once we have these capabilities we won't need to
  // restrict guests to a single SiteInstance.
  if (is_guest_)
    return false;

  // Most WebUI processes should be locked on all platforms.  The only exception
  // is NTP, handled via the separate callout to the embedder.
  const auto& webui_schemes = URLDataManagerBackend::GetWebUISchemes();
  if (base::Contains(webui_schemes, site_url_.scheme())) {
    return GetContentClient()->browser()->DoesWebUISchemeRequireProcessLock(
        site_url_.scheme());
  }

  // Allow the embedder to prevent process locking so that multiple sites
  // can share a process. For example, this is how Chrome allows ordinary
  // extensions to share a process.
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

StoragePartitionId SiteInfo::GetStoragePartitionId(
    BrowserContext* browser_context) const {
  if (site_url().is_empty())
    return StoragePartitionId(browser_context);

  return StoragePartitionId(site_url().spec(), storage_partition_config());
}

// static
StoragePartitionConfig SiteInfo::GetStoragePartitionConfigForUrl(
    BrowserContext* browser_context,
    const GURL& url,
    bool is_site_url) {
  if (url.is_empty())
    return StoragePartitionConfig::CreateDefault(browser_context);

  if (!is_site_url && url.SchemeIs(kGuestScheme)) {
    // Guest schemes should only appear in site URLs. Generate a crash
    // dump to help debug unexpected callers that might not be setting
    // |is_site_url| correctly.
    // TODO(acolwell): Once we have confidence all callers are setting
    // |is_site_url| correctly, replace crash reporting with code that returns a
    // default config for this scheme in the non-site URL case.
    SCOPED_CRASH_KEY_STRING256("StoragePartitionConfigForUrl", "guest_url",
                               url.possibly_invalid_spec());
    base::debug::DumpWithoutCrashing();
  }

  return GetContentClient()->browser()->GetStoragePartitionConfigForSite(
      browser_context, url);
}

void SiteInfo::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("site_url", site_url());
  dict.Add("process_lock_url", process_lock_url());
  dict.Add("is_origin_keyed", is_origin_keyed_);
  dict.Add("is_guest", is_guest_);
}

bool SiteInfo::is_error_page() const {
  return !is_guest_ && site_url_ == GetErrorPageSiteAndLockURL();
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

  // Navigations to uuid-in-package: / urn: URLs served from Web Bundles [1]
  // require special care to use the origin of the bundle rather than the
  // uuid-in-package: / urn: URL, which lacks any origin information.
  // [1] bit.ly/subresource-web-bundles-doc
  // TODO(https://crbug.com/1257045): Remove urn: scheme support.
  // TODO(acolwell): Update this so we can use url::Origin::Resolve() for all
  // cases.
  url::Origin origin;
  if ((url.SchemeIs(url::kUrnScheme) ||
       url.SchemeIs(url::kUuidInPackageScheme)) &&
      real_url_info.origin.opaque()) {
    auto precursor = real_url_info.origin.GetTupleOrPrecursorTupleIfOpaque();
    if (precursor.IsValid()) {
      // Use the precursor as the origin. This should be the origin of the
      // bundle.
      origin = url::Origin::CreateFromNormalizedTuple(
          precursor.scheme(), precursor.host(), precursor.port());
    } else {
      origin = url::Origin::Resolve(url, real_url_info.origin);
    }
  } else {
    origin = url::Origin::Create(url);
  }

  // If the url has a host, then determine the site.  Skip file URLs to avoid a
  // situation where site URL of file://localhost/ would mismatch Blink's origin
  // (which ignores the hostname in this case - see https://crbug.com/776160).
  GURL site_url;
  if (!origin.host().empty() && origin.scheme() != url::kFileScheme) {
    // For Strict Origin Isolation, use the full origin instead of site for all
    // HTTP/HTTPS URLs.  Note that the HTTP/HTTPS restriction guarantees that
    // we won't hit this for hosted app effective URLs (see
    // https://crbug.com/961386).
    if (SiteIsolationPolicy::IsStrictOriginIsolationEnabled() &&
        origin.GetURL().SchemeIsHTTPOrHTTPS())
      return origin.GetURL();

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
            real_url_info.requests_origin_agent_cluster_isolation(), site_url,
            &isolated_origin)) {
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
      // In some cases, it is not safe to use just the scheme as a site URL, as
      // that might allow two URLs created by different sites to share a
      // process. See https://crbug.com/863623 and https://crbug.com/863069.
      //
      // TODO(alexmos,creis): This should eventually be expanded to certain
      // other schemes, such as file:.
      if (url.SchemeIsBlob() || url.scheme() == url::kDataScheme) {
        // We get here for blob URLs of form blob:null/guid.  Use the full URL
        // with the guid in that case, which isolates all blob URLs with unique
        // origins from each other.  We also get here for browser-initiated
        // navigations to data URLs, which have a unique origin and should only
        // share a process when they are identical.  Remove hash from the URL in
        // either case, since same-document navigations shouldn't use a
        // different site URL.
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

}  // namespace content
