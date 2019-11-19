// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_instance_impl.h"

#include <string>

#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/isolated_origin_util.h"
#include "content/browser/isolation_context.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/webui/url_data_manager_backend.h"
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

namespace content {

namespace {

GURL SchemeAndHostToSite(const std::string& scheme, const std::string& host) {
  return GURL(scheme + url::kStandardSchemeSeparator + host);
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

  // Note: The |allow_default_instance| value used here MUST match the value
  // used in DoesSiteForURLMatch().
  return instance->GetSiteInstanceForURL(url,
                                         true /* allow_default_instance */);
}

// static
scoped_refptr<SiteInstanceImpl> SiteInstanceImpl::CreateForServiceWorker(
    BrowserContext* browser_context,
    const GURL& url,
    bool can_reuse_process) {
  // This will create a new SiteInstance and BrowsingInstance.
  scoped_refptr<BrowsingInstance> instance(
      new BrowsingInstance(browser_context));

  // We do NOT want to allow the default site instance here because workers
  // need to be kept separate from other sites.
  scoped_refptr<SiteInstanceImpl> site_instance =
      instance->GetSiteInstanceForURL(url, /* allow_default_instance */ false);
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
scoped_refptr<SiteInstanceImpl>
SiteInstanceImpl::CreateReusableInstanceForTesting(
    BrowserContext* browser_context,
    const GURL& url) {
  DCHECK(browser_context);
  // This will create a new SiteInstance and BrowsingInstance.
  scoped_refptr<BrowsingInstance> instance(
      new BrowsingInstance(browser_context));
  auto site_instance =
      instance->GetSiteInstanceForURL(url,
                                      /* allow_default_instance */ false);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  return site_instance;
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

// static
bool SiteInstanceImpl::IsOriginLockASite(const GURL& lock_url) {
  return lock_url.has_scheme() && lock_url.has_host();
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

RenderProcessHost* SiteInstanceImpl::GetDefaultProcessIfUsable() {
  if (!base::FeatureList::IsEnabled(
          features::kProcessSharingWithStrictSiteInstances)) {
    return nullptr;
  }
  if (RequiresDedicatedProcess())
    return nullptr;
  return browsing_instance_->default_process();
}

bool SiteInstanceImpl::IsDefaultSiteInstance() const {
  return browsing_instance_->IsDefaultSiteInstance(this);
}

bool SiteInstanceImpl::IsSiteInDefaultSiteInstance(const GURL& site_url) const {
  return browsing_instance_->IsSiteInDefaultSiteInstance(site_url);
}

void SiteInstanceImpl::MaybeSetBrowsingInstanceDefaultProcess() {
  if (!base::FeatureList::IsEnabled(
          features::kProcessSharingWithStrictSiteInstances)) {
    return;
  }
  // Wait until this SiteInstance both has a site and a process
  // assigned, so that we can be sure that RequiresDedicatedProcess()
  // is accurate and we actually have a process to set.
  if (!process_ || !has_site_ || RequiresDedicatedProcess())
    return;
  if (browsing_instance_->default_process()) {
    DCHECK_EQ(process_, browsing_instance_->default_process());
    return;
  }
  browsing_instance_->SetDefaultProcess(process_);
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
  BrowserContext* browser_context = browsing_instance_->GetBrowserContext();
  if (has_site_ &&
      RenderProcessHost::ShouldUseProcessPerSite(browser_context, site_) &&
      RenderProcessHostImpl::GetSoleProcessHostForSite(
          browser_context, GetIsolationContext(), site_, lock_url_)) {
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
    BrowserContext* browser_context = browsing_instance_->GetBrowserContext();

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

    MaybeSetBrowsingInstanceDefaultProcess();

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
  // TODO(creis): Consider calling ShouldAssignSiteForURL internally, rather
  // than before multiple call sites.  See https://crbug.com/949220.
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
  BrowserContext* browser_context = browsing_instance_->GetBrowserContext();
  original_url_ = url;
  browsing_instance_->GetSiteAndLockForURL(
      url, /* allow_default_instance */ false, &site_, &lock_url_);

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
    MaybeSetBrowsingInstanceDefaultProcess();
  }
}

void SiteInstanceImpl::ConvertToDefaultOrSetSite(const GURL& url) {
  DCHECK(!has_site_);

  if (browsing_instance_->TrySettingDefaultSiteInstance(this, url))
    return;

  SetSite(url);
}

const GURL& SiteInstanceImpl::GetSiteURL() {
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
  return browsing_instance_->GetSiteInstanceForURL(
      url, /* allow_default_instance */ true);
}

bool SiteInstanceImpl::IsRelatedSiteInstance(const SiteInstance* instance) {
  return browsing_instance_.get() == static_cast<const SiteInstanceImpl*>(
                                         instance)->browsing_instance_.get();
}

size_t SiteInstanceImpl::GetRelatedActiveContentsCount() {
  return browsing_instance_->active_contents_count();
}

bool SiteInstanceImpl::IsSuitableForURL(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If the URL to navigate to can be associated with any site instance,
  // we want to keep it in the same process.
  if (IsRendererDebugURL(url))
    return true;

  // Any process can host an about:blank URL, except the one used for error
  // pages, which should not commit successful navigations.  This check avoids a
  // process transfer for browser-initiated navigations to about:blank in a
  // dedicated process; without it, IsSuitableHost would consider this process
  // unsuitable for about:blank when it compares origin locks.
  // Renderer-initiated navigations will handle about:blank navigations
  // elsewhere and leave them in the source SiteInstance, along with
  // about:srcdoc and data:.
  if (url.IsAboutBlank() && site_ != GURL(kUnreachableWebDataURL))
    return true;

  // If the site URL is an extension (e.g., for hosted apps or WebUI) but the
  // process is not (or vice versa), make sure we notice and fix it.
  GURL site_url;
  GURL origin_lock;

  // Note: This call must return information that is identical to what
  // would be reported in the SiteInstance returned by
  // GetRelatedSiteInstance(url).
  browsing_instance_->GetSiteAndLockForURL(
      url, /* allow_default_instance */ true, &site_url, &origin_lock);

  // If this is a default SiteInstance and the BrowsingInstance gives us a
  // non-default site URL even when we explicitly allow the default SiteInstance
  // to be considered, then |url| does not belong in the same process as this
  // SiteInstance. This can happen when the
  // kProcessSharingWithDefaultSiteInstances feature is not enabled and the
  // site URL is explicitly set on a SiteInstance for a URL that would normally
  // be directed to the default SiteInstance (e.g. a site not requiring a
  // dedicated process). This situation typically happens when the top-level
  // frame is a site that should be in the default SiteInstance and the
  // SiteInstance associated with that frame is initially a SiteInstance with
  // no site URL set.
  if (IsDefaultSiteInstance() && site_url != GetSiteURL())
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
    // been discarded after we navigated away.  If the site URLs match, then it
    // is safe to use this SiteInstance.
    if (GetSiteURL() == site_url)
      return true;

    // If the site URLs do not match, but neither this SiteInstance nor the
    // destination site_url require dedicated processes, then it is safe to use
    // this SiteInstance.
    if (!RequiresDedicatedProcess() &&
        !DoesSiteURLRequireDedicatedProcess(GetIsolationContext(), site_url)) {
      return true;
    }

    // Otherwise, there's no process, the site URLs don't match, and at least
    // one of them requires a dedicated process, so it is not safe to use this
    // SiteInstance.
    return false;
  }

  return RenderProcessHostImpl::IsSuitableHost(
      GetProcess(), browsing_instance_->GetBrowserContext(),
      GetIsolationContext(), site_url, origin_lock);
}

bool SiteInstanceImpl::RequiresDedicatedProcess() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!has_site_)
    return false;

  return DoesSiteURLRequireDedicatedProcess(GetIsolationContext(), site_);
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
  return SiteInstanceImpl::CreateForURL(browser_context, url);
}

// static
bool SiteInstance::ShouldAssignSiteForURL(const GURL& url) {
  return SiteInstanceImpl::ShouldAssignSiteForURL(url);
}

bool SiteInstanceImpl::IsSameSiteWithURL(const GURL& url) {
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
    DCHECK_EQ(site_, GetDefaultSiteURL());
    return site_ == GetSiteForURLInternal(GetIsolationContext(), url,
                                          true /* should_use_effective_urls */,
                                          true /* allow_default_site_url */) &&
           !browsing_instance_->HasSiteInstance(url);
  }

