// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SECURITY_COOP_COOP_RELATED_GROUP_H_
#define CONTENT_BROWSER_SECURITY_COOP_COOP_RELATED_GROUP_H_

#include <optional>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/url_info.h"
#include "content/browser/web_exposed_isolation_info.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

class BrowserContext;
class BrowsingInstance;
class SiteInstanceImpl;

// A CoopRelatedGroup is a set of browsing context groups that can communicate
// with each other via a limited subset of properties
// (currently window.postMessage() and window.closed). Documents in
// BrowsingContexts that are not part of the same CoopRelatedGroup cannot get
// references to each other's Window by any means at all. CoopRelatedGroup,
// browsing context groups (BrowsingInstances) and Agent Clusters (roughly, but
// not strictly equivalent to SiteInstances) provide three tiers of
// communication capabilities:
// - Documents in the same Agent Cluster can synchronously DOM script each
//   other.
// - Documents in the same browsing context group can asynchronously interact
//   with each other, via cross-origin Window properties.
// - Documents in the same CoopRelatedGroup can only message each
//   other and observe window.closed.
//
// These layers have a 1->n relationship pattern: a CoopRelatedGroup contains 1
// or more browsing context groups, itself containing 1 or more agent clusters.
// Each layer is refcounted and therefore kept alive by the layer below it, with
// individual SiteInstances at the base, being kept alive manually.
//
// When no document inside a browsing context group sets COOP:
// restrict-properties, the CoopRelatedGroup contains only a single browsing
// context group. CoopRelatedGroups containing more than a single browsing
// context group occur when COOP: restrict-properties forces a browsing context
// group swap in the same CoopRelatedGroup. It allows retaining a relationship
// to the opener across browsing context groups, hence creating the actual
// communication channel.
//
// Like BrowsingInstance, CoopRelatedGroup has no public members, as it is
// designed to be interacted with only from the BrowsingInstance class, itself
// only reachable from SiteInstance. To get a new SiteInstance that is part of
// the same CoopRelatedGroup but in a different BrowsingInstance, use
// SiteInstanceImpl::GetCoopRelatedSiteInstance. Because of this,
// CoopRelatedGroups are tested in site_instance_impl_unittest.cc.
class CONTENT_EXPORT CoopRelatedGroup final
    : public base::RefCounted<CoopRelatedGroup> {
 public:
  CoopRelatedGroup(const CoopRelatedGroup&) = delete;
  CoopRelatedGroup& operator=(const CoopRelatedGroup&) = delete;

 private:
  friend class base::RefCounted<CoopRelatedGroup>;
  friend class BrowsingInstance;
  friend class CoopRelatedGroupTest;

  explicit CoopRelatedGroup(BrowserContext* browser_context,
                            bool is_guest,
                            bool is_fenced,
                            bool is_fixed_storage_partition);
  ~CoopRelatedGroup();

  // Returns the token uniquely identifying this CoopRelatedGroup.
  base::UnguessableToken token() const { return token_; }

  // Returns a SiteInstance in this CoopRelatedGroup, depending on the passed
  // `url_info`. It might reuse an existing BrowsingInstance that is part of the
  // group if one is suitable, given its COOP value, origin and cross-origin
  // isolation state. If none is suitable, a new BrowsingInstance with the
  // appropriate characteristics will be created.
  //
  // `allow_default_site_instance` is used to specify whether the returned
  // SiteInstance can be the default SiteInstance.
  scoped_refptr<SiteInstanceImpl> GetCoopRelatedSiteInstanceForURL(
      const UrlInfo& url_info,
      bool allow_default_site_instance);

  // These functions keep the group informed of the BrowsingInstances that are
  // alive and part of it. It is necessary for the BrowsingInstance reuse
  // mechanism. They should be called in the constructor and destructor of
  // BrowsingInstance.
  void RegisterBrowsingInstance(BrowsingInstance* browsing_instance);
  void UnregisterBrowsingInstance(BrowsingInstance* browsing_instance);

  // Internal helpers that return a BrowsingInstance for a given COOP "Policy"
  // which includes whether COOP: restrict-properties was set and from which
  // origin, as well as whether it was augmented with COEP.
  // `FindSuitableBrowsingInstanceForCoopPolicy` only returns an existing
  // BrowsingInstance with the given Policy, while
  // `GetOrCreateBrowsingInstanceForCoopPolicy` will create a new one if no
  // suitable BrowsingInstance exists in this group.
  scoped_refptr<BrowsingInstance> FindSuitableBrowsingInstanceForCoopPolicy(
      const std::optional<url::Origin>& common_coop_origin,
      const WebExposedIsolationInfo& web_exposed_isolation_info);
  scoped_refptr<BrowsingInstance> GetOrCreateBrowsingInstanceForCoopPolicy(
      const std::optional<url::Origin>& common_coop_origin,
      const WebExposedIsolationInfo& web_exposed_isolation_info);

  // Tracks the number of WebContents currently in this CoopRelatedGroup.
  // Note: We also separately track the number of WebContents in specific
  // BrowsingInstances, for validity checks.
  size_t active_contents_count() const { return active_contents_count_; }
  void increment_active_contents_count() { active_contents_count_++; }
  void decrement_active_contents_count() {
    DCHECK_LT(0u, active_contents_count_);
    active_contents_count_--;
  }

  // Recorded with the first BrowsingInstance and used to create new
  // BrowsingInstances. All BrowsingInstances in a CoopRelatedGroup should share
  // the same BrowserContext, therefore recording it at creation time is fine.
  raw_ptr<BrowserContext, DanglingUntriaged> browser_context_;

  // Whether all the documents presented in this CoopRelatedGroup are for guest
  // views.
  bool is_guest_;

  // Whether all the documents presented in this CoopRelatedGroup are for a
  // fenced frame.
  bool is_fenced_;

  // Whether all the documents presented in this CoopRelatedGroup have fixed
  // storage partition config.
  //
  // TODO(crbug.com/40943418): We actually always want this behavior. Remove
  // this bit when we are ready.
  bool is_fixed_storage_partition_;

  // All the BrowsingInstances belonging to this CoopRelatedGroup. They are not
  // owned by this group, but collectively own it instead. To keep track of the
  // group members we therefore use raw_ptrs, and add or delete members of the
  // group via the RegisterBrowsingInstance and UnregisterBrowsingInstance
  // methods. These are called from the BrowsingInstance constructor and
  // destructor respectively.
  //
  // There exists at most one BrowsingInstance hosting documents with the same
  // "Policy", namely a combination of whether COOP: restrict-properties was set
  // and from which origin, and whether it set COEP as well. This gives us three
  // types of BrowsingInstances:
  // - The ones with COOP: restrict-properties set from a given origin.
  // - The ones with COOP: restrict-properties-plus-COEP set from a given
  //   origin.
  // - A single BrowsingInstance for all the rest.
  //
  // We make sure we do not create two BrowsingInstances with the same
  // Policy when running RegisterBrowsingInstance.
  std::vector<raw_ptr<BrowsingInstance>> coop_related_browsing_instances_;

  // Number of all WebContents currently using any of the BrowsingInstances in
  // this group. This is used to determine if there are multiple windows in the
  // group, to know whether certain actions (e.g. putting a page into the
  // BFCache) are allowed.
  size_t active_contents_count_{0u};

  // A token uniquely identifying this CoopRelatedGroup. This can be sent to the
  // renderer process if needed, without security risks.
  const base::UnguessableToken token_ = base::UnguessableToken::Create();
};

}  // namespace content

#endif  // CONTENT_BROWSER_SECURITY_COOP_COOP_RELATED_GROUP_H_;
