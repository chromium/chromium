// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_INSTANCE_H_
#define CONTENT_BROWSER_BROWSING_INSTANCE_H_

#include <stddef.h>

#include "base/check_op.h"
#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/isolation_context.h"
#include "content/browser/site_instance_group_manager.h"
#include "content/browser/web_exposed_isolation_info.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host_observer.h"
#include "url/origin.h"

class GURL;

namespace content {
class SiteInfo;
class SiteInstanceImpl;
struct UrlInfo;

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

  // Return an ID of the next BrowsingInstance to be created.  This ID is
  // guaranteed to be higher than any ID of an existing BrowsingInstance.  This
  // does *not* increment the global counter used for assigning
  // BrowsingInstance IDs: that happens only in the BrowsingInstance
  // constructor.
  static BrowsingInstanceId NextBrowsingInstanceId();

  // Create a new BrowsingInstance.
  // |web_exposed_isolation_info| indicates whether the BrowsingInstance
  // should contain only cross-origin isolated pages, i.e. pages with
  // cross-origin-opener-policy set to same-origin and
  // cross-origin-embedder-policy set to require-corp, and if so, from which
  // top level origin.
  explicit BrowsingInstance(
      BrowserContext* context,
      const WebExposedIsolationInfo& web_exposed_isolation_info);

  ~BrowsingInstance();

  // Get the browser context to which this BrowsingInstance belongs.
  BrowserContext* GetBrowserContext() const;

  // Get the IsolationContext associated with this BrowsingInstance.  This can
  // be used to track this BrowsingInstance in other areas of the code, along
  // with any other state needed to make isolation decisions.
  const IsolationContext& isolation_context() { return isolation_context_; }

  // Get the SiteInstanceGroupManager that controls all of the SiteInstance
  // groups associated with this BrowsingInstance.
  SiteInstanceGroupManager& site_instance_group_manager() {
    return site_instance_group_manager_;
  }

  // Returns whether this BrowsingInstance has registered a SiteInstance for
  // the site of |site_info|.
  bool HasSiteInstance(const SiteInfo& site_info);

  // Get the SiteInstance responsible for rendering the given UrlInfo.  Should
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
      const UrlInfo& url_info,
      bool allow_default_instance);

  // Returns a SiteInfo with site and process-lock URLs for |url_info| that are
  // identical with what these values would be if we called
  // GetSiteInstanceForURL() with the same |url_info| and
  // |allow_default_instance|. This method is used when we need this
  // information, but do not want to create a SiteInstance yet.
  //
  // Note: Unlike ComputeSiteInfoForURL() this method can return a SiteInfo for
  // a default SiteInstance, if |url_info| can be placed in the default
  // SiteInstance and |allow_default_instance| is true.
  SiteInfo GetSiteInfoForURL(const UrlInfo& url_info,
                             bool allow_default_instance);

  // Helper function used by GetSiteInstanceForURL() and GetSiteInfoForURL()
  // that returns an existing SiteInstance from |site_instance_map_| or
  // returns |default_site_instance_| if |allow_default_instance| is true and
  // other conditions are met. If there is no existing SiteInstance that is
  // appropriate for |url_info|, |allow_default_instance| combination, then a
  // nullptr is returned.
  //
  // Note: This method is not intended to be called by code outside this object.
  scoped_refptr<SiteInstanceImpl> GetSiteInstanceForURLHelper(
      const UrlInfo& url_info,
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

  bool HasDefaultSiteInstance() const {
    return default_site_instance_ != nullptr;
  }

  // Helper function used by other methods in this class to ensure consistent
  // mapping between |url_info| and SiteInfo. This method will never return a
  // SiteInfo for the default SiteInstance. It will always return something
  // specific to |url_info|.
  //
  // Note: This should not be used by code outside this class.
  SiteInfo ComputeSiteInfoForURL(const UrlInfo& url_info) const;

  // Map of SiteInfo to SiteInstance, to ensure we only have one SiteInstance
  // per SiteInfo. See https://crbug.com/1085275#c2 for the rationale behind
  // why SiteInfo is the right class to key this on.
  typedef std::map<SiteInfo, SiteInstanceImpl*> SiteInstanceMap;

  // Returns the cross-origin isolation status of the BrowsingInstance.
  const WebExposedIsolationInfo& web_exposed_isolation_info() const {
    return web_exposed_isolation_info_;
  }

  // The next available browser-global BrowsingInstance ID.
  static int next_browsing_instance_id_;

  // The IsolationContext associated with this BrowsingInstance.  This will not
  // change after the BrowsingInstance is constructed.
  //
  // This holds a common BrowserContext to which all SiteInstances in this
  // BrowsingInstance must belong.
  const IsolationContext isolation_context_;

  // Manages all SiteInstance groups for this BrowsingInstance.
  SiteInstanceGroupManager site_instance_group_manager_;

  // Map of site to SiteInstance, to ensure we only have one SiteInstance per
  // site.  The site string should be the possibly_invalid_spec() of a GURL
  // obtained with SiteInstanceImpl::GetSiteForURL.  Note that this map may not
  // contain every active SiteInstance, because a race exists where two
  // SiteInstances can be assigned to the same site.  This is ok in rare cases.
  // It also does not contain SiteInstances which have not yet been assigned a
  // site, such as about:blank.  See SiteInstance::ShouldAssignSiteForURL.
  // This map only contains instances that map to a single site. The
  // |default_site_instance_|, which associates multiple sites with a single
  // instance, is not contained in this map.
  SiteInstanceMap site_instance_map_;

  // Number of WebContentses currently using this BrowsingInstance.
  size_t active_contents_count_;

  // SiteInstance to use if a URL does not correspond to an instance in
  // |site_instance_map_| and it does not require a dedicated process.
  // This field and |default_process_| are mutually exclusive and this field
  // should only be set if kProcessSharingWithStrictSiteInstances is not
  // enabled. This is a raw pointer to avoid a reference cycle between the
  // BrowsingInstance and the SiteInstanceImpl.
  SiteInstanceImpl* default_site_instance_;

  // The cross-origin isolation status of the BrowsingInstance. This indicates
  // whether this BrowsingInstance is hosting only cross-origin isolated pages
  // and if so, from which top level origin.
  const WebExposedIsolationInfo web_exposed_isolation_info_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingInstance);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_INSTANCE_H_