  return SiteInstanceImpl::IsSameSite(GetIsolationContext(), site_, url,
                                      true /* should_compare_effective_urls */);
}

bool SiteInstanceImpl::IsOriginalUrlSameSite(
    const GURL& dest_url,
    bool should_compare_effective_urls) {
  if (IsDefaultSiteInstance())
    return IsSameSiteWithURL(dest_url);

  return IsSameSite(GetIsolationContext(), original_url_, dest_url,
                    should_compare_effective_urls);
}

// static
bool SiteInstanceImpl::IsSameSite(const IsolationContext& isolation_context,
                                  const GURL& real_src_url,
                                  const GURL& real_dest_url,
                                  bool should_compare_effective_urls) {
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
  if (IsRendererDebugURL(src_url) || IsRendererDebugURL(dest_url))
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
  bool src_origin_is_isolated = policy->GetMatchingIsolatedOrigin(
      isolation_context, src_origin, &src_isolated_origin);
  bool dest_origin_is_isolated = policy->GetMatchingIsolatedOrigin(
      isolation_context, dest_origin, &dest_isolated_origin);
  if (src_origin_is_isolated || dest_origin_is_isolated) {
    // Compare most specific matching origins to ensure that a subdomain of an
    // isolated origin (e.g., https://subdomain.isolated.foo.com) also matches
    // the isolated origin's site URL (e.g., https://isolated.foo.com).
    return src_isolated_origin == dest_isolated_origin;
  }

  return true;
}

bool SiteInstanceImpl::DoesSiteForURLMatch(const GURL& url) {
  // Note: The |allow_default_site_url| value used here MUST match the value
  // used in CreateForURL().
  return site_ == GetSiteForURLInternal(GetIsolationContext(), url,
                                        true /* should_use_effective_urls */,
                                        true /* allow_default_site_url */);
}

// static
GURL SiteInstance::GetSiteForURL(BrowserContext* browser_context,
                                 const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);

