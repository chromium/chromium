// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_instance_impl.h"

#include <string>
#include <tuple>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "content/browser/bad_message.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/isolated_origin_util.h"
#include "content/browser/isolation_context.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/public/browser/browser_or_resource_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host_factory.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "url/origin.h"
#include "url/url_constants.h"

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

GURL SchemeAndHostToSite(const std::string& scheme, const std::string& host) {
  return GURL(scheme + url::kStandardSchemeSeparator + host);
}

// Constant used to mark two call sites that must always agree on whether
// the default SiteInstance is allowed.
constexpr bool kCreateForURLAllowsDefaultSiteInstance = true;

// URL used for the site URL and lock URL in error page SiteInfo objects.
GURL GetErrorPageSiteAndLockURL() {
  return GURL(kUnreachableWebDataURL);
}

}  // namespace

int32_t SiteInstanceImpl::next_site_instance_id_ = 1;

// static
const GURL& SiteInstanceImpl::GetDefaultSiteURL() {
  struct DefaultSiteURL {
    const GURL url = GURL("http://unisolated.invalid");
  };
  static base::LazyInstance<DefaultSiteURL>::Leaky default_site_url =
      LAZY_INSTANCE_INITIALIZER;

  return default_site_url.Get().url;
}

// static
SiteInfo SiteInfo::CreateForErrorPage(
    const WebExposedIsolationInfo& web_exposed_isolation_info) {
  return SiteInfo(GetErrorPageSiteAndLockURL(), GetErrorPageSiteAndLockURL(),
                  false /* is_origin_keyed */, web_exposed_isolation_info,
                  false /* is_guest */,
                  false /* does_site_request_dedicated_process_for_coop */,
                  false /* is_jit_disabled */);
}

// static
SiteInfo SiteInfo::CreateForDefaultSiteInstance(
    BrowserContext* browser_context,
    const WebExposedIsolationInfo& web_exposed_isolation_info) {
  // Get default JIT policy for this browser_context by passing in an empty
  // site_url.
  bool is_jit_disabled = GetContentClient()->browser()->IsJitDisabledForSite(
      browser_context, GURL());

  return SiteInfo(SiteInstanceImpl::GetDefaultSiteURL(),
                  SiteInstanceImpl::GetDefaultSiteURL(),
                  false /* is_origin_keyed */, web_exposed_isolation_info,
                  false /* is_guest */,
                  false /* does_site_request_dedicated_process_for_coop */,
                  is_jit_disabled);
}

// static
SiteInfo SiteInfo::CreateForGuest(BrowserContext* browser_context,
                                  const GURL& guest_site_url) {
  // Setting site and lock directly without the site URL conversions we
  // do for user provided URLs. Callers expect GetSiteURL() to return the
  // value they provide in |guest_site_url|.
  return SiteInfo(guest_site_url, guest_site_url, false /* is_origin_keyed */,
                  WebExposedIsolationInfo::CreateNonIsolated(),
                  true /* is_guest */,
                  false /* does_site_request_dedicated_process_for_coop */,
                  false /* is_jit_disabled */);
}

// static
SiteInfo SiteInfo::Create(
    const IsolationContext& isolation_context,
    const UrlInfo& url_info,
    const WebExposedIsolationInfo& web_exposed_isolation_info) {
  // The call to GetSiteForURL() below is only allowed on the UI thread, due to
  // its possible use of effective urls.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return CreateInternal(isolation_context, url_info, web_exposed_isolation_info,
                        /*compute_site_url=*/true);
}

// static
SiteInfo SiteInfo::CreateOnIOThread(
    const IsolationContext& isolation_context,
    const UrlInfo& url_info,
    const WebExposedIsolationInfo& web_exposed_isolation_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return CreateInternal(isolation_context, url_info, web_exposed_isolation_info,
                        /*compute_site_url=*/false);
}

// static
SiteInfo SiteInfo::CreateInternal(
    const IsolationContext& isolation_context,
    const UrlInfo& url_info,
    const WebExposedIsolationInfo& web_exposed_isolation_info,
    bool compute_site_url) {
  if (url_info.url.SchemeIs(kChromeErrorScheme))
    return CreateForErrorPage(web_exposed_isolation_info);

  GURL lock_url = DetermineProcessLockURL(isolation_context, url_info);
  GURL site_url = lock_url;
  bool is_jitless = false;
  if (compute_site_url) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    site_url = GetSiteForURLInternal(isolation_context, url_info,
                                     true /* should_use_effective_urls */);

    is_jitless = GetContentClient()->browser()->IsJitDisabledForSite(
        isolation_context.browser_or_resource_context().ToBrowserContext(),
        lock_url);
  }
  bool is_origin_keyed =
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
                  web_exposed_isolation_info, false /* is_guest */,
                  does_site_request_dedicated_process_for_coop, is_jitless);
}

// static
SiteInfo SiteInfo::CreateForTesting(const IsolationContext& isolation_context,
                                    const GURL& url) {
  return Create(isolation_context, UrlInfo::CreateForTesting(url),
                WebExposedIsolationInfo::CreateNonIsolated());
}

SiteInfo::SiteInfo() = default;
SiteInfo::SiteInfo(const SiteInfo& rhs) = default;

SiteInfo::~SiteInfo() = default;

SiteInfo::SiteInfo(const GURL& site_url,
                   const GURL& process_lock_url,
                   bool is_origin_keyed,
                   const WebExposedIsolationInfo& web_exposed_isolation_info,
                   bool is_guest,
                   bool does_site_request_dedicated_process_for_coop,
                   bool is_jit_disabled)
    : site_url_(site_url),
      process_lock_url_(process_lock_url),
      is_origin_keyed_(is_origin_keyed),
      web_exposed_isolation_info_(web_exposed_isolation_info),
      is_guest_(is_guest),
      does_site_request_dedicated_process_for_coop_(
          does_site_request_dedicated_process_for_coop),
      is_jit_disabled_(is_jit_disabled) {}

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
                  site_info.web_exposed_isolation_info_, site_info.is_guest_,
                  site_info.is_jit_disabled_);
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
      web_exposed_isolation_info_ == other.web_exposed_isolation_info_ &&
      is_guest_ == other.is_guest_ &&
      does_site_request_dedicated_process_for_coop_ ==
          other.does_site_request_dedicated_process_for_coop_ &&
      is_jit_disabled_ == other.is_jit_disabled_;

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

  return GetContentClient()->browser()->GetStoragePartitionIdForSite(
      browser_context, site_url());
}

