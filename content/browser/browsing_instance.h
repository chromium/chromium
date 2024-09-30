// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_INSTANCE_H_
#define CONTENT_BROWSER_BROWSING_INSTANCE_H_

#include <stddef.h>

#include <optional>

#include "base/check_op.h"
#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/browser/isolation_context.h"
#include "content/browser/security/coop/coop_related_group.h"
#include "content/browser/site_instance_group_manager.h"
#include "content/browser/web_exposed_isolation_info.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/storage_partition_config.h"
#include "url/origin.h"

class GURL;

namespace content {
class SiteInfo;
class SiteInstanceGroup;
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
// it, and thus as long as any SiteInstanceGroup within it exists.  A
// SiteInstance is live as long as any NavigationEntry or RenderFrameHost have
// references to it.  Because both classes are RefCounted, they do not need to
// be manually deleted.
//
// BrowsingInstance has no public members, as it is designed to be
// visible only from the SiteInstance and CoopRelatedGroup classes. To get a new
// SiteInstance that is part of the same BrowsingInstance, use
// SiteInstance::GetRelatedSiteInstance. Because of this, BrowsingInstances and
// SiteInstances are tested together in site_instance_unittest.cc.
//
// Note that a browsing instance in the browser is independently tracked in
// the renderer inside blink::Page::RelatedPages() method (in theory the browser
// and renderer should always stay in sync).
//
///////////////////////////////////////////////////////////////////////////////
class CONTENT_EXPORT BrowsingInstance final
    : public base::RefCounted<BrowsingInstance> {
 public:
  BrowsingInstance(const BrowsingInstance&) = delete;
  BrowsingInstance& operator=(const BrowsingInstance&) = delete;

 private:
  friend class base::RefCounted<BrowsingInstance>;
  friend class SiteInstanceGroup;
  friend class SiteInstanceImpl;
  friend class CoopRelatedGroup;
  FRIEND_TEST_ALL_PREFIXES(SiteInstanceGroupTest, BrowsingInstanceLifetime);
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
  //
  // `web_exposed_isolation_info` indicates whether the BrowsingInstance
  // should contain only cross-origin isolated pages, i.e. pages with
  // cross-origin-opener-policy set to same-origin and
  // cross-origin-embedder-policy set to require-corp, and if so, from which
  // top level origin.
  //
  // `is_guest` specifies whether this BrowsingInstance will
  // be used in a <webview> guest; `is_fenced` specifies whether this
  // BrowsingInstance is used inside a fenced frame.
  // `is_fixed_storage_partition` indicates whether the current
  // StoragePartition will apply to future navigations. It must be set to true
  // if `is_guest` is true. Note that `is_guest`, `is_fenced`, and
  // `is_fixed_storage_partition` cannot change over the lifetime of the
  // BrowsingInstance.
  //
  // `coop_related_group` represents the CoopRelatedGroup to which this
  // BrowsingInstance belongs. Pages that live in BrowsingInstances in the same
  // group can communicate with each other through a subset of the WindowProxy
  // APIs. This is only used for COOP logic and for all other cases should
  // simply be nullptr. The constructor will take care of building a new group.
  //
  // If `common_coop_origin` is set, it indicates that all documents hosted by
  // the BrowsingInstance have the same COOP value defined by the given origin.
  explicit BrowsingInstance(
      BrowserContext* context,
      const WebExposedIsolationInfo& web_exposed_isolation_info,
      bool is_guest,
      bool is_fenced,
      bool is_fixed_storage_partition,
      const scoped_refptr<CoopRelatedGroup>& coop_related_group,
      std::optional<url::Origin> common_coop_origin);

  ~BrowsingInstance();

  // Get the browser context to which this BrowsingInstance belongs.
  BrowserContext* GetBrowserContext() const;

  // Get the IsolationContext associated with this BrowsingInstance.  This can
  // be used to track this BrowsingInstance in other areas of the code, along
  // with any other state needed to make isolation decisions.
  const IsolationContext& isolation_context() { return isolation_context_; }

  // Return true if the StoragePartition should be preserved across future
  // navigations in the frames belonging to this BrowsingInstance. For <webview>
  // tags, this always returns true.
  bool is_fixed_storage_partition() { return is_fixed_storage_partition_; }

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

  // Searches existing SiteInstances in the BrowsingInstance and returns a
  // pointer to the (unique) SiteInstance that matches `site_info`, if any.
  // If no matching SiteInstance is found, then a new SiteInstance is created
  // in this BrowsingInstance with its site set to `site_info`.
  scoped_refptr<SiteInstanceImpl> GetSiteInstanceForSiteInfo(
      const SiteInfo& site_info);

  // Return a SiteInstance in the same CoopRelatedGroup as this
  // BrowsingInstance. It might or might not be in a new BrowsingInstance, and
  // if it reuses an existing BrowsingInstance of the group, it might reuse an
  // appropriate SiteInstance as well.
  scoped_refptr<SiteInstanceImpl> GetCoopRelatedSiteInstanceForURL(
      const UrlInfo& url_info,
      bool allow_default_instance);

  // Returns a SiteInfo with site and process-lock URLs for |url_info| that are
  // identical with what these values would be if we called
  // GetSiteInstanceForURL() with the same `url_info` and
  // `allow_default_instance`. This method is used when we need this
  // information, but do not want to create a SiteInstance yet.
  //
  // Note: Unlike ComputeSiteInfoForURL() this method can return a SiteInfo for
  // a default SiteInstance, if `url_info` can be placed in the default
  // SiteInstance and `allow_default_instance` is true.
  //
  // Note: Since we're asking to get a SiteInfo that would belong in this
  // BrowsingInstance, it is mandatory that |url_info|'s
  // web_exposed_isolation_info is compatible with the BrowsingInstance's
  // internal WebExposedIsolationInfo value.
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

  // Returns the token uniquely identifying the CoopRelatedGroup this
  // BrowsingInstance belongs to. This might be used in the renderer, as opposed
  // to IDs.
  base::UnguessableToken coop_related_group_token() const {
    return coop_related_group_->token();
  }

  // Returns the token uniquely identifying this BrowsingInstance. See member
  // declaration for more context.
  base::UnguessableToken token() const { return token_; }

  // Returns the total number of WebContents either living in this
  // BrowsingInstance or that can communicate with it via the CoopRelatedGroup.
  size_t GetCoopRelatedGroupActiveContentsCount();

  // Tracks the number of WebContents currently in this BrowsingInstance.
  // Note: We also separately track the number of WebContents in the entire
  // CoopRelatedGroup, and keep the per-BrowsingInstance counts for validity
  // checks.
  void IncrementActiveContentsCount();
  void DecrementActiveContentsCount();

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

  // Computes the number of extra SiteInstances for each site due to OAC's
  // splitting a site into isolated origins.
  int EstimateOriginAgentClusterOverhead();

  // Map of SiteInfo to SiteInstance, to ensure we only have one SiteInstance
  // per SiteInfo. See https://crbug.com/1085275#c2 for the rationale behind
  // why SiteInfo is the right class to key this on.
  typedef std::map<SiteInfo, raw_ptr<SiteInstanceImpl, CtnExperimental>>
      SiteInstanceMap;

  // Returns the cross-origin isolation status of the BrowsingInstance.
  const WebExposedIsolationInfo& web_exposed_isolation_info() const {
    return web_exposed_isolation_info_;
  }

  SiteInstanceImpl* default_site_instance() { return default_site_instance_; }

  const std::optional<url::Origin>& common_coop_origin() const {
    return common_coop_origin_;
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
  // This field and site_instance_group_manager_.default_process_ are mutually
  // exclusive and this field should only be set if
  // kProcessSharingWithStrictSiteInstances is not enabled. This is a raw
  // pointer to avoid a reference cycle between the BrowsingInstance and the
  // SiteInstanceImpl. Note: This can hold cross-origin isolated SiteInstances.
  // It will however only do so under certain specific circumstances (for
  // example on a low memory device), which don't use the COOP isolation
  // heuristic that normally prevents the use of default SiteInstances for
  // cross-origin isolated pages.
  raw_ptr<SiteInstanceImpl> default_site_instance_;

  // The cross-origin isolation status of the BrowsingInstance. This indicates
  // whether this BrowsingInstance is hosting only cross-origin isolated pages
  // and if so, from which top level origin.
  const WebExposedIsolationInfo web_exposed_isolation_info_;

  // The StoragePartitionConfig that must be used by all SiteInstances in this
  // BrowsingInstance. This will be set to the StoragePartitionConfig of the
  // first SiteInstance that has its SiteInfo assigned in this
  // BrowsingInstance, and cannot be changed afterwards.
  //
  // See crbug.com/1212266 for more context on why we track the
  // StoragePartitionConfig here.
  std::optional<StoragePartitionConfig> storage_partition_config_;

  // The CoopRelatedGroup this BrowsingInstance belongs to. BrowsingInstances in
  // the same CoopRelatedGroup have limited window proxy access to each other.
  // In most cases, a CoopRelatedGroup will only contain a single
  // BrowsingInstance, unless pages that use COOP: restrict-properties headers
  // are involved.
  scoped_refptr<CoopRelatedGroup> coop_related_group_;

  // If set, indicates that all documents in this BrowsingInstance share the
  // same COOP value defined by the given origin. In practice, this can only be
  // the case for COOP: same-origin and COOP: restrict-properties.
  //
  // For COOP: same-origin, this will be enforced by COOP swap rules and the
  // value is recorded for invariant checking.
  //
  // For COOP: restrict-properties, this is also used to make sure that the
  // BrowsingInstance is suitable when we're trying to put a new document into
  // an existing BrowsingInstance that is part of the CoopRelatedGroup. To
  // prevent unwanted access, a document with COOP: restrict-properties set from
  // origin a.com should only be put in a BrowsingInstance that holds such
  // documents. This would otherwise break the access guarantees that we have
  // given, of only being able to DOM script same-origin same-COOP documents,
  // and to have limited cross-origin communication with all other pages.
  //
  // TODO(crbug.com/40879437): This assumes that popups opened from
  // cross-origin iframes are opened with no-opener. Once COOP inheritance for
  // those cases is figured out, change the mentions of origin to "COOP origin".
  std::optional<url::Origin> common_coop_origin_;

  // Set to true if the StoragePartition should be preserved across future
  // navigations in the frames belonging to this BrowsingInstance. For <webview>
  // tags, this is always true.
  //
  // TODO(crbug.com/40943418): We actually always want this behavior. Remove
  // this bit when we are ready.
  const bool is_fixed_storage_partition_;

  // A token uniquely identifying this BrowsingInstance. This is used in case we
  // need this information available in the renderer process, rather than
  // sending an ID. Both IDs and Tokens are necessary, because some parts of the
  // process model use the ordering of the IDs, that cannot be provided by
  // tokens alone. Also note that IDs are defined in IsolationContext while
  // tokens are more conveniently defined here.
  const base::UnguessableToken token_ = base::UnguessableToken::Create();
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_INSTANCE_H_
