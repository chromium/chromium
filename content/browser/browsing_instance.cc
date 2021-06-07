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
    const WebExposedIsolationInfo& web_exposed_isolation_info)
    : isolation_context_(
          BrowsingInstanceId::FromUnsafeValue(next_browsing_instance_id_++),
          BrowserOrResourceContext(browser_context)),
      active_contents_count_(0u),
      default_site_instance_(nullptr),
      web_exposed_isolation_info_(web_exposed_isolation_info) {
  DCHECK(browser_context);
}

BrowserContext* BrowsingInstance::GetBrowserContext() const {
  return isolation_context_.browser_or_resource_context().ToBrowserContext();
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
    scoped_refptr<SiteInstanceImpl> site_instance = default_site_instance_;
    if (!site_instance) {
      site_instance = new SiteInstanceImpl(this);

      // Note: |default_site_instance_| will get set inside this call
      // via RegisterSiteInstance().
      site_instance->SetSiteInfoToDefault();
      DCHECK_EQ(default_site_instance_, site_instance.get());
    }

    // Add |site_info| to the set so we can keep track of all the sites the
    // the default SiteInstance has been returned for.
    site_instance->AddSiteInfoToDefault(site_info);
    return site_instance;
  }

  return nullptr;
}

void BrowsingInstance::RegisterSiteInstance(SiteInstanceImpl* site_instance) {
  DCHECK(site_instance->browsing_instance_.get() == this);
  DCHECK(site_instance->HasSite());

  // Explicitly prevent the default SiteInstance from being added since
  // the map is only supposed to contain instances that map to a single site.
  if (site_instance->IsDefaultSiteInstance()) {
    CHECK(!default_site_instance_);
    default_site_instance_ = site_instance;
    return;
  }

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

  // Remove any origin isolation opt-ins related to this instance.
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  policy->RemoveOptInIsolatedOriginsForBrowsingInstance(
      isolation_context_.browsing_instance_id());
}

SiteInfo BrowsingInstance::ComputeSiteInfoForURL(
    const UrlInfo& url_info) const {
  return SiteInfo::Create(isolation_context_, url_info,
                          web_exposed_isolation_info_);
}

}  // namespace content