StoragePartitionConfig SiteInfo::GetStoragePartitionConfig(
    BrowserContext* browser_context) const {
  return GetStoragePartitionConfigForUrl(browser_context, site_url(),
                                         /*is_site_url=*/true);
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

  // Navigations to urn: URLs served from Web Bundles [1] require special care
  // to use the origin of the bundle rather than the urn: URL, which lacks any
  // origin information.
  // [1] bit.ly/subresource-web-bundles-doc
  // TODO(acolwell): Update this so we can use url::Origin::Resolve() for all
  // cases.
  url::Origin origin;
  if (url.SchemeIs(url::kUrnScheme) && real_url_info.origin.opaque()) {
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

class SiteInstanceImpl::DefaultSiteInstanceState {
 public:
  void AddSiteInfo(const SiteInfo& site_info) {
    default_site_url_set_.insert(site_info.site_url());
  }

  bool ContainsSite(const GURL& site_url) {
    return base::Contains(default_site_url_set_, site_url);
  }

 private:
  // Keeps track of the site URLs that have been mapped to the default
  // SiteInstance.
  // TODO(wjmaclean): Revise this to store SiteInfos instead of GURLs.
  std::set<GURL> default_site_url_set_;
};

SiteInstanceImpl::SiteInstanceImpl(BrowsingInstance* browsing_instance)
    : id_(next_site_instance_id_++),
      active_frame_count_(0),
      browsing_instance_(browsing_instance),
      process_(nullptr),
      agent_scheduling_group_(nullptr),
      can_associate_with_spare_process_(true),
      has_site_(false),
      process_reuse_policy_(ProcessReusePolicy::DEFAULT),
      is_for_service_worker_(false),
      process_assignment_(SiteInstanceProcessAssignment::UNKNOWN) {
  DCHECK(browsing_instance);
}

SiteInstanceImpl::~SiteInstanceImpl() {
  GetContentClient()->browser()->SiteInstanceDeleting(this);

  if (process_) {
    process_->RemoveObserver(this);

    // Ensure the RenderProcessHost gets deleted if this SiteInstance created a
    // process which was never used by any listeners.
    process_->Cleanup();
  }

  // Now that no one is referencing us, we can safely remove ourselves from
  // the BrowsingInstance.  Any future visits to a page from this site
  // (within the same BrowsingInstance) can safely create a new SiteInstance.
  if (has_site_)
    browsing_instance_->UnregisterSiteInstance(this);
}

// static
scoped_refptr<SiteInstanceImpl> SiteInstanceImpl::Create(
    BrowserContext* browser_context) {
  DCHECK(browser_context);
  return base::WrapRefCounted(new SiteInstanceImpl(new BrowsingInstance(
      browser_context, WebExposedIsolationInfo::CreateNonIsolated())));
}

// static
scoped_refptr<SiteInstanceImpl> SiteInstanceImpl::CreateForUrlInfo(
    BrowserContext* browser_context,
    const UrlInfo& url_info,
    const WebExposedIsolationInfo& web_exposed_isolation_info) {
  DCHECK(browser_context);
  // This will create a new SiteInstance and BrowsingInstance.
  scoped_refptr<BrowsingInstance> instance(
      new BrowsingInstance(browser_context, web_exposed_isolation_info));

  // Note: The |allow_default_instance| value used here MUST match the value
  // used in DoesSiteForURLMatch().
  return instance->GetSiteInstanceForURL(
      url_info, kCreateForURLAllowsDefaultSiteInstance);
}

// static
scoped_refptr<SiteInstanceImpl> SiteInstanceImpl::CreateForServiceWorker(
    BrowserContext* browser_context,
    const GURL& url,
    const WebExposedIsolationInfo& web_exposed_isolation_info,
    bool can_reuse_process,
    bool is_guest) {
  DCHECK(!url.SchemeIs(kChromeErrorScheme));
  scoped_refptr<SiteInstanceImpl> site_instance;

  if (is_guest) {
    site_instance = CreateForGuest(browser_context, url);
  } else {
    // This will create a new SiteInstance and BrowsingInstance.
    scoped_refptr<BrowsingInstance> instance(
        new BrowsingInstance(browser_context, web_exposed_isolation_info));

    // We do NOT want to allow the default site instance here because workers
    // need to be kept separate from other sites.
    site_instance = instance->GetSiteInstanceForURL(
        UrlInfo(url, UrlInfo::OriginIsolationRequest::kNone),
        /* allow_default_instance */ false);
  }
  DCHECK(!site_instance->GetSiteInfo().is_error_page());
  site_instance->is_for_service_worker_ = true;

  // Attempt to reuse a renderer process if possible. Note that in the
  // <webview> case, process reuse isn't currently supported and a new
  // process will always be created (https://crbug.com/752667).
  DCHECK(site_instance->process_reuse_policy() ==
             SiteInstanceImpl::ProcessReusePolicy::DEFAULT ||
         site_instance->process_reuse_policy() ==
             SiteInstanceImpl::ProcessReusePolicy::PROCESS_PER_SITE);
  if (can_reuse_process) {
    site_instance->set_process_reuse_policy(
        SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  }
  return site_instance;
}

// static
scoped_refptr<SiteInstanceImpl> SiteInstanceImpl::CreateForGuest(
    content::BrowserContext* browser_context,
    const GURL& guest_site_url) {
  DCHECK(browser_context);
  DCHECK_NE(guest_site_url, GetDefaultSiteURL());

  auto guest_site_info =
      SiteInfo::CreateForGuest(browser_context, guest_site_url);
  scoped_refptr<SiteInstanceImpl> site_instance =
      base::WrapRefCounted(new SiteInstanceImpl(new BrowsingInstance(
          browser_context, guest_site_info.web_exposed_isolation_info())));

  site_instance->SetSiteInfoInternal(guest_site_info);

  return site_instance;
}

// static
scoped_refptr<SiteInstanceImpl>
SiteInstanceImpl::CreateReusableInstanceForTesting(
    BrowserContext* browser_context,
    const GURL& url) {
  DCHECK(browser_context);
  // This will create a new SiteInstance and BrowsingInstance.
  scoped_refptr<BrowsingInstance> instance(new BrowsingInstance(
      browser_context, WebExposedIsolationInfo::CreateNonIsolated()));
  auto site_instance = instance->GetSiteInstanceForURL(
      UrlInfo(url, UrlInfo::OriginIsolationRequest::kNone),
      /* allow_default_instance */ false);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  return site_instance;
}

// static
scoped_refptr<SiteInstanceImpl> SiteInstanceImpl::CreateForTesting(
    BrowserContext* browser_context,
    const GURL& url) {
  DCHECK(browser_context);
  return SiteInstanceImpl::CreateForUrlInfo(
      browser_context, UrlInfo::CreateForTesting(url),
      WebExposedIsolationInfo::CreateNonIsolated());
}

// static
bool SiteInstanceImpl::ShouldAssignSiteForURL(const GURL& url) {
  // about:blank should not "use up" a new SiteInstance.  The SiteInstance can
  // still be used for a normal web site.
  if (url.IsAboutBlank())
    return false;

  // The embedder will then have the opportunity to determine if the URL
  // should "use up" the SiteInstance.
  return GetContentClient()->browser()->ShouldAssignSiteForURL(url);
}

int32_t SiteInstanceImpl::GetId() {
  return id_;
}

int32_t SiteInstanceImpl::GetBrowsingInstanceId() {
  // This is being vended out as an opaque ID, and it is always defined for
  // a BrowsingInstance affiliated IsolationContext, so it's safe to call
  // "GetUnsafeValue" and expose the inner value directly.
  return browsing_instance_->isolation_context()
      .browsing_instance_id()
      .GetUnsafeValue();
}

const IsolationContext& SiteInstanceImpl::GetIsolationContext() {
  return browsing_instance_->isolation_context();
}

RenderProcessHost* SiteInstanceImpl::GetSiteInstanceGroupProcessIfAvailable() {
  return browsing_instance_->site_instance_group_manager()
      .GetExistingGroupProcess(this);
}

bool SiteInstanceImpl::IsDefaultSiteInstance() const {
  return default_site_instance_state_ != nullptr;
}

void SiteInstanceImpl::AddSiteInfoToDefault(const SiteInfo& site_info) {
  DCHECK(IsDefaultSiteInstance());
  default_site_instance_state_->AddSiteInfo(site_info);
}

bool SiteInstanceImpl::IsSiteInDefaultSiteInstance(const GURL& site_url) const {
  DCHECK(IsDefaultSiteInstance());
  return default_site_instance_state_->ContainsSite(site_url);
}

// static
BrowsingInstanceId SiteInstanceImpl::NextBrowsingInstanceId() {
  return BrowsingInstance::NextBrowsingInstanceId();
}

bool SiteInstanceImpl::HasProcess() {
  if (process_ != nullptr)
    return true;

  // If we would use process-per-site for this site, also check if there is an
  // existing process that we would use if GetProcess() were called.
  if (ShouldUseProcessPerSite() &&
      RenderProcessHostImpl::GetSoleProcessHostForSite(GetIsolationContext(),
                                                       site_info_)) {
    return true;
  }

  return false;
}

RenderProcessHost* SiteInstanceImpl::GetProcess() {
  // TODO(erikkay) It would be nice to ensure that the renderer type had been
  // properly set before we get here.  The default tab creation case winds up
  // with no site set at this point, so it will default to TYPE_NORMAL.  This
  // may not be correct, so we'll wind up potentially creating a process that
  // we then throw away, or worse sharing a process with the wrong process type.
  // See crbug.com/43448.

  // Create a new process if ours went away or was reused.
  if (!process_) {
    // Check if the ProcessReusePolicy should be updated.
    if (ShouldUseProcessPerSite()) {
      process_reuse_policy_ = ProcessReusePolicy::PROCESS_PER_SITE;
    } else if (process_reuse_policy_ == ProcessReusePolicy::PROCESS_PER_SITE) {
      process_reuse_policy_ = ProcessReusePolicy::DEFAULT;
    }

    SetProcessInternal(
        RenderProcessHostImpl::GetProcessHostForSiteInstance(this));
  }
  DCHECK(process_);

  return process_;
}

AgentSchedulingGroupHost& SiteInstanceImpl::GetAgentSchedulingGroup() {
  if (!agent_scheduling_group_) {
    // If an AgentSchedulingGroup has not yet been assigned, we need to have it
    // assigned (along with a RenderProcessHost). To preserve the invariant that
    // |process_| and |agent_scheduling_group_| are always changed together, we
    // call GetProcess(), and assume that it will set both members.
    GetProcess();
  }

  DCHECK(agent_scheduling_group_);
  DCHECK_EQ(agent_scheduling_group_->GetProcess(), process_);

  return *agent_scheduling_group_;
}

bool SiteInstanceImpl::ShouldUseProcessPerSite() const {
  BrowserContext* browser_context = browsing_instance_->GetBrowserContext();
  return has_site_ && site_info_.ShouldUseProcessPerSite(browser_context);
}

void SiteInstanceImpl::ReuseCurrentProcessIfPossible(
    RenderProcessHost* current_process) {
  DCHECK(!IsGuest());
  if (HasProcess())
    return;

  // We should not reuse the current process if the destination uses
  // process-per-site. Note that this includes the case where the process for
  // the site is not there yet (so we're going to create a new process).
  // Note also that this does not apply for the reverse case: if the current
  // process is used for a process-per-site site, it is ok to reuse this for the
  // new page (regardless of the site).
  if (ShouldUseProcessPerSite())
    return;

  // Do not reuse the process if it's not suitable for this SiteInstance. For
  // example, this won't allow reusing a process if it's locked to a site that's
  // different from this SiteInstance's site.
  if (!RenderProcessHostImpl::MayReuseAndIsSuitable(current_process, this))
    return;

  // TODO(crbug.com/1055779 ): Don't try to reuse process if either of the
  // SiteInstances are cross-origin isolated (uses COOP/COEP).
  SetProcessInternal(current_process);
}

void SiteInstanceImpl::SetProcessInternal(RenderProcessHost* process) {
  //  It is never safe to change |process_| without going through
  //  RenderProcessHostDestroyed first to set it to null. Otherwise, same-site
  //  frames will end up in different processes and everything will get
  //  confused.
  CHECK(!process_);
  CHECK(process);
  process_ = process;
  process_->AddObserver(this);
  CHECK(!agent_scheduling_group_);
  agent_scheduling_group_ =
      AgentSchedulingGroupHost::GetOrCreate(*this, *process_);

  LockProcessIfNeeded();

  // If we are using process-per-site, we need to register this process
  // for the current site so that we can find it again.  (If no site is set
  // at this time, we will register it in SetSite().)
  if (process_reuse_policy_ == ProcessReusePolicy::PROCESS_PER_SITE &&
      has_site_) {
    RenderProcessHostImpl::RegisterSoleProcessHostForSite(process_, this);
  }

  TRACE_EVENT2("navigation", "SiteInstanceImpl::SetProcessInternal", "site id",
               id_, "process id", process_->GetID());
  GetContentClient()->browser()->SiteInstanceGotProcess(this);

  // Notify SiteInstanceGroupManager that the process was set on this
  // SiteInstance. This must be called after LockProcessIfNeeded() because
  // the SiteInstanceGroupManager does suitability checks that use the lock.
  browsing_instance_->site_instance_group_manager().OnProcessSet(this);
}

bool SiteInstanceImpl::CanAssociateWithSpareProcess() {
  return can_associate_with_spare_process_;
}

void SiteInstanceImpl::PreventAssociationWithSpareProcess() {
  can_associate_with_spare_process_ = false;
}

void SiteInstanceImpl::SetSite(const UrlInfo& url_info) {
  const GURL& url = url_info.url;
  // TODO(creis): Consider calling ShouldAssignSiteForURL internally, rather
  // than before multiple call sites.  See https://crbug.com/949220.
  TRACE_EVENT2("navigation", "SiteInstanceImpl::SetSite", "site id", id_, "url",
               url.possibly_invalid_spec());
  // A SiteInstance's site should not change.
  // TODO(creis): When following links or script navigations, we can currently
  // render pages from other sites in this SiteInstance.  This will eventually
  // be fixed, but until then, we should still not set the site of a
  // SiteInstance more than once.
  DCHECK(!has_site_);

  original_url_ = url;
  // Convert |url| into an appropriate SiteInfo that can be passed to
  // SetSiteInfoInternal(). We must do this transformation for any arbitrary
  // URL we get from a user, a navigation, or script.
  SetSiteInfoInternal(browsing_instance_->GetSiteInfoForURL(
      url_info, /* allow_default_instance */ false));
}

void SiteInstanceImpl::SetSiteInfoToDefault() {
  TRACE_EVENT1("navigation", "SiteInstanceImpl::SetSiteInfoToDefault",
               "site id", id_);
  DCHECK(!has_site_);
  default_site_instance_state_ = std::make_unique<DefaultSiteInstanceState>();
  original_url_ = GetDefaultSiteURL();
  SetSiteInfoInternal(SiteInfo::CreateForDefaultSiteInstance(
      GetBrowserContext(), browsing_instance_->web_exposed_isolation_info()));
}

void SiteInstanceImpl::SetSiteInfoInternal(const SiteInfo& site_info) {
  // TODO(acolwell): Add logic to validate |site_url| and |lock_url| are valid.
  DCHECK(!has_site_);
  CHECK_EQ(site_info.web_exposed_isolation_info(),
           browsing_instance_->web_exposed_isolation_info());

  if (verify_storage_partition_info_) {
    auto* browser_context = browsing_instance_->GetBrowserContext();
    auto old_partition_id = site_info_.GetStoragePartitionId(browser_context);
    auto old_partition_config =
        site_info_.GetStoragePartitionConfig(browser_context);
    auto new_partition_id = site_info.GetStoragePartitionId(browser_context);
    auto new_partition_config =
        site_info.GetStoragePartitionConfig(browser_context);
    CHECK_EQ(old_partition_id, new_partition_id);
    CHECK_EQ(old_partition_config, new_partition_config);
  }
  // Remember that this SiteInstance has been used to load a URL, even if the
  // URL is invalid.
  has_site_ = true;
  site_info_ = site_info;

  // Now that we have a site, register it with the BrowsingInstance.  This
  // ensures that we won't create another SiteInstance for this site within
  // the same BrowsingInstance, because all same-site pages within a
  // BrowsingInstance can script each other.
  browsing_instance_->RegisterSiteInstance(this);

  if (site_info_.is_origin_keyed()) {
    // Track this origin's isolation in the current BrowsingInstance.  This is
    // needed to consistently isolate future navigations to this origin in this
    // BrowsingInstance, even if its opt-in status changes later.
    ChildProcessSecurityPolicyImpl* policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
    url::Origin origin(url::Origin::Create(site_info_.process_lock_url()));
    // This is one of two places that origins can be marked as opted-in, the
    // other is
    // NavigationRequest::AddSameProcessOriginAgentClusterOptInIfNecessary().
    // This site handles the case where OAC isolation gets a separate process.
    // In future, when SiteInstance Groups are complete, this may revert to
    // being the only call site.
    policy->AddIsolatedOriginForBrowsingInstance(
        browsing_instance_->isolation_context(), origin,
        true /* is_origin_keyed */,
        ChildProcessSecurityPolicy::IsolatedOriginSource::WEB_TRIGGERED);
  }

  if (site_info_.does_site_request_dedicated_process_for_coop()) {
    // If there was a request to process-isolate `site_info_` from COOP
    // headers, notify ChildProcessSecurityPolicy about the new isolated origin
    // in the current BrowsingInstance.  Note that we must convert the origin
    // to a site to avoid breaking document.domain.  Typically, the process
    // lock URL would already correspond to a site (since we isolate sites, not
    // origins, by default), but this isn't always the case.  For example, this
    // SiteInstance could be isolated with the origin granularity due to
    // Origin-Agent-Cluster (see site_info_.is_origin_keyed() above).
    url::Origin origin(url::Origin::Create(site_info_.process_lock_url()));
    GURL site(SiteInfo::GetSiteForOrigin(origin));
    ChildProcessSecurityPolicyImpl* policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
    policy->AddIsolatedOriginForBrowsingInstance(
        browsing_instance_->isolation_context(), url::Origin::Create(site),
        false /* is_origin_keyed */,
        ChildProcessSecurityPolicy::IsolatedOriginSource::WEB_TRIGGERED);
  }

  // Update the process reuse policy based on the site.
  bool should_use_process_per_site = ShouldUseProcessPerSite();
  if (should_use_process_per_site)
    process_reuse_policy_ = ProcessReusePolicy::PROCESS_PER_SITE;

  if (process_) {
    LockProcessIfNeeded();

    // Ensure the process is registered for this site if necessary.
    if (should_use_process_per_site)
      RenderProcessHostImpl::RegisterSoleProcessHostForSite(process_, this);
  }

  // Notify SiteInstanceGroupManager that the SiteInfo was set on this
  // SiteInstance. This must be called after LockProcessIfNeeded() because
  // the SiteInstanceGroupManager does suitability checks that use the lock.
  browsing_instance_->site_instance_group_manager().OnSiteInfoSet(
      this, process_ != nullptr);
}

void SiteInstanceImpl::ConvertToDefaultOrSetSite(const UrlInfo& url_info) {
  DCHECK(!has_site_);

  if (!browsing_instance_->HasDefaultSiteInstance()) {
    const SiteInfo site_info = SiteInfo::Create(GetIsolationContext(), url_info,
                                                GetWebExposedIsolationInfo());
    if (CanBePlacedInDefaultSiteInstance(GetIsolationContext(), url_info.url,
                                         site_info)) {
      SetSiteInfoToDefault();
      AddSiteInfoToDefault(site_info);

      DCHECK(browsing_instance_->HasDefaultSiteInstance());
      return;
    }
  }

  SetSite(url_info);
}

SiteInstanceProcessAssignment
SiteInstanceImpl::GetLastProcessAssignmentOutcome() {
  return process_assignment_;
}

const GURL& SiteInstanceImpl::GetSiteURL() {
  return site_info_.site_url();
}

const SiteInfo& SiteInstanceImpl::GetSiteInfo() {
  return site_info_;
}

const SiteInfo& SiteInstanceImpl::GetSiteInfoForRenderViewHost() {
  if (!has_site_) {
    // Note: `site_info_` has not been set yet. When the RenderViewHost uses
    // this SiteInfo to generate a partition ID it will be using an empty
    // SiteInfo. This is ok as long as the ID does not change when `site_info_`
    // is actually set. Enable the verification code in SetSiteInfoInternal() to
    // verify that the partition info does not change.
    verify_storage_partition_info_ = true;
  }

  return site_info_;
}

SiteInfo SiteInstanceImpl::DeriveSiteInfo(const UrlInfo& url_info,
                                          bool is_related) {
  if (IsGuest()) {
    // Guests currently must stay in the same SiteInstance no matter what the
    // information in |url_info| so we return the current SiteInfo.
    return site_info_;
  }

  if (is_related) {
    return browsing_instance_->GetSiteInfoForURL(
        url_info, /* allow_default_instance */ true);
  }

  return SiteInfo::Create(GetIsolationContext(), url_info,
                          GetWebExposedIsolationInfo());
}

const ProcessLock SiteInstanceImpl::GetProcessLock() const {
  return ProcessLock(site_info_);
}

bool SiteInstanceImpl::HasSite() const {
  return has_site_;
}

bool SiteInstanceImpl::HasRelatedSiteInstance(const SiteInfo& site_info) {
  return browsing_instance_->HasSiteInstance(site_info);
}

scoped_refptr<SiteInstance> SiteInstanceImpl::GetRelatedSiteInstance(
    const GURL& url) {
  return GetRelatedSiteInstanceImpl(
      UrlInfo(url, UrlInfo::OriginIsolationRequest::kNone));
}

scoped_refptr<SiteInstanceImpl> SiteInstanceImpl::GetRelatedSiteInstanceImpl(
    const UrlInfo& url_info) {
  return browsing_instance_->GetSiteInstanceForURL(
      url_info, /* allow_default_instance */ true);
}

bool SiteInstanceImpl::IsRelatedSiteInstance(const SiteInstance* instance) {
  return browsing_instance_.get() ==
         static_cast<const SiteInstanceImpl*>(instance)
             ->browsing_instance_.get();
}

size_t SiteInstanceImpl::GetRelatedActiveContentsCount() {
  return browsing_instance_->active_contents_count();
}

bool SiteInstanceImpl::IsSuitableForUrlInfo(const UrlInfo& url_info) {
  const GURL& url = url_info.url;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If the URL to navigate to can be associated with any site instance,
  // we want to keep it in the same process.
  if (blink::IsRendererDebugURL(url))
    return true;

  // Any process can host an about:blank URL, except the one used for error
  // pages, which should not commit successful navigations.  This check avoids a
  // process transfer for browser-initiated navigations to about:blank in a
  // dedicated process; without it, IsSuitableHost would consider this process
  // unsuitable for about:blank when it compares process locks.
  // Renderer-initiated navigations will handle about:blank navigations
  // elsewhere and leave them in the source SiteInstance, along with
  // about:srcdoc and data:.
  if (url.IsAboutBlank() && !site_info_.is_error_page())
    return true;

  // If the site URL is an extension (e.g., for hosted apps or WebUI) but the
  // process is not (or vice versa), make sure we notice and fix it.

  // Note: This call must return information that is identical to what
  // would be reported in the SiteInstance returned by
  // GetRelatedSiteInstance(url).
  SiteInfo site_info = DeriveSiteInfo(url_info, /* is_related= */ true);

  // If this is a default SiteInstance and the BrowsingInstance gives us a
  // non-default SiteInfo even when we explicitly allow the default SiteInstance
  // to be considered, then |url| does not belong in the same process as this
  // SiteInstance. This can happen when the
  // kProcessSharingWithDefaultSiteInstances feature is not enabled and the
  // site URL is explicitly set on a SiteInstance for a URL that would normally
  // be directed to the default SiteInstance (e.g. a site not requiring a
  // dedicated process). This situation typically happens when the top-level
  // frame is a site that should be in the default SiteInstance and the
  // SiteInstance associated with that frame is initially a SiteInstance with
  // no site URL set.
  if (IsDefaultSiteInstance() && site_info != site_info_)
    return false;

  // Note that HasProcess() may return true if process_ is null, in
  // process-per-site cases where there's an existing process available.
  // We want to use such a process in the IsSuitableHost check, so we
  // may end up assigning process_ in the GetProcess() call below.
  if (!HasProcess()) {
    // If there is no process or site, then this is a new SiteInstance that can
    // be used for anything.
    if (!HasSite())
      return true;

    // If there is no process but there is a site, then the process must have
    // been discarded after we navigated away.  If the SiteInfos match, then it
    // is safe to use this SiteInstance unless it is a guest. Guests are a
    // special case because we need to be consistent with the HasProcess() path
    // and the IsSuitableHost() call below always returns false for guests.
    if (site_info_ == site_info)
      return !IsGuest();

    // If the site URLs do not match, but neither this SiteInstance nor the
    // destination site_url require dedicated processes, then it is safe to use
    // this SiteInstance.
    if (!RequiresDedicatedProcess() &&
        !site_info.RequiresDedicatedProcess(GetIsolationContext())) {
      return true;
    }

    // Otherwise, there's no process, the SiteInfos don't match, and at least
    // one of them requires a dedicated process, so it is not safe to use this
    // SiteInstance.
    return false;
  }

  return RenderProcessHostImpl::IsSuitableHost(
      GetProcess(), GetIsolationContext(), site_info);
}

bool SiteInstanceImpl::RequiresDedicatedProcess() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!has_site_)
    return false;

  return site_info_.RequiresDedicatedProcess(GetIsolationContext());
}

void SiteInstanceImpl::IncrementActiveFrameCount() {
  active_frame_count_++;
}

void SiteInstanceImpl::DecrementActiveFrameCount() {
  if (--active_frame_count_ == 0) {
    for (auto& observer : observers_)
      observer.ActiveFrameCountIsZero(this);
  }
}

void SiteInstanceImpl::IncrementRelatedActiveContentsCount() {
  browsing_instance_->increment_active_contents_count();
}

void SiteInstanceImpl::DecrementRelatedActiveContentsCount() {
  browsing_instance_->decrement_active_contents_count();
}

void SiteInstanceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SiteInstanceImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

BrowserContext* SiteInstanceImpl::GetBrowserContext() {
  return browsing_instance_->GetBrowserContext();
}

// static
scoped_refptr<SiteInstance> SiteInstance::Create(
    BrowserContext* browser_context) {
  DCHECK(browser_context);
  return SiteInstanceImpl::Create(browser_context);
}

// static
scoped_refptr<SiteInstance> SiteInstance::CreateForURL(
    BrowserContext* browser_context,
    const GURL& url) {
  DCHECK(browser_context);
  return SiteInstanceImpl::CreateForUrlInfo(
      browser_context, UrlInfo(url, UrlInfo::OriginIsolationRequest::kNone),
      WebExposedIsolationInfo::CreateNonIsolated());
}

// static
scoped_refptr<SiteInstance> SiteInstance::CreateForGuest(
    content::BrowserContext* browser_context,
    const GURL& guest_site_url) {
  DCHECK(browser_context);
  return SiteInstanceImpl::CreateForGuest(browser_context, guest_site_url);
}

// static
bool SiteInstance::ShouldAssignSiteForURL(const GURL& url) {
  return SiteInstanceImpl::ShouldAssignSiteForURL(url);
}

bool SiteInstanceImpl::IsSameSiteWithURL(const GURL& url) {
  return IsSameSiteWithURLInfo(
      UrlInfo(url, UrlInfo::OriginIsolationRequest::kNone));
}

bool SiteInstanceImpl::IsSameSiteWithURLInfo(const UrlInfo& url_info) {
  const GURL& url = url_info.url;
  if (IsDefaultSiteInstance()) {
    // about:blank URLs should always be considered same site just like they are
    // in IsSameSite().
    if (url.IsAboutBlank())
      return true;

    // Consider |url| the same site if it could be handled by the
    // default SiteInstance and we don't already have a SiteInstance for
    // this URL.
    // TODO(acolwell): Remove HasSiteInstance() call once we have a way to
    // prevent SiteInstances with no site URL from being used for URLs
    // that should be routed to the default SiteInstance.
    DCHECK_EQ(site_info_.site_url(), GetDefaultSiteURL());
    auto site_info = SiteInfo::Create(GetIsolationContext(), url_info,
                                      GetWebExposedIsolationInfo());
    return CanBePlacedInDefaultSiteInstance(GetIsolationContext(), url,
                                            site_info) &&
           !browsing_instance_->HasSiteInstance(site_info);
  }

  return SiteInstanceImpl::IsSameSite(
      GetIsolationContext(),
      UrlInfo(site_info_.site_url(), UrlInfo::OriginIsolationRequest::kNone),
      url_info, true /* should_compare_effective_urls */);
}

bool SiteInstanceImpl::IsGuest() {
  return site_info_.is_guest();
}

bool SiteInstanceImpl::IsJitDisabled() {
  return site_info_.is_jit_disabled();
}

std::string SiteInstanceImpl::GetPartitionDomain(
    StoragePartitionImpl* storage_partition) {
  auto storage_partition_config =
      site_info_.GetStoragePartitionConfig(GetBrowserContext());

  // The DCHECK here is to allow the trybots to detect any attempt to introduce
  // new code that violates this assumption.
  DCHECK_EQ(storage_partition->GetPartitionDomain(),
            storage_partition_config.partition_domain());

  if (storage_partition->GetPartitionDomain() !=
      storage_partition_config.partition_domain()) {
    // Trigger crash logging if we encounter a case that violates our
    // assumptions.
    SCOPED_CRASH_KEY_STRING256("GetPartitionDomain", "domain",
                               storage_partition->GetPartitionDomain());
    SCOPED_CRASH_KEY_STRING256("GetPartitionDomain", "config_domain_key",
                               storage_partition_config.partition_domain());

    base::debug::DumpWithoutCrashing();

    // Return the value from the config to preserve legacy behavior until we
    // can land a fix.
    return storage_partition_config.partition_domain();
  }
  return storage_partition->GetPartitionDomain();
}

bool SiteInstanceImpl::IsOriginalUrlSameSite(
    const UrlInfo& dest_url_info,
    bool should_compare_effective_urls) {
  if (IsDefaultSiteInstance())
    return IsSameSiteWithURLInfo(dest_url_info);

  // Here we use an |origin_isolation_request| of kNone when converting
  // |original_url_| to UrlInfo, since (i) the isolation status of this
  // SiteInstance was determined at the time |original_url_| was set, and in
  // this case it is |dest_url_info| that is currently navigating, and that's
  // where the current isolation request (if any) is stored. Whether or not
  // this SiteInstance has origin isolation is a separate question, and not
  // what the UrlInfo for |original_url_| is supposed to reflect.
  return IsSameSite(
      GetIsolationContext(),
      UrlInfo(original_url_, UrlInfo::OriginIsolationRequest::kNone),
      dest_url_info, should_compare_effective_urls);
}

bool SiteInstanceImpl::IsNavigationSameSite(
    const GURL& last_successful_url,
    const url::Origin last_committed_origin,
    bool for_main_frame,
    const UrlInfo& dest_url_info) {
  const GURL& dest_url = dest_url_info.url;
  BrowserContext* browser_context = GetBrowserContext();

  // Ask embedder whether effective URLs should be used when determining if
  // |dest_url| should end up in this SiteInstance.
  // This is used to keep same-site scripting working for hosted apps.
  bool should_compare_effective_urls =
      IsDefaultSiteInstance() ||
      GetContentClient()
          ->browser()
          ->ShouldCompareEffectiveURLsForSiteInstanceSelection(
              browser_context, this, for_main_frame, original_url(), dest_url);

  bool src_has_effective_url = !IsDefaultSiteInstance() &&
                               HasEffectiveURL(browser_context, original_url());
  bool dest_has_effective_url = HasEffectiveURL(browser_context, dest_url);

  // If IsSuitableForURL finds a process type mismatch, return false
  // even if |dest_url| is same-site.  (The URL may have been installed as an
  // app since the last time we visited it.)
  //
  // This check must be skipped to keep same-site subframe navigations from a
  // hosted app to non-hosted app, and vice versa, in the same process.
  // Otherwise, this would return false due to a process privilege level
  // mismatch.
  bool should_check_for_wrong_process =
      should_compare_effective_urls ||
      (!src_has_effective_url && !dest_has_effective_url);
  if (should_check_for_wrong_process && !IsSuitableForUrlInfo(dest_url_info))
    return false;

  // If we don't have a last successful URL, we can't trust the origin or URL
  // stored on the frame, so we fall back to the SiteInstance URL.  This case
  // matters for newly created frames which haven't committed a navigation yet,
  // as well as for net errors. Note that we use the SiteInstance's
  // original_url() and not the site URL, so that we can do this comparison
  // without the effective URL resolution if needed.
  if (last_successful_url.is_empty())
    return IsOriginalUrlSameSite(dest_url_info, should_compare_effective_urls);

  // In the common case, we use the last successful URL. Thus, we compare
  // against the last successful commit when deciding whether to swap this time.
  // We convert |last_successful_url| to UrlInfo with
  // |origin_isolation_request| set to kNone since it isn't currently
  // navigating.
  if (IsSameSite(
          GetIsolationContext(),
          UrlInfo(last_successful_url, UrlInfo::OriginIsolationRequest::kNone),
          dest_url_info, should_compare_effective_urls)) {
    return true;
  }

  // It is possible that last_successful_url was a nonstandard scheme (for
  // example, "about:blank"). If so, examine the last committed origin to
  // determine the site.
  // Similar to above, convert |last_committed_origin| to UrlInfo with
  // |origin_isolation_request| set to kNone.
  if (!last_committed_origin.opaque() &&
      IsSameSite(GetIsolationContext(),
                 UrlInfo(GURL(last_committed_origin.Serialize()),
                         UrlInfo::OriginIsolationRequest::kNone),
                 dest_url_info, should_compare_effective_urls)) {
    return true;
  }

  // If the last successful URL was "about:blank" with a unique origin (which
  // implies that it was a browser-initiated navigation to "about:blank"), none
  // of the cases above apply, but we should still allow a scenario like
  // foo.com -> about:blank -> foo.com to be treated as same-site, as some
  // tests rely on that behavior.  To accomplish this, compare |dest_url|
  // against the site URL.
  if (last_successful_url.IsAboutBlank() && last_committed_origin.opaque() &&
      IsOriginalUrlSameSite(dest_url_info, should_compare_effective_urls)) {
    return true;
  }

  // Not same-site.
  return false;
}

// static
bool SiteInstanceImpl::IsSameSite(const IsolationContext& isolation_context,
                                  const UrlInfo& real_src_url_info,
                                  const UrlInfo& real_dest_url_info,
                                  bool should_compare_effective_urls) {
  const GURL& real_src_url = real_src_url_info.url;
  const GURL& real_dest_url = real_dest_url_info.url;

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserContext* browser_context =
      isolation_context.browser_or_resource_context().ToBrowserContext();
  DCHECK(browser_context);
  DCHECK_NE(real_src_url, GetDefaultSiteURL());

  GURL src_url =
      should_compare_effective_urls
          ? SiteInstanceImpl::GetEffectiveURL(browser_context, real_src_url)
          : real_src_url;
  GURL dest_url =
      should_compare_effective_urls
          ? SiteInstanceImpl::GetEffectiveURL(browser_context, real_dest_url)
          : real_dest_url;

  // We infer web site boundaries based on the registered domain name of the
  // top-level page and the scheme.  We do not pay attention to the port if
  // one is present, because pages served from different ports can still
  // access each other if they change their document.domain variable.

  // Some special URLs will match the site instance of any other URL. This is
  // done before checking both of them for validity, since we want these URLs
  // to have the same site instance as even an invalid one.
  if (blink::IsRendererDebugURL(src_url) || blink::IsRendererDebugURL(dest_url))
    return true;

  // If either URL is invalid, they aren't part of the same site.
  if (!src_url.is_valid() || !dest_url.is_valid())
    return false;

  // If the destination url is just a blank page, we treat them as part of the
  // same site.
  if (dest_url.IsAboutBlank())
    return true;

  // If the source and destination URLs are equal excluding the hash, they have
  // the same site.  This matters for file URLs, where SameDomainOrHost() would
  // otherwise return false below.
  if (src_url.EqualsIgnoringRef(dest_url))
    return true;

  url::Origin src_origin = url::Origin::Create(src_url);
  url::Origin dest_origin = url::Origin::Create(dest_url);

  // If the schemes differ, they aren't part of the same site.
  if (src_origin.scheme() != dest_origin.scheme())
    return false;

  if (SiteIsolationPolicy::IsStrictOriginIsolationEnabled())
    return src_origin == dest_origin;

  if (!net::registry_controlled_domains::SameDomainOrHost(
          src_origin, dest_origin,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    return false;
  }

  // If the sites are the same, check isolated origins.  If either URL matches
  // an isolated origin, compare origins rather than sites.  As an optimization
  // to avoid unneeded isolated origin lookups, shortcut this check if the two
  // origins are the same.
  if (src_origin == dest_origin)
    return true;
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  url::Origin src_isolated_origin;
  url::Origin dest_isolated_origin;
  bool src_origin_is_isolated = policy->GetMatchingProcessIsolatedOrigin(
      isolation_context, src_origin,
      real_src_url_info.requests_origin_agent_cluster_isolation(),
      &src_isolated_origin);
  bool dest_origin_is_isolated = policy->GetMatchingProcessIsolatedOrigin(
      isolation_context, dest_origin,
      real_dest_url_info.requests_origin_agent_cluster_isolation(),
      &dest_isolated_origin);
  if (src_origin_is_isolated || dest_origin_is_isolated) {
    // Compare most specific matching origins to ensure that a subdomain of an
    // isolated origin (e.g., https://subdomain.isolated.foo.com) also matches
    // the isolated origin's site URL (e.g., https://isolated.foo.com).
    return src_isolated_origin == dest_isolated_origin;
  }

  return true;
}

bool SiteInstanceImpl::DoesSiteInfoForURLMatch(const UrlInfo& url_info) {
  // TODO(acolwell, ahemery): Update callers to pass in COOP/COEP info into
  // this method. The code is currently safe because the caller checks to make
  // sure the COOP/COEP info matches on this object before calling this method.
  auto site_info = SiteInfo::Create(GetIsolationContext(), url_info,
                                    GetWebExposedIsolationInfo());
  if (kCreateForURLAllowsDefaultSiteInstance &&
      CanBePlacedInDefaultSiteInstance(GetIsolationContext(), url_info.url,
                                       site_info)) {
    site_info = SiteInfo::CreateForDefaultSiteInstance(
        GetBrowserContext(), GetWebExposedIsolationInfo());
  }

  return site_info_.IsExactMatch(site_info);
}

void SiteInstanceImpl::PreventOptInOriginIsolation(
    const url::Origin& previously_visited_origin) {
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddNonIsolatedOriginIfNeeded(
      GetIsolationContext(), previously_visited_origin,
      true /* is_global_walk_or_frame_removal */);
}

// static
bool SiteInstanceImpl::CanBePlacedInDefaultSiteInstance(
    const IsolationContext& isolation_context,
    const GURL& url,
    const SiteInfo& site_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(
          features::kProcessSharingWithDefaultSiteInstances)) {
    return false;
  }

  // Exclude "file://" URLs from the default SiteInstance to prevent the
  // default SiteInstance process from accumulating file access grants that
  // could be exploited by other non-isolated sites.
  if (url.SchemeIs(url::kFileScheme))
    return false;

  // Don't use the default SiteInstance when
  // kProcessSharingWithStrictSiteInstances is enabled because we want each
  // site to have its own SiteInstance object and logic elsewhere ensures
  // that those SiteInstances share a process.
  if (base::FeatureList::IsEnabled(
          features::kProcessSharingWithStrictSiteInstances)) {
    return false;
  }

  // Don't use the default SiteInstance when SiteInstance doesn't assign a
  // site URL for |url|, since in that case the SiteInstance should remain
  // unused, and a subsequent navigation should always be able to reuse it,
  // whether or not it's to a site requiring a dedicated process or to a site
  // that will use the default SiteInstance.
  if (!ShouldAssignSiteForURL(url))
    return false;

  // Allow the default SiteInstance to be used for sites that don't need to be
  // isolated in their own process.
  return !site_info.RequiresDedicatedProcess(isolation_context);
}

// static
GURL SiteInstanceImpl::GetEffectiveURL(BrowserContext* browser_context,
                                       const GURL& url) {
  DCHECK(browser_context);
  return GetContentClient()->browser()->GetEffectiveURL(browser_context, url);
}

// static
bool SiteInstanceImpl::HasEffectiveURL(BrowserContext* browser_context,
                                       const GURL& url) {
  return GetEffectiveURL(browser_context, url) != url;
}

void SiteInstanceImpl::RenderProcessHostDestroyed(RenderProcessHost* host) {
  DCHECK_EQ(process_, host);
  process_->RemoveObserver(this);
  process_ = nullptr;
  agent_scheduling_group_ = nullptr;

  // Protect this from being deleted inside of the observers.
  scoped_refptr<SiteInstanceImpl> protect(this);

  for (auto& observer : observers_)
    observer.RenderProcessHostDestroyed();
}

void SiteInstanceImpl::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  for (auto& observer : observers_)
    observer.RenderProcessGone(this, info);
}

