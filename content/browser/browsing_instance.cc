// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_instance.h"

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/origin_agent_cluster_isolation_state.h"
#include "content/browser/security/coop/coop_related_group.h"
#include "content/browser/site_info.h"
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
    const WebExposedIsolationInfo& web_exposed_isolation_info,
    bool is_guest,
    bool is_fenced,
    bool is_fixed_storage_partition,
    const scoped_refptr<CoopRelatedGroup>& coop_related_group,
    std::optional<url::Origin> common_coop_origin)
    : isolation_context_(
          BrowsingInstanceId::FromUnsafeValue(next_browsing_instance_id_++),
          BrowserOrResourceContext(browser_context),
          is_guest,
          is_fenced,
          OriginAgentClusterIsolationState::CreateForDefaultIsolation(
              browser_context)),
      active_contents_count_(0u),
      default_site_instance_(nullptr),
      web_exposed_isolation_info_(web_exposed_isolation_info),
      coop_related_group_(coop_related_group),
      common_coop_origin_(common_coop_origin),
      is_fixed_storage_partition_(is_fixed_storage_partition) {
  DCHECK(browser_context);
  if (is_guest) {
    CHECK(is_fixed_storage_partition);
  }

  // If we get passed an empty group, build a new one. This is the common case.
  if (!coop_related_group_) {
    coop_related_group_ =
        base::WrapRefCounted<CoopRelatedGroup>(new CoopRelatedGroup(
            browser_context, is_guest, is_fenced, is_fixed_storage_partition_));
  }
  DCHECK(coop_related_group_);

  coop_related_group_->RegisterBrowsingInstance(this);
}

BrowserContext* BrowsingInstance::GetBrowserContext() const {
  return isolation_context_.browser_or_resource_context().ToBrowserContext();
}