  // By default, GetSiteForURL will resolve |url| to an effective URL
  // before computing its site.
  //
  // TODO(alexmos): Callers inside content/ should already be using the
  // internal SiteInstanceImpl version and providing a proper IsolationContext.
  // For callers outside content/, plumb the applicable IsolationContext here,
  // where needed.  Eventually, GetSiteForURL should always require an
  // IsolationContext to be passed in, and this implementation should just
  // become SiteInstanceImpl::GetSiteForURL.
  return SiteInstanceImpl::GetSiteForURL(IsolationContext(browser_context),
                                         url);
}

// static
GURL SiteInstanceImpl::DetermineProcessLockURL(
    const IsolationContext& isolation_context,
    const GURL& url) {
  // For the process lock URL, convert |url| to a site without resolving |url|
  // to an effective URL.
  return SiteInstanceImpl::GetSiteForURLInternal(
      isolation_context, url, false /* should_use_effective_urls */,
      false /* allow_default_site_url */);
}

// static
GURL SiteInstanceImpl::GetSiteForURL(const IsolationContext& isolation_context,
                                     const GURL& real_url) {
  return GetSiteForURLInternal(isolation_context, real_url,
                               true /* should_use_effective_urls */,
                               false /* allow_default_site_url */);
}

// static
GURL SiteInstanceImpl::GetSiteForURLInternal(
    const IsolationContext& isolation_context,
    const GURL& real_url,
    bool should_use_effective_urls,
    bool allow_default_site_url) {
  // TODO(fsamuel, creis): For some reason appID is not recognized as a host.
  if (real_url.SchemeIs(kGuestScheme))
    return real_url;

  // Explicitly group chrome-error: URLs based on their host component.
  // These URLs are special because we want to group them like other URLs
  // with a host even though they are considered "no access" and
  // generate an opaque origin.
  if (real_url.SchemeIs(kChromeErrorScheme))
    return SchemeAndHostToSite(real_url.scheme(), real_url.host());

  if (should_use_effective_urls)
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GURL url = should_use_effective_urls
                 ? SiteInstanceImpl::GetEffectiveURL(
                       isolation_context.browser_or_resource_context()
                           .ToBrowserContext(),
                       real_url)
                 : real_url;
  url::Origin origin = url::Origin::Create(url);

  // If the url has a host, then determine the site.  Skip file URLs to avoid a
  // situation where site URL of file://localhost/ would mismatch Blink's origin
  // (which ignores the hostname in this case - see https://crbug.com/776160).
  GURL site_url;
  if (!origin.host().empty() && origin.scheme() != url::kFileScheme) {
    // For Strict Origin Isolation, use the full origin instead of site for all
    // HTTP/HTTPS URLs.  Note that the HTTP/HTTPS restriction guarantees that
    // we won't hit this for hosted app effective URLs, which would otherwise
    // need to append a non-translated site URL to the hash below (see
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
    if (policy->GetMatchingIsolatedOrigin(isolation_context, origin, site_url,
                                          &isolated_origin))
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
          GetSiteForURLInternal(isolation_context, real_url,
                                false /* should_use_effective_urls */,
                                allow_default_site_url)
              .spec());
      GURL::Replacements replacements;
      replacements.SetRefStr(non_translated_site_url.c_str());
      site_url = site_url.ReplaceComponents(replacements);
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

  if (allow_default_site_url &&
      CanBePlacedInDefaultSiteInstance(isolation_context, url, site_url)) {
    return GetDefaultSiteURL();
  }
  return site_url;
}

