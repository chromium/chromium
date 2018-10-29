// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_instance.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"

namespace content {

BrowsingInstance::BrowsingInstance(BrowserContext* browser_context)
    : browser_context_(browser_context),
      active_contents_count_(0u) {
  DCHECK(browser_context);
}

bool BrowsingInstance::HasSiteInstance(const GURL& url) {
  std::string site = SiteInstance::GetSiteForURL(browser_context_, url)
                         .possibly_invalid_spec();

  return site_instance_map_.find(site) != site_instance_map_.end();
}

scoped_refptr<SiteInstanceImpl> BrowsingInstance::GetSiteInstanceForURL(
    const GURL& url) {
  std::string site = SiteInstance::GetSiteForURL(browser_context_, url)
                         .possibly_invalid_spec();

  auto i = site_instance_map_.find(site);
  if (i != site_instance_map_.end())
    return i->second;

  // No current SiteInstance for this site, so let's create one.
  scoped_refptr<SiteInstanceImpl> instance = new SiteInstanceImpl(this);

  // Set the site of this new SiteInstance, which will register it with us.
  instance->SetSite(url);
  return instance;
}

void BrowsingInstance::RegisterSiteInstance(SiteInstanceImpl* site_instance) {
  DCHECK(site_instance->browsing_instance_.get() == this);
  DCHECK(site_instance->HasSite());

  std::string site = site_instance->GetSiteURL().possibly_invalid_spec();

  // Only register if we don't have a SiteInstance for this site already.
  // It's possible to have two SiteInstances point to the same site if two
  // tabs are navigated there at the same time.  (We don't call SetSite or
  // register them until DidNavigate.)  If there is a previously existing
  // SiteInstance for this site, we just won't register the new one.
  auto i = site_instance_map_.find(site);
  if (i == site_instance_map_.end()) {
    // Not previously registered, so register it.
    site_instance_map_[site] = site_instance;
  }
}

void BrowsingInstance::UnregisterSiteInstance(SiteInstanceImpl* site_instance) {
  DCHECK(site_instance->browsing_instance_.get() == this);
  DCHECK(site_instance->HasSite());
  std::string site = site_instance->GetSiteURL().possibly_invalid_spec();

  // Only unregister the SiteInstance if it is the same one that is registered
  // for the site.  (It might have been an unregistered SiteInstance.  See the
  // comments in RegisterSiteInstance.)
  auto i = site_instance_map_.find(site);
  if (i != site_instance_map_.end() && i->second == site_instance) {
    // Matches, so erase it.
    site_instance_map_.erase(i);
  }
}

BrowsingInstance::~BrowsingInstance() {
  // We should only be deleted when all of the SiteInstances that refer to
  // us are gone.
  DCHECK(site_instance_map_.empty());
  DCHECK_EQ(0u, active_contents_count_);
}

}  // namespace content
