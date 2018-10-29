// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_INSTANCE_H_
#define CONTENT_BROWSER_BROWSING_INSTANCE_H_

#include <stddef.h>

#include <string>

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"

class GURL;

namespace content {
class SiteInstanceImpl;

///////////////////////////////////////////////////////////////////////////////
//
// BrowsingInstance class
//
// A browsing instance corresponds to the notion of a "unit of related browsing
// contexts" in the HTML 5 spec.  Intuitively, it represents a collection of
// tabs and frames that can have script connections to each other.  In that
// sense, it reflects the user interface, and not the contents of the tabs and
// frames.
//
// We further subdivide a BrowsingInstance into SiteInstances, which represent
// the documents within each BrowsingInstance that are from the same site and
// thus can have script access to each other.  Different SiteInstances can
// safely run in different processes, because their documents cannot access
// each other's contents (due to the same origin policy).
//
// It is important to only have one SiteInstance per site within a given
// BrowsingInstance.  This is because any two documents from the same site
// might be able to script each other if they are in the same BrowsingInstance.
// Thus, they must be rendered in the same process.
//
// A BrowsingInstance is live as long as any SiteInstance has a reference to
// it.  A SiteInstance is live as long as any NavigationEntry or RenderViewHost
// have references to it.  Because both classes are RefCounted, they do not
// need to be manually deleted.
//
// BrowsingInstance has no public members, as it is designed to be
// visible only from the SiteInstance class.  To get a new
// SiteInstance that is part of the same BrowsingInstance, use
// SiteInstance::GetRelatedSiteInstance.  Because of this,
// BrowsingInstances and SiteInstances are tested together in
// site_instance_unittest.cc.
//
// Note that a browsing instance in the browser is independently tracked in
// the renderer inside blink::Page::RelatedPages() method (in theory the browser
// and renderer should always stay in sync).
//
///////////////////////////////////////////////////////////////////////////////
class CONTENT_EXPORT BrowsingInstance final
    : public base::RefCounted<BrowsingInstance> {
 private:
  friend class base::RefCounted<BrowsingInstance>;
  friend class SiteInstanceImpl;
  FRIEND_TEST_ALL_PREFIXES(SiteInstanceTest, OneSiteInstancePerSite);
  FRIEND_TEST_ALL_PREFIXES(SiteInstanceTest,
                           OneSiteInstancePerSiteInBrowserContext);

  // Create a new BrowsingInstance.
  explicit BrowsingInstance(BrowserContext* context);

  ~BrowsingInstance();

  // Get the browser context to which this BrowsingInstance belongs.
  BrowserContext* browser_context() const { return browser_context_; }

  // Returns whether this BrowsingInstance has registered a SiteInstance for
  // the site of the given URL.
  bool HasSiteInstance(const GURL& url);

  // Get the SiteInstance responsible for rendering the given URL.  Should
  // create a new one if necessary, but should not create more than one
  // SiteInstance per site.
  scoped_refptr<SiteInstanceImpl> GetSiteInstanceForURL(const GURL& url);

  // Adds the given SiteInstance to our map, to ensure that we do not create
  // another SiteInstance for the same site.
  void RegisterSiteInstance(SiteInstanceImpl* site_instance);

  // Removes the given SiteInstance from our map, after all references to it
  // have been deleted.  This means it is safe to create a new SiteInstance
  // if the user later visits a page from this site, within this
  // BrowsingInstance.
  void UnregisterSiteInstance(SiteInstanceImpl* site_instance);

  // Tracks the number of WebContents currently in this BrowsingInstance.
  size_t active_contents_count() const { return active_contents_count_; }
  void increment_active_contents_count() { active_contents_count_++; }
  void decrement_active_contents_count() {
    DCHECK_LT(0u, active_contents_count_);
    active_contents_count_--;
  }

  // Map of site to SiteInstance, to ensure we only have one SiteInstance per
  // site.
  typedef base::hash_map<std::string, SiteInstanceImpl*> SiteInstanceMap;

  // Common browser context to which all SiteInstances in this BrowsingInstance
  // must belong.
  BrowserContext* const browser_context_;

  // Map of site to SiteInstance, to ensure we only have one SiteInstance per
  // site.  The site string should be the possibly_invalid_spec() of a GURL
  // obtained with SiteInstanceImpl::GetSiteForURL.  Note that this map may not
  // contain every active SiteInstance, because a race exists where two
  // SiteInstances can be assigned to the same site.  This is ok in rare cases.
  // It also does not contain SiteInstances which have not yet been assigned a
  // site, such as about:blank.  See NavigatorImpl::ShouldAssignSiteForURL.
  SiteInstanceMap site_instance_map_;

  // Number of WebContentses currently using this BrowsingInstance.
  size_t active_contents_count_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingInstance);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_INSTANCE_H_