// static
bool SiteInstanceImpl::CanBePlacedInDefaultSiteInstance(
    const IsolationContext& isolation_context,
    const GURL& url,
    const GURL& site_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(
          features::kProcessSharingWithDefaultSiteInstances)) {
    return false;
  }

  // Exclude "chrome-guest:" URLs from the default SiteInstance to ensure that
  // guest specific process selection, process swapping, and storage partition
  // behavior is preserved.
  if (url.SchemeIs(kGuestScheme))
    return false;

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
  return !DoesSiteURLRequireDedicatedProcess(isolation_context, site_url);
}

// static
GURL SiteInstanceImpl::GetSiteForOrigin(const url::Origin& origin) {
  // Only keep the scheme and registered domain of |origin|.
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      origin.host(),
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return SchemeAndHostToSite(origin.scheme(),
                             domain.empty() ? origin.host() : domain);
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

// static
bool SiteInstanceImpl::DoesSiteRequireDedicatedProcess(
    const IsolationContext& isolation_context,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return SiteIsolationPolicy::UseDedicatedProcessesForAllSites() ||
         DoesSiteURLRequireDedicatedProcess(
             isolation_context,
             SiteInstanceImpl::GetSiteForURL(isolation_context, url));
}

// static
bool SiteInstanceImpl::DoesSiteURLRequireDedicatedProcess(
    const IsolationContext& isolation_context,
    const GURL& site_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(isolation_context.browser_or_resource_context());

  // If --site-per-process is enabled, site isolation is enabled everywhere.
  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
    return true;

  // Always require a dedicated process for isolated origins.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (policy->IsIsolatedOrigin(isolation_context,
                               url::Origin::Create(site_url)))
    return true;

  // Error pages in main frames do require isolation, however since this is
  // missing the context whether this is for a main frame or not, that part
  // is enforced in RenderFrameHostManager.
  if (site_url.SchemeIs(kChromeErrorScheme))
    return true;

  // Isolate WebUI pages from one another and from other kinds of schemes.
  for (const auto& webui_scheme : URLDataManagerBackend::GetWebUISchemes()) {
    if (site_url.SchemeIs(webui_scheme))
      return true;
  }

  // Let the content embedder enable site isolation for specific URLs. Use the
  // canonical site url for this check, so that schemes with nested origins
  // (blob and filesystem) work properly.
  if (GetContentClient()->browser()->DoesSiteRequireDedicatedProcess(
          isolation_context.browser_or_resource_context().ToBrowserContext(),
          site_url)) {
    return true;
  }

  return false;
}

