// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_instance_impl.h"

#include <string>

#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/macros.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host_factory.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace content {

int32_t SiteInstanceImpl::next_site_instance_id_ = 1;

using CheckOriginLockResult =
    ChildProcessSecurityPolicyImpl::CheckOriginLockResult;

SiteInstanceImpl::SiteInstanceImpl(BrowsingInstance* browsing_instance)
    : id_(next_site_instance_id_++),
      active_frame_count_(0),
      browsing_instance_(browsing_instance),
      process_(nullptr),
      can_associate_with_spare_process_(true),
      has_site_(false),
      process_reuse_policy_(ProcessReusePolicy::DEFAULT),
      is_for_service_worker_(false) {
  DCHECK(browsing_instance);
}

SiteInstanceImpl::~SiteInstanceImpl() {
  GetContentClient()->browser()->SiteInstanceDeleting(this);

  if (process_)
    process_->RemoveObserver(this);

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
  return base::WrapRefCounted(
      new SiteInstanceImpl(new BrowsingInstance(browser_context)));
}

// static
scoped_refptr<SiteInstanceImpl> SiteInstanceImpl::CreateForURL(
    BrowserContext* browser_context,
    const GURL& url) {
  DCHECK(browser_context);
  // This will create a new SiteInstance and BrowsingInstance.
  scoped_refptr<BrowsingInstance> instance(
      new BrowsingInstance(browser_context));
  return instance->GetSiteInstanceForURL(url);
}

// static
bool SiteInstanceImpl::ShouldAssignSiteForURL(const GURL& url) {
  // about:blank should not "use up" a new SiteInstance.  The SiteInstance can
  // still be used for a normal web site.
  if (url == url::kAboutBlankURL)
    return false;

  // The embedder will then have the opportunity to determine if the URL
  // should "use up" the SiteInstance.
  return GetContentClient()->browser()->ShouldAssignSiteForURL(url);
}

// static
bool SiteInstanceImpl::IsOriginLockASite(const GURL& lock_url) {
  return lock_url.has_scheme() && lock_url.has_host();
}

int32_t SiteInstanceImpl::GetId() {
  return id_;
}

bool SiteInstanceImpl::HasProcess() const {
  if (process_ != nullptr)
    return true;

  // If we would use process-per-site for this site, also check if there is an
  // existing process that we would use if GetProcess() were called.
  BrowserContext* browser_context =
      browsing_instance_->browser_context();
  if (has_site_ &&
      RenderProcessHost::ShouldUseProcessPerSite(browser_context, site_) &&
      RenderProcessHostImpl::GetSoleProcessHostForSite(browser_context, site_,
                                                       lock_url_)) {
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
    BrowserContext* browser_context = browsing_instance_->browser_context();

    // Check if the ProcessReusePolicy should be updated.
    bool should_use_process_per_site =
        has_site_ &&
        RenderProcessHost::ShouldUseProcessPerSite(browser_context, site_);
    if (should_use_process_per_site) {
      process_reuse_policy_ = ProcessReusePolicy::PROCESS_PER_SITE;
    } else if (process_reuse_policy_ == ProcessReusePolicy::PROCESS_PER_SITE) {
      process_reuse_policy_ = ProcessReusePolicy::DEFAULT;
    }

    process_ = RenderProcessHostImpl::GetProcessHostForSiteInstance(this);

    CHECK(process_);
    process_->AddObserver(this);

    // If we are using process-per-site, we need to register this process
    // for the current site so that we can find it again.  (If no site is set
    // at this time, we will register it in SetSite().)
    if (process_reuse_policy_ == ProcessReusePolicy::PROCESS_PER_SITE &&
        has_site_) {
      RenderProcessHostImpl::RegisterSoleProcessHostForSite(browser_context,
                                                            process_, this);
    }

    TRACE_EVENT2("navigation", "SiteInstanceImpl::GetProcess",
                 "site id", id_, "process id", process_->GetID());
    GetContentClient()->browser()->SiteInstanceGotProcess(this);

    if (has_site_)
      LockToOriginIfNeeded();
  }
  DCHECK(process_);

  return process_;
}

bool SiteInstanceImpl::CanAssociateWithSpareProcess() {
  return can_associate_with_spare_process_;
}

void SiteInstanceImpl::PreventAssociationWithSpareProcess() {
  can_associate_with_spare_process_ = false;
}