bool BrowsingInstance::HasSiteInstance(const SiteInfo& site_info) {
  return base::Contains(site_instance_map_, site_info);
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

  // Set the site of this new SiteInstance, which will register it with us.
  // Some URLs should leave the SiteInstance's site unassigned, though if
  // `instance` is for a guest, we should always set the site to ensure that it
  // carries guest information contained within SiteInfo.
  if (SiteInstanceImpl::ShouldAssignSiteForUrlInfo(url_info) ||
      isolation_context_.is_guest()) {
    instance->SetSite(url_info);
  }

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

scoped_refptr<SiteInstanceImpl> BrowsingInstance::GetSiteInstanceForSiteInfo(
    const SiteInfo& site_info) {
  auto i = site_instance_map_.find(site_info);
  if (i != site_instance_map_.end())
    return i->second.get();

  scoped_refptr<SiteInstanceImpl> instance = new SiteInstanceImpl(this);
  instance->SetSite(site_info);
  return instance;
}

scoped_refptr<SiteInstanceImpl>
BrowsingInstance::GetCoopRelatedSiteInstanceForURL(
    const UrlInfo& url_info,
    bool allow_default_instance) {
  return coop_related_group_->GetCoopRelatedSiteInstanceForURL(
      url_info, allow_default_instance);
}

scoped_refptr<SiteInstanceImpl> BrowsingInstance::GetSiteInstanceForURLHelper(
    const UrlInfo& url_info,
    bool allow_default_instance) {
  const SiteInfo site_info = ComputeSiteInfoForURL(url_info);
  auto i = site_instance_map_.find(site_info);
  if (i != site_instance_map_.end())
    return i->second.get();

  // Check to see if we can use the default SiteInstance for sites that don't
  // need to be isolated in their own process.
  if (allow_default_instance &&
      SiteInstanceImpl::CanBePlacedInDefaultSiteInstance(
          isolation_context_, url_info.url, site_info)) {
    scoped_refptr<SiteInstanceImpl> site_instance =
        default_site_instance_.get();
    if (!site_instance) {
      site_instance = new SiteInstanceImpl(this);

      // Note: |default_site_instance_| will get set inside this call
      // via RegisterSiteInstance().
      site_instance->SetSiteInfoToDefault(site_info.storage_partition_config());
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

  // Verify that the SiteInstance's StoragePartitionConfig matches this
  // BrowsingInstance's StoragePartitionConfig if it already has one.
  const StoragePartitionConfig& storage_partition_config =
      site_instance->GetSiteInfo().storage_partition_config();
  if (storage_partition_config_.has_value()) {
    // We should only use a single StoragePartition within a BrowsingInstance.
    // If we're attempting to use multiple, something has gone wrong with the
    // logic at upper layers.  Similarly, whether this StoragePartition is for
    // a guest should remain constant over a BrowsingInstance's lifetime.
    CHECK_EQ(storage_partition_config_.value(), storage_partition_config);
    CHECK_EQ(isolation_context_.is_guest(), site_instance->IsGuest());
  } else {
    storage_partition_config_ = storage_partition_config;
  }

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

  coop_related_group_->UnregisterBrowsingInstance(this);
}

SiteInfo BrowsingInstance::ComputeSiteInfoForURL(
    const UrlInfo& url_info) const {
  // If a StoragePartitionConfig is specified in both `url_info` and this
  // BrowsingInstance, make sure they match.
  if (url_info.storage_partition_config.has_value() &&
      storage_partition_config_.has_value()) {
    CHECK_EQ(storage_partition_config_.value(),
             url_info.storage_partition_config.value());
  }
  // If no StoragePartitionConfig was set in `url_info`, create a new UrlInfo
  // that inherit's this BrowsingInstance's StoragePartitionConfig.
  UrlInfo url_info_with_partition =
      url_info.storage_partition_config.has_value()
          ? url_info
          : UrlInfo(UrlInfoInit(url_info).WithStoragePartitionConfig(
                storage_partition_config_));

  // The WebExposedIsolationInfos must be compatible for this function to make
  // sense.
  DCHECK(WebExposedIsolationInfo::AreCompatible(
      url_info.web_exposed_isolation_info, web_exposed_isolation_info_));

  // If the passed in UrlInfo has a null WebExposedIsolationInfo, meaning that
  // it is compatible with any isolation state, we reuse the isolation state of
  // the BrowsingInstance.
  url_info_with_partition.web_exposed_isolation_info =
      url_info.web_exposed_isolation_info.value_or(web_exposed_isolation_info_);
  return SiteInfo::Create(isolation_context_, url_info_with_partition);
}

int BrowsingInstance::EstimateOriginAgentClusterOverhead() {
  DCHECK(SiteIsolationPolicy::IsProcessIsolationForOriginAgentClusterEnabled());

  std::set<SiteInfo> site_info_set;
  std::set<SiteInfo> site_info_set_no_oac;

  // The following computes an estimate of how many additional processes have
  // been created to deal with OriginAgentCluster (OAC) headers. When OAC
  // headers forces an additional process, that corresponds to the SiteInfo's
  // is_origin_keyed_ flag being set. To compute the estimate, we use the set of
  // unique SiteInstances (each represented by a unique SiteInfo) in each
  // BrowsingInstance as a proxy for the set of different RenderProcesses. We
  // start with the total count of SiteInfos, then we create a new set of
  // SiteInfos created by resetting the is_origin_keyed_ flag on each of the
  // SiteInfos (along with any corresponding adjustments to the site_url_ and
  // process_lock_url_ to reflect the possible conversion from origin to site).
  // The assumption here is that SiteInfos that forced a new process due to OAC
  // may no longer be unique once these values are reset, and as such the new
  // set will have less elements than the original set, with the difference
  // being the count of extra SiteInstances due to OAC. There are cases where
  // ignoring the OAC header would still result in an extra process, e.g. when
  // the SiteInfo's origin appears in the command-line origin isolation list.
  //
  // The estimate is computed using several simplifying assumptions:
  // 1) We only consider HTTPS SiteInfos to compute the additional SiteInfos.
  // This assumption should generally be valid, since we don't apply
  // is_origin_keyed_ to non-HTTPS schemes.
  // 2) We assume that SiteInfos from multiple BrowsingInstances aren't
  // coalesced into a single RenderProcess.  While this isn't true in general,
  // it is difficult in practice to account for, so we don't try to.
  for (auto& entry : site_instance_map_) {
    const SiteInfo& site_info = entry.first;
    GURL process_lock_url = site_info.process_lock_url();
    if (!process_lock_url.SchemeIs(url::kHttpsScheme))
      continue;

    site_info_set.insert(site_info);
    site_info_set_no_oac.insert(
        site_info.GetNonOriginKeyedEquivalentForMetrics(isolation_context_));
  }
  DCHECK_GE(site_info_set.size(), site_info_set_no_oac.size());
  int result = site_info_set.size() - site_info_set_no_oac.size();
  return result;
}

size_t BrowsingInstance::GetCoopRelatedGroupActiveContentsCount() {
  return coop_related_group_->active_contents_count();
}

void BrowsingInstance::IncrementActiveContentsCount() {
  active_contents_count_++;

  coop_related_group_->increment_active_contents_count();
}

void BrowsingInstance::DecrementActiveContentsCount() {
  DCHECK_LT(0u, active_contents_count_);
  active_contents_count_--;

  coop_related_group_->decrement_active_contents_count();
}

}  // namespace content