// static
bool SiteInstanceImpl::ShouldLockToOrigin(
    const IsolationContext& isolation_context,
    GURL site_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserContext* browser_context =
      isolation_context.browser_or_resource_context().ToBrowserContext();
  DCHECK(browser_context);

  // Don't lock to origin in --single-process mode, since this mode puts
  // cross-site pages into the same process.
  if (RenderProcessHost::run_renderer_in_process())
    return false;

  if (!DoesSiteURLRequireDedicatedProcess(isolation_context, site_url))
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

// static
base::Optional<url::Origin> SiteInstanceImpl::GetRequestInitiatorSiteLock(
    GURL site_url) {
  // The following schemes are safe for sites that require a process lock:
  // - data: - locking |request_initiator| to an opaque origin
  // - http/https - requiring |request_initiator| to match |site_url| with
  //   DomainIs (i.e. suffix-based) comparison.
  if (site_url.SchemeIsHTTPOrHTTPS() || site_url.SchemeIs(url::kDataScheme))
    return url::Origin::Create(site_url);

  // Other schemes might not be safe to use as |request_initiator_site_lock|.
  // One example is chrome-guest://...
  return base::nullopt;
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
    observer.RenderProcessGone(this, info);
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
  GURL process_lock = policy->GetOriginLock(process_->GetID());
  if (ShouldLockToOrigin(GetIsolationContext(), site_)) {
    // Sanity check that this won't try to assign an origin lock to a <webview>
    // process, which can't be locked.
    CHECK(!process_->IsForGuestsOnly());

    if (process_lock.is_empty()) {
      // TODO(nick): When all sites are isolated, this operation provides
      // strong protection. If only some sites are isolated, we need
      // additional logic to prevent the non-isolated sites from requesting
      // resources for isolated sites. https://crbug.com/509125
      TRACE_EVENT2("navigation", "SiteInstanceImpl::LockToOrigin", "site id",
                   id_, "lock", lock_url().possibly_invalid_spec());
      process_->LockToOrigin(GetIsolationContext(), lock_url());
    } else if (process_lock != lock_url()) {
      // We should never attempt to reassign a different origin lock to a
      // process.
      base::debug::SetCrashKeyString(bad_message::GetRequestedSiteURLKey(),
                                     site_.possibly_invalid_spec());
      policy->LogKilledProcessOriginLock(process_->GetID());
      CHECK(false) << "Trying to lock a process to " << lock_url()
                   << " but the process is already locked to " << process_lock;
    } else {
      // Process already has the right origin lock assigned.  This case will
      // happen for commits to |site_| after the first one.
    }
  } else {
    // If the site that we've just committed doesn't require a dedicated
    // process, make sure we aren't putting it in a process for a site that
    // does.
    if (!process_lock.is_empty()) {
      base::debug::SetCrashKeyString(bad_message::GetRequestedSiteURLKey(),
                                     site_.possibly_invalid_spec());
      policy->LogKilledProcessOriginLock(process_->GetID());
      CHECK(false) << "Trying to commit non-isolated site " << site_
                   << " in process locked to " << process_lock;
    }
  }

  // Track which isolation contexts use the given process.  This lets
  // ChildProcessSecurityPolicyImpl (e.g. CanAccessDataForOrigin) determine
  // whether a given URL should require a lock or not (a dynamically isolated
  // origin may require a lock in some isolation contexts but not in others).
  policy->IncludeIsolationContext(process_->GetID(), GetIsolationContext());
}

// static
void SiteInstance::StartIsolatingSite(BrowserContext* context,
                                      const GURL& url) {
  if (!SiteIsolationPolicy::AreDynamicIsolatedOriginsEnabled())
    return;

  // Ignore attempts to isolate origins that are not supported.  Do this here
  // instead of relying on AddIsolatedOrigins()'s internal validation, to avoid
  // the runtime warning generated by the latter.
  url::Origin origin(url::Origin::Create(url));
  if (!IsolatedOriginUtil::IsValidIsolatedOrigin(origin))
    return;

  // Convert |url| to a site, to avoid breaking document.domain.  Note that
  // this doesn't use effective URL resolution or other special cases from
  // GetSiteForURL() and simply converts |origin| to a scheme and eTLD+1.
  GURL site(SiteInstanceImpl::GetSiteForOrigin(origin));

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  url::Origin site_origin(url::Origin::Create(site));
  policy->AddIsolatedOrigins(
      {site_origin},
      ChildProcessSecurityPolicy::IsolatedOriginSource::USER_TRIGGERED,
      context);

  // This function currently assumes the new isolated site should persist
  // across restarts, so ask the embedder to save it, excluding off-the-record
  // profiles.
  if (!context->IsOffTheRecord())
    GetContentClient()->browser()->PersistIsolatedOrigin(context, site_origin);
}

}  // namespace content