void SiteInstanceImpl::SetSite(const GURL& url) {
  TRACE_EVENT2("navigation", "SiteInstanceImpl::SetSite",
               "site id", id_, "url", url.possibly_invalid_spec());
  // A SiteInstance's site should not change.
  // TODO(creis): When following links or script navigations, we can currently
  // render pages from other sites in this SiteInstance.  This will eventually
  // be fixed, but until then, we should still not set the site of a
  // SiteInstance more than once.
  DCHECK(!has_site_);

  // Remember that this SiteInstance has been used to load a URL, even if the
  // URL is invalid.
  has_site_ = true;
  BrowserContext* browser_context = browsing_instance_->browser_context();
  site_ =
      GetSiteForURL(browser_context, url, true /* should_use_effective_urls */);
  original_url_ = url;
  lock_url_ = DetermineProcessLockURL(browser_context, url);

  // Now that we have a site, register it with the BrowsingInstance.  This
  // ensures that we won't create another SiteInstance for this site within
  // the same BrowsingInstance, because all same-site pages within a
  // BrowsingInstance can script each other.
  browsing_instance_->RegisterSiteInstance(this);

  // Update the process reuse policy based on the site.
  bool should_use_process_per_site =
      RenderProcessHost::ShouldUseProcessPerSite(browser_context, site_);
  if (should_use_process_per_site) {
    process_reuse_policy_ = ProcessReusePolicy::PROCESS_PER_SITE;
  }

  if (process_) {
    LockToOriginIfNeeded();

    // Ensure the process is registered for this site if necessary.
    if (should_use_process_per_site) {
      RenderProcessHostImpl::RegisterSoleProcessHostForSite(browser_context,
                                                            process_, this);
    }
  }
}

const GURL& SiteInstanceImpl::GetSiteURL() const {
  return site_;
}

bool SiteInstanceImpl::HasSite() const {
  return has_site_;
}

bool SiteInstanceImpl::HasRelatedSiteInstance(const GURL& url) {
  return browsing_instance_->HasSiteInstance(url);
}

scoped_refptr<SiteInstance> SiteInstanceImpl::GetRelatedSiteInstance(
    const GURL& url) {
  return browsing_instance_->GetSiteInstanceForURL(url);
}

bool SiteInstanceImpl::IsRelatedSiteInstance(const SiteInstance* instance) {
  return browsing_instance_.get() == static_cast<const SiteInstanceImpl*>(
                                         instance)->browsing_instance_.get();
}

size_t SiteInstanceImpl::GetRelatedActiveContentsCount() {
  return browsing_instance_->active_contents_count();
}

bool SiteInstanceImpl::HasWrongProcessForURL(const GURL& url) {
  // Having no process isn't a problem, since we'll assign it correctly.
  // Note that HasProcess() may return true if process_ is null, in
  // process-per-site cases where there's an existing process available.
  // We want to use such a process in the IsSuitableHost check, so we
  // may end up assigning process_ in the GetProcess() call below.
  if (!HasProcess())
    return false;

  // If the URL to navigate to can be associated with any site instance,
  // we want to keep it in the same process.
  if (IsRendererDebugURL(url))
    return false;

  // Any process can host an about:blank URL, except the one used for error
  // pages, which should not commit successful navigations.  This check avoids a
  // process transfer for browser-initiated navigations to about:blank in a
  // dedicated process; without it, IsSuitableHost would consider this process
  // unsuitable for about:blank when it compares origin locks.
  // Renderer-initiated navigations will handle about:blank navigations
  // elsewhere and leave them in the source SiteInstance, along with
  // about:srcdoc and data:.
  if (url.IsAboutBlank() && site_ != GURL(kUnreachableWebDataURL))
    return false;

  // If the site URL is an extension (e.g., for hosted apps or WebUI) but the
  // process is not (or vice versa), make sure we notice and fix it.
  GURL site_url =
      SiteInstance::GetSiteForURL(browsing_instance_->browser_context(), url);
  GURL origin_lock =
      DetermineProcessLockURL(browsing_instance_->browser_context(), url);
  return !RenderProcessHostImpl::IsSuitableHost(
      GetProcess(), browsing_instance_->browser_context(), site_url,
      origin_lock);
}