void SiteInstanceImpl::LockProcessIfNeeded() {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  ProcessLock process_lock = policy->GetProcessLock(process_->GetID());
  if (!has_site_) {
    CHECK(!process_lock.is_locked_to_site())
        << "A process that's already locked to " << process_lock.ToString()
        << " cannot be updated to a more permissive lock";
    // Update the process lock state to signal that the process has been
    // associated with a SiteInstance that is not locked to a site yet.  Note
    // that even if the process lock is already set to a lock that allows any
    // site, we still need to notify ChildProcessSecurityPolicy about the
    // current SiteInstance's IsolationContext, so that the corresponding
    // BrowsingInstance can be associated with |process_|.  See
    // https://crbug.com/1135539.
    if (process_lock.is_invalid()) {
      auto new_process_lock =
          ProcessLock::CreateAllowAnySite(GetWebExposedIsolationInfo());
      process_->SetProcessLock(GetIsolationContext(), new_process_lock);
    } else {
      CHECK(process_lock.allows_any_site())
          << "Unexpected process lock " << process_lock.ToString();
      policy->IncludeIsolationContext(process_->GetID(), GetIsolationContext());
    }
    return;
  }

  DCHECK(HasSite());

  // From now on, this process should be considered "tainted" for future
  // process reuse decisions:
  // (1) If |site_info_| required a dedicated process, this SiteInstance's
  //     process can only host URLs for the same site.
  // (2) Even if |site_info_| does not require a dedicated process, this
  //     SiteInstance's process still cannot be reused to host other sites
  //     requiring dedicated sites in the future.
  // We can get here either when we commit a URL into a SiteInstance that does
  // not yet have a site, or when we create a process for a SiteInstance with a
  // preassigned site.
  process_->SetIsUsed();

  if (site_info_.ShouldLockProcessToSite(GetIsolationContext())) {
    // Sanity check that this won't try to assign an origin lock to a <webview>
    // process, which can't be locked.
    CHECK(!process_->IsForGuestsOnly());

    ProcessLock lock_to_set = GetProcessLock();
    if (!process_lock.is_locked_to_site()) {
      // TODO(nick): When all sites are isolated, this operation provides
      // strong protection. If only some sites are isolated, we need
      // additional logic to prevent the non-isolated sites from requesting
      // resources for isolated sites. https://crbug.com/509125
      TRACE_EVENT2("navigation", "RenderProcessHost::SetProcessLock", "site id",
                   id_, "lock", lock_to_set.ToString());
      process_->SetProcessLock(GetIsolationContext(), lock_to_set);
    } else if (process_lock != lock_to_set) {
      // We should never attempt to reassign a different origin lock to a
      // process.
      base::debug::SetCrashKeyString(bad_message::GetRequestedSiteInfoKey(),
                                     site_info_.GetDebugString());
      policy->LogKilledProcessOriginLock(process_->GetID());
      CHECK(false) << "Trying to lock a process to " << lock_to_set.ToString()
                   << " but the process is already locked to "
                   << process_lock.ToString();
    } else {
      // Process already has the right origin lock assigned.  This case will
      // happen for commits to |site_info_| after the first one.
    }
  } else {
    if (process_lock.is_locked_to_site()) {
      // The site that we're committing doesn't require a dedicated
      // process, but it has been put in a process for a site that does.
      base::debug::SetCrashKeyString(bad_message::GetRequestedSiteInfoKey(),
                                     site_info_.GetDebugString());
      policy->LogKilledProcessOriginLock(process_->GetID());
      CHECK(false) << "Trying to commit non-isolated site " << site_info_
                   << " in process locked to " << process_lock.ToString();
    } else if (process_lock.is_invalid()) {
      // Update the process lock state to signal that the process has been
      // associated with a SiteInstance that is not locked to a site yet.
      auto new_process_lock =
          ProcessLock::CreateAllowAnySite(GetWebExposedIsolationInfo());
      process_->SetProcessLock(GetIsolationContext(), new_process_lock);
    } else {
      CHECK(process_lock.allows_any_site())
          << "Unexpected process lock " << process_lock.ToString();
    }
  }

  // Track which isolation contexts use the given process.  This lets
  // ChildProcessSecurityPolicyImpl (e.g. CanAccessDataForOrigin) determine
  // whether a given URL should require a lock or not (a dynamically isolated
  // origin may require a lock in some isolation contexts but not in others).
  policy->IncludeIsolationContext(process_->GetID(), GetIsolationContext());
}

