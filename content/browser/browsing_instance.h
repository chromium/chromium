// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_INSTANCE_H_
#define CONTENT_BROWSER_BROWSING_INSTANCE_H_

#include <stddef.h>

#include <string>
#include <unordered_map>

#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/isolation_context.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host_observer.h"

class GURL;

namespace content {
class RenderProcessHost;
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
    : public base::RefCounted<BrowsingInstance>,
      public RenderProcessHostObserver {
 private:
  friend class base::RefCounted<BrowsingInstance>;
  friend class SiteInstanceImpl;
  FRIEND_TEST_ALL_PREFIXES(SiteInstanceTest, OneSiteInstancePerSite);
  FRIEND_TEST_ALL_PREFIXES(SiteInstanceTest,
                           OneSiteInstancePerSiteInBrowserContext);

  // Return an ID of the next BrowsingInstance to be created.  This ID is
  // guaranteed to be higher than any ID of an existing BrowsingInstance.  This
  // does *not* increment the global counter used for assigning
  // BrowsingInstance IDs: that happens only in the BrowsingInstance
  // constructor.
  static BrowsingInstanceId NextBrowsingInstanceId();

  // Create a new BrowsingInstance.
  explicit BrowsingInstance(BrowserContext* context);

  ~BrowsingInstance() final;

  // RenderProcessHostObserver implementation.
  void RenderProcessHostDestroyed(RenderProcessHost* host) final;

  // Get the browser context to which this BrowsingInstance belongs.
  BrowserContext* GetBrowserContext() const;

  // Get the IsolationContext associated with this BrowsingInstance.  This can
  // be used to track this BrowsingInstance in other areas of the code, along
  // with any other state needed to make isolation decisions.
  const IsolationContext& isolation_context() { return isolation_context_; }

  // Returns whether this BrowsingInstance has registered a SiteInstance for
  // the site of the given URL.
  bool HasSiteInstance(const GURL& url);

  // Get the SiteInstance responsible for rendering the given URL.  Should
  // create a new one if necessary, but should not create more than one
  // SiteInstance per site.
  //
  // |allow_default_instance| should be set to true in cases where the caller
  // is ok with |url| sharing a process with other sites that do not require
  // a dedicated process. Note that setting this to true means that the
  // SiteInstanceImpl you get back may return "http://unisolated.invalid" for
  // GetSiteURL() and lock_url() calls because the default instance is not
  // bound to a single site.
  scoped_refptr<SiteInstanceImpl> GetSiteInstanceForURL(
      const GURL& url,
      bool allow_default_instance);

  // Gets site and lock URLs for |url| that are identical with what these
  // values would be if we called GetSiteInstanceForURL() with the same
  // |url| and |allow_default_instance|. This method is used when we need this
  // information, but do not want to create a SiteInstance yet.
  void GetSiteAndLockForURL(const GURL& url,
                            bool allow_default_instance,
                            GURL* site_url,
                            GURL* lock_url);

  // Helper function used by GetSiteInstanceForURL() and GetSiteAndLockForURL()
  // that returns an existing SiteInstance from |site_instance_map_| or
  // returns |default_site_instance_| if |allow_default_instance| is true and
  // other conditions are met. If there is no existing SiteInstance that is
  // appropriate for |url|, |allow_default_instance| combination, then a nullptr
  // is returned.
  //
  // Note: This method is not intended to be called by code outside this object.
  scoped_refptr<SiteInstanceImpl> GetSiteInstanceForURLHelper(
      const GURL& url,
      bool allow_default_instance);

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

  // Stores the process that should be used if a SiteInstance doesn't need
  // a dedicated process.
  void SetDefaultProcess(RenderProcessHost* default_process);
  RenderProcessHost* default_process() const { return default_process_; }

  bool IsDefaultSiteInstance(const SiteInstanceImpl* site_instance) const;

  // Returns true if |site_url| has been used to get a SiteInstance from this
  // object and the default SiteInstance was returned. This simply indicates
  // the site may be directed to the default SiteInstance process, but it does
  // not indicate that the site has already been committed to that process.
  // Returns false if no request for |site_url| has resulted in this object
  // returning the default SiteInstance.
  bool IsSiteInDefaultSiteInstance(const GURL& site_url) const;

  // Attempts to convert |site_instance| into a default SiteInstance,
  // if |url| can be placed inside a default SiteInstance, and the default
  // SiteInstance has not already been set for this object.
  // Returns true if |site_instance| was successfully converted to a default
  // SiteInstance. Otherwise, returns false.
  bool TrySettingDefaultSiteInstance(SiteInstanceImpl* site_instance,
                                     const GURL& url);

  // Helper function used by other methods in this class to ensure consistent
  // mapping between |url| and site URL.
  // Note: This should not be used by code outside this class.
  GURL GetSiteForURL(const GURL& url) const;

  // Map of site to SiteInstance, to ensure we only have one SiteInstance per
  // site.
  typedef std::unordered_map<std::string, SiteInstanceImpl*> SiteInstanceMap;

  // The next available browser-global BrowsingInstance ID.
  static int next_browsing_instance_id_;

  // The IsolationContext associated with this BrowsingInstance.  This will not
  // change after the BrowsingInstance is constructed.
  //
  // This holds a common BrowserContext to which all SiteInstances in this
  // BrowsingInstance must belong.
  const IsolationContext isolation_context_;

  // Map of site to SiteInstance, to ensure we only have one SiteInstance per
  // site.  The site string should be the possibly_invalid_spec() of a GURL
  // obtained with SiteInstanceImpl::GetSiteForURL.  Note that this map may not
  // contain every active SiteInstance, because a race exists where two
  // SiteInstances can be assigned to the same site.  This is ok in rare cases.
  // It also does not contain SiteInstances which have not yet been assigned a
  // site, such as about:blank.  See NavigatorImpl::ShouldAssignSiteForURL.
  // This map only contains instances that map to a single site. The
  // |default_site_instance_|, which associates multiple sites with a single
  // instance, is not contained in this map.
  SiteInstanceMap site_instance_map_;

  // Number of WebContentses currently using this BrowsingInstance.
  size_t active_contents_count_;

  // The process to use for any SiteInstance in this BrowsingInstance that
  // doesn't require a dedicated process.
  RenderProcessHost* default_process_;

  // SiteInstance to use if a URL does not correspond to an instance in
  // |site_instance_map_| and it does not require a dedicated process.
  // This field and |default_process_| are mutually exclusive and this field
  // should only be set if kProcessSharingWithStrictSiteInstances is not
  // enabled. This is a raw pointer to avoid a reference cycle between the
  // BrowsingInstance and the SiteInstanceImpl.
  SiteInstanceImpl* default_site_instance_;

  // Keeps track of the site URLs that this object mapped to the
  // |default_site_instance_|.
  std::set<GURL> site_url_set_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingInstance);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_INSTANCE_H_
