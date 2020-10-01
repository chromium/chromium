// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_instance.h"

#include "base/check_op.h"
#include "base/command_line.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_or_resource_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"

namespace content {

// Start the BrowsingInstance ID counter from 1 to avoid a conflict with the
// invalid BrowsingInstanceId value, which is 0 in its underlying IdType32.
int BrowsingInstance::next_browsing_instance_id_ = 1;

BrowsingInstance::BrowsingInstance(
    BrowserContext* browser_context,
    bool is_coop_coep_cross_origin_isolated,
    const base::Optional<url::Origin>& coop_coep_cross_origin_isolated_origin)
    : isolation_context_(
          BrowsingInstanceId::FromUnsafeValue(next_browsing_instance_id_++),
          BrowserOrResourceContext(browser_context)),
      active_contents_count_(0u),
      default_process_(nullptr),
      default_site_instance_(nullptr),
      is_coop_coep_cross_origin_isolated_(is_coop_coep_cross_origin_isolated),
      coop_coep_cross_origin_isolated_origin_(
          coop_coep_cross_origin_isolated_origin) {
  DCHECK(!is_coop_coep_cross_origin_isolated_ ||
         coop_coep_cross_origin_isolated_origin_.has_value());
  DCHECK(browser_context);
}

void BrowsingInstance::RenderProcessHostDestroyed(RenderProcessHost* host) {
  DCHECK_EQ(default_process_, host);
  // Only clear the default process if the RenderProcessHost object goes away,
  // not if the renderer process goes away while the RenderProcessHost remains.
  default_process_->RemoveObserver(this);
  default_process_ = nullptr;
}

BrowserContext* BrowsingInstance::GetBrowserContext() const {
  return isolation_context_.browser_or_resource_context().ToBrowserContext();
}

void BrowsingInstance::SetDefaultProcess(RenderProcessHost* default_process) {
  DCHECK(!default_process_);
  DCHECK(!default_site_instance_);
  default_process_ = default_process;
  default_process_->AddObserver(this);
}

bool BrowsingInstance::IsDefaultSiteInstance(
    const SiteInstanceImpl* site_instance) const {
  return site_instance != nullptr && site_instance == default_site_instance_;
}

bool BrowsingInstance::IsSiteInDefaultSiteInstance(const GURL& site_url) const {
  return site_url_set_.find(site_url) != site_url_set_.end();
}

bool BrowsingInstance::HasSiteInstance(const SiteInfo& site_info) {
  return site_instance_map_.find(site_info) != site_instance_map_.end();
}

scoped_refptr<SiteInstanceImpl> BrowsingInstance::GetSiteInstanceForURL(
    const UrlInfo& url_info,
    bool allow_default_instance) {
  scoped_refptr<SiteInstanceImpl> site_instance =
      GetSiteInstanceForURLHelper(url_info, allow_default_instance);

  if (site_instance)
    return site_instance;

  // No current SiteInstance for this site, so let's create one.
  scoped_refptr<SiteInstanceImpl> instance = new SiteInstanceImpl(this);

  // Set the site of this new SiteInstance, which will register it with us,
  // unless this URL should leave the SiteInstance's site unassigned.
  if (SiteInstance::ShouldAssignSiteForURL(url_info.url))
    instance->SetSite(url_info);
  return instance;
}

SiteInfo BrowsingInstance::GetSiteInfoForURL(const UrlInfo& url_info,
                                             bool allow_default_instance) {
  scoped_refptr<SiteInstanceImpl> site_instance =
      GetSiteInstanceForURLHelper(url_info, allow_default_instance);

  if (site_instance)
    return site_instance->GetSiteInfo();

  return ComputeSiteInfoForURL(url_info);
}

bool BrowsingInstance::TrySettingDefaultSiteInstance(
    SiteInstanceImpl* site_instance,
    const UrlInfo& url_info) {
  DCHECK(!site_instance->HasSite());
  const SiteInfo site_info = ComputeSiteInfoForURL(url_info);
  if (default_site_instance_ ||
      !SiteInstanceImpl::CanBePlacedInDefaultSiteInstance(
          isolation_context_, url_info.url, site_info)) {
    return false;
  }

  // Note: |default_site_instance_| must be set before SetSite() call to
  // properly trigger default SiteInstance behavior inside that method.
  default_site_instance_ = site_instance;
  site_instance->SetSiteInfoToDefault();
  site_url_set_.insert(site_info.site_url());
  return true;
}

scoped_refptr<SiteInstanceImpl> BrowsingInstance::GetSiteInstanceForURLHelper(
    const UrlInfo& url_info,
    bool allow_default_instance) {
  const SiteInfo site_info = ComputeSiteInfoForURL(url_info);
  auto i = site_instance_map_.find(site_info);
  if (i != site_instance_map_.end())
    return i->second;

  // Check to see if we can use the default SiteInstance for sites that don't
  // need to be isolated in their own process.
  if (allow_default_instance &&
      SiteInstanceImpl::CanBePlacedInDefaultSiteInstance(
          isolation_context_, url_info.url, site_info)) {
    DCHECK(!default_process_);
    scoped_refptr<SiteInstanceImpl> site_instance = default_site_instance_;
    if (!site_instance) {
      site_instance = new SiteInstanceImpl(this);

      // Keep a copy of the pointer so it can be used for other URLs. This is
      // safe because the SiteInstanceImpl destructor will call
      // UnregisterSiteInstance() to clear this copy when the last
      // reference to |site_instance| is destroyed.
      // Note: This assignment MUST happen before the SetSite() call to ensure
      // this instance is not added to |site_instance_map_| when SetSite()
      // calls RegisterSiteInstance().
      default_site_instance_ = site_instance.get();

      site_instance->SetSiteInfoToDefault();
    }

    // Add |site_url| to the set so we can keep track of all the sites the
    // the default SiteInstance has been returned for.
    site_url_set_.insert(site_info.site_url());
    return site_instance;
  }

  return nullptr;
}

void BrowsingInstance::RegisterSiteInstance(SiteInstanceImpl* site_instance) {
  DCHECK(site_instance->browsing_instance_.get() == this);
  DCHECK(site_instance->HasSite());

  // Explicitly prevent the |default_site_instance_| from being added since
  // the map is only supposed to contain instances that map to a single site.
  if (site_instance == default_site_instance_)
    return;

  const SiteInfo& site_info = site_instance->GetSiteInfo();

  // Only register if we don't have a SiteInstance for this site already.
  // It's possible to have two SiteInstances point to the same site if two
  // tabs are navigated there at the same time.  (We don't call SetSite or
  // register them until DidNavigate.)  If there is a previously existing
  // SiteInstance for this site, we just won't register the new one.
  auto i = site_instance_map_.find(site_info);
  if (i == site_instance_map_.end()) {
    // Not previously registered, so register it.
    site_instance_map_[site_info] = site_instance;
  }
}

void BrowsingInstance::UnregisterSiteInstance(SiteInstanceImpl* site_instance) {
  DCHECK(site_instance->browsing_instance_.get() == this);
  DCHECK(site_instance->HasSite());

  if (site_instance == default_site_instance_) {
    // The last reference to the default SiteInstance is being destroyed.
    default_site_instance_ = nullptr;
  }

  // Only unregister the SiteInstance if it is the same one that is registered
  // for the site.  (It might have been an unregistered SiteInstance.  See the
  // comments in RegisterSiteInstance.)
  auto i = site_instance_map_.find(site_instance->GetSiteInfo());
  if (i != site_instance_map_.end() && i->second == site_instance) {
    // Matches, so erase it.
    site_instance_map_.erase(i);
  }
}

// static
BrowsingInstanceId BrowsingInstance::NextBrowsingInstanceId() {
  return BrowsingInstanceId::FromUnsafeValue(next_browsing_instance_id_);
}

BrowsingInstance::~BrowsingInstance() {
  // We should only be deleted when all of the SiteInstances that refer to
  // us are gone.
  DCHECK(site_instance_map_.empty());
  DCHECK_EQ(0u, active_contents_count_);
  DCHECK(!default_site_instance_);
  if (default_process_)
    default_process_->RemoveObserver(this);

  // Remove any origin isolation opt-ins related to this instance.
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  policy->RemoveOptInIsolatedOriginsForBrowsingInstance(isolation_context_);
}

SiteInfo BrowsingInstance::ComputeSiteInfoForURL(
    const UrlInfo& url_info) const {
  return SiteInstanceImpl::ComputeSiteInfo(
      isolation_context_, url_info, is_coop_coep_cross_origin_isolated_,
      coop_coep_cross_origin_isolated_origin_);
}

}  // namespace content