bool SiteInstanceImpl::RequiresDedicatedProcess() {
  if (!has_site_)
    return false;

  return DoesSiteRequireDedicatedProcess(GetBrowserContext(), site_);
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

BrowserContext* SiteInstanceImpl::GetBrowserContext() const {
  return browsing_instance_->browser_context();
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
  return SiteInstanceImpl::CreateForURL(browser_context, url);
}

// static
bool SiteInstance::ShouldAssignSiteForURL(const GURL& url) {
  return SiteInstanceImpl::ShouldAssignSiteForURL(url);
}

// static
bool SiteInstance::IsSameWebSite(BrowserContext* browser_context,
                                 const GURL& real_src_url,
                                 const GURL& real_dest_url) {
  return SiteInstanceImpl::IsSameWebSite(browser_context, real_src_url,
                                         real_dest_url, true);
}

bool SiteInstanceImpl::IsSameWebSite(BrowserContext* browser_context,
                                     const GURL& real_src_url,
                                     const GURL& real_dest_url,
                                     bool should_compare_effective_urls) {
  DCHECK(browser_context);

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
  if (IsRendererDebugURL(src_url) || IsRendererDebugURL(dest_url))
    return true;

  // If either URL is invalid, they aren't part of the same site.
  if (!src_url.is_valid() || !dest_url.is_valid())
    return false;

  // If the destination url is just a blank page, we treat them as part of the
  // same site.
  GURL blank_page(url::kAboutBlankURL);
  if (dest_url == blank_page)
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
  bool src_origin_is_isolated =
      policy->GetMatchingIsolatedOrigin(src_origin, &src_isolated_origin);
  bool dest_origin_is_isolated =
      policy->GetMatchingIsolatedOrigin(dest_origin, &dest_isolated_origin);
  if (src_origin_is_isolated || dest_origin_is_isolated) {
    // Compare most specific matching origins to ensure that a subdomain of an
    // isolated origin (e.g., https://subdomain.isolated.foo.com) also matches
    // the isolated origin's site URL (e.g., https://isolated.foo.com).
    return src_isolated_origin == dest_isolated_origin;
  }

  return true;
}

// static
GURL SiteInstance::GetSiteForURL(BrowserContext* browser_context,
                                 const GURL& url) {
  // By default, GetSiteForURL will resolve |real_url| to an effective URL
  // before computing its site.
  return SiteInstanceImpl::GetSiteForURL(browser_context, url,
                                         true /* should_use_effective_urls */);
}

// static
GURL SiteInstanceImpl::DetermineProcessLockURL(BrowserContext* browser_context,
                                               const GURL& url) {
  // For the process lock URL, convert |url| to a site without resolving |url|
  // to an effective URL.
  return SiteInstanceImpl::GetSiteForURL(browser_context, url,
                                         false /* should_use_effective_urls */);
}

GURL SiteInstanceImpl::GetSiteForURL(BrowserContext* browser_context,
                                     const GURL& real_url,
                                     bool should_use_effective_urls) {
  // TODO(fsamuel, creis): For some reason appID is not recognized as a host.
  if (real_url.SchemeIs(kGuestScheme))
    return real_url;

  GURL url = should_use_effective_urls
                 ? SiteInstanceImpl::GetEffectiveURL(browser_context, real_url)
                 : real_url;
  url::Origin origin = url::Origin::Create(url);

  // If the url has a host, then determine the site.  Skip file URLs to avoid a
  // situation where site URL of file://localhost/ would mismatch Blink's origin
  // (which ignores the hostname in this case - see https://crbug.com/776160).
  if (!origin.host().empty() && origin.scheme() != url::kFileScheme) {
    GURL site_url(GetSiteForOrigin(origin));

    // Isolated origins should use the full origin as their site URL. A
    // subdomain of an isolated origin should also use that isolated origin's
    // site URL. It is important to check |origin| (based on |url|) rather than
    // |real_url| here, since some effective URLs (such as for NTP) need to be
    // resolved prior to the isolated origin lookup.
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    url::Origin isolated_origin;
    if (policy->GetMatchingIsolatedOrigin(origin, site_url, &isolated_origin))
      return isolated_origin.GetURL();

    // If an effective URL was used, augment the effective site URL with the
    // underlying web site in the hash.  This is needed to keep
    // navigations across sites covered by one hosted app in separate
    // SiteInstances.  See https://crbug.com/791796.
    //
    // TODO(https://crbug.com/734722): Consider replacing this hack with
    // a proper security principal.
    if (should_use_effective_urls && url != real_url) {
      std::string non_translated_site_url(
          GetSiteForURL(browser_context, real_url,
                        false /* should_use_effective_urls */)
              .spec());
      GURL::Replacements replacements;
      replacements.SetRefStr(non_translated_site_url.c_str());
      site_url = site_url.ReplaceComponents(replacements);
    }

    return site_url;
  }

  // If there is no host but there is a scheme, return the scheme.
  // This is useful for cases like file URLs.
  if (!origin.opaque()) {
    // Prefer to use the scheme of |origin| rather than |url|, to correctly
    // cover blob:file: and filesystem:file: URIs (see also
    // https://crbug.com/697111).
    DCHECK(!origin.scheme().empty());
    return GURL(origin.scheme() + ":");
  } else if (url.has_scheme()) {
    // In some cases, it is not safe to use just the scheme as a site URL, as
    // that might allow two URLs created by different sites to share a process.
    // See https://crbug.com/863623 and https://crbug.com/863069.
    //
    // TODO(alexmos,creis): This should eventually be expanded to certain other
    // schemes, such as file:.
    // TODO(creis): This currently causes problems with tests on Android and
    // Android WebView.  For now, skip it when Site Isolation is not enabled,
    // since there's no need to isolate data and blob URLs from each other in
    // that case.
    bool is_site_isolation_enabled =
        SiteIsolationPolicy::UseDedicatedProcessesForAllSites() ||
        SiteIsolationPolicy::AreIsolatedOriginsEnabled();
    if (is_site_isolation_enabled &&
        (url.SchemeIsBlob() || url.scheme() == url::kDataScheme)) {
      // We get here for blob URLs of form blob:null/guid.  Use the full URL
      // with the guid in that case, which isolates all blob URLs with unique
      // origins from each other.  We also get here for browser-initiated
      // navigations to data URLs, which have a unique origin and should only
      // share a process when they are identical.  Remove hash from the URL in
      // either case, since same-document navigations shouldn't use a different
      // site URL.
      if (url.has_ref()) {
        GURL::Replacements replacements;
        replacements.ClearRef();
        url = url.ReplaceComponents(replacements);
      }
      return url;
    }

    DCHECK(!url.scheme().empty());
    return GURL(url.scheme() + ":");
  }

  // Otherwise the URL should be invalid; return an empty site.
  DCHECK(!url.is_valid()) << url;
  return GURL();
}

// static
GURL SiteInstanceImpl::GetSiteForOrigin(const url::Origin& origin) {
  // Only keep the scheme and registered domain of |origin|.
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      origin.host(),
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  std::string site = origin.scheme();
  site += url::kStandardSchemeSeparator;
  site += domain.empty() ? origin.host() : domain;
  return GURL(site);
}

// static
GURL SiteInstanceImpl::GetEffectiveURL(BrowserContext* browser_context,
                                       const GURL& url) {
  return GetContentClient()->browser()->GetEffectiveURL(browser_context, url);
}

// static
bool SiteInstanceImpl::HasEffectiveURL(BrowserContext* browser_context,
                                       const GURL& url) {
  return GetEffectiveURL(browser_context, url) != url;
}

// static
bool SiteInstanceImpl::DoesSiteRequireDedicatedProcess(
    BrowserContext* browser_context,
    const GURL& url) {
  DCHECK(browser_context);

  // If --site-per-process is enabled, site isolation is enabled everywhere.
  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
    return true;

  // Always require a dedicated process for isolated origins.
  GURL site_url = SiteInstance::GetSiteForURL(browser_context, url);
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (policy->IsIsolatedOrigin(url::Origin::Create(site_url)))
    return true;

  // Error pages in main frames do require isolation, however since this is
  // missing the context whether this is for a main frame or not, that part
  // is enforced in RenderFrameHostManager.
  if (site_url.SchemeIs(kChromeErrorScheme))
    return true;

  // Isolate kChromeUIScheme pages from one another and from other kinds of
  // schemes.
  if (site_url.SchemeIs(content::kChromeUIScheme))
    return true;

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
bool SiteInstanceImpl::ShouldLockToOrigin(BrowserContext* browser_context,
                                          GURL site_url) {
  DCHECK(browser_context);

  // Don't lock to origin in --single-process mode, since this mode puts
  // cross-site pages into the same process.
  if (RenderProcessHost::run_renderer_in_process())
    return false;

  if (!DoesSiteRequireDedicatedProcess(browser_context, site_url))
    return false;

  // Guest processes cannot be locked to their site because guests always have
  // a fixed SiteInstance. The site of GURLs a guest loads doesn't match that
  // SiteInstance. So we skip locking the guest process to the site.
  // TODO(ncarter): Remove this exclusion once we can make origin lock per
  // RenderFrame routing id.
  if (site_url.SchemeIs(content::kGuestScheme))
    return false;

  // TODO(creis, nick): Until we can handle sites with effective URLs at the
  // call sites of ChildProcessSecurityPolicy::CanAccessDataForOrigin, we
  // must give the embedder a chance to exempt some sites to avoid process
  // kills.
  if (!GetContentClient()->browser()->ShouldLockToOrigin(browser_context,
                                                         site_url)) {
    return false;
  }

  return true;
}

void SiteInstanceImpl::RenderProcessHostDestroyed(RenderProcessHost* host) {
  DCHECK_EQ(process_, host);
  process_->RemoveObserver(this);
  process_ = nullptr;
}

void SiteInstanceImpl::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  for (auto& observer : observers_)
    observer.RenderProcessGone(this);
}

void SiteInstanceImpl::LockToOriginIfNeeded() {
  DCHECK(HasSite());

  // From now on, this process should be considered "tainted" for future
  // process reuse decisions:
  // (1) If |site_| required a dedicated process, this SiteInstance's process
  //     can only host URLs for the same site.
  // (2) Even if |site_| does not require a dedicated process, this
  //     SiteInstance's process still cannot be reused to host other sites
  //     requiring dedicated sites in the future.
  // We can get here either when we commit a URL into a SiteInstance that does
  // not yet have a site, or when we create a process for a SiteInstance with a
  // preassigned site.
  process_->SetIsUsed();

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  auto lock_state = policy->CheckOriginLock(process_->GetID(), lock_url());
  if (ShouldLockToOrigin(GetBrowserContext(), site_)) {
    // Sanity check that this won't try to assign an origin lock to a <webview>
    // process, which can't be locked.
    CHECK(!process_->IsForGuestsOnly());

    switch (lock_state) {
      case CheckOriginLockResult::NO_LOCK: {
        // TODO(nick): When all sites are isolated, this operation provides
        // strong protection. If only some sites are isolated, we need
        // additional logic to prevent the non-isolated sites from requesting
        // resources for isolated sites. https://crbug.com/509125
        TRACE_EVENT2("navigation", "SiteInstanceImpl::LockToOrigin", "site id",
                     id_, "lock", lock_url().possibly_invalid_spec());
        process_->LockToOrigin(lock_url());
        break;
      }
      case CheckOriginLockResult::HAS_WRONG_LOCK:
        // We should never attempt to reassign a different origin lock to a
        // process.
        base::debug::SetCrashKeyString(bad_message::GetRequestedSiteURLKey(),
                                       site_.spec());
        base::debug::SetCrashKeyString(
            bad_message::GetKilledProcessOriginLockKey(),
            policy->GetOriginLock(process_->GetID()).spec());
        CHECK(false) << "Trying to lock a process to " << lock_url()
                     << " but the process is already locked to "
                     << policy->GetOriginLock(process_->GetID());
        break;
      case CheckOriginLockResult::HAS_EQUAL_LOCK:
        // Process already has the right origin lock assigned.  This case will
        // happen for commits to |site_| after the first one.
        break;
      default:
        NOTREACHED();
    }
  } else {
    // If the site that we've just committed doesn't require a dedicated
    // process, make sure we aren't putting it in a process for a site that
    // does.
    if (lock_state != CheckOriginLockResult::NO_LOCK) {
      base::debug::SetCrashKeyString(bad_message::GetRequestedSiteURLKey(),
                                     site_.spec());
      base::debug::SetCrashKeyString(
          bad_message::GetKilledProcessOriginLockKey(),
          policy->GetOriginLock(process_->GetID()).spec());
      CHECK(false) << "Trying to commit non-isolated site " << site_
                   << " in process locked to "
                   << policy->GetOriginLock(process_->GetID());
    }
  }
}

}  // namespace content