const WebExposedIsolationInfo& SiteInstanceImpl::GetWebExposedIsolationInfo()
    const {
  return browsing_instance_->web_exposed_isolation_info();
}

bool SiteInstanceImpl::IsCrossOriginIsolated() const {
  return GetWebExposedIsolationInfo().is_isolated();
}

// static
void SiteInstance::StartIsolatingSite(
    BrowserContext* context,
    const GURL& url,
    ChildProcessSecurityPolicy::IsolatedOriginSource source,
    bool should_persist) {
  if (!SiteIsolationPolicy::AreDynamicIsolatedOriginsEnabled())
    return;

  // Ignore attempts to isolate origins that are not supported.  Do this here
  // instead of relying on AddFutureIsolatedOrigins()'s internal validation, to
  // avoid the runtime warning generated by the latter.
  url::Origin origin(url::Origin::Create(url));
  if (!IsolatedOriginUtil::IsValidIsolatedOrigin(origin))
    return;

  // Convert |url| to a site, to avoid breaking document.domain.  Note that
  // this doesn't use effective URL resolution or other special cases from
  // GetSiteForURL() and simply converts |origin| to a scheme and eTLD+1.
  GURL site(SiteInfo::GetSiteForOrigin(origin));

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  url::Origin site_origin(url::Origin::Create(site));
  policy->AddFutureIsolatedOrigins({site_origin}, source, context);

  // This function currently assumes the new isolated site should persist
  // across restarts, so ask the embedder to save it, excluding off-the-record
  // profiles.
  if (!context->IsOffTheRecord() && should_persist) {
    GetContentClient()->browser()->PersistIsolatedOrigin(context, site_origin,
                                                         source);
  }
}

void SiteInstanceImpl::WriteIntoTrace(perfetto::TracedValue context) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("id", GetId());
  dict.Add("browsing_instance_id", GetBrowsingInstanceId());
  dict.Add("is_default", IsDefaultSiteInstance());
  dict.Add("site_info", site_info_);
  dict.Add("active_frame_count", active_frame_count_);
}

}  // namespace content
