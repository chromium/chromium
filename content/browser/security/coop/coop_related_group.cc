// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/security/coop/coop_related_group.h"

#include "content/browser/browsing_instance.h"
#include "content/browser/site_instance_impl.h"

#include "base/logging.h"

namespace content {

CoopRelatedGroup::CoopRelatedGroup(BrowserContext* browser_context,
                                   bool is_guest,
                                   bool is_fenced,
                                   bool is_fixed_storage_partition)
    : browser_context_(browser_context),
      is_guest_(is_guest),
      is_fenced_(is_fenced),
      is_fixed_storage_partition_(is_fixed_storage_partition) {
  if (is_guest_) {
    CHECK(is_fixed_storage_partition_);
  }
}

CoopRelatedGroup::~CoopRelatedGroup() = default;

scoped_refptr<BrowsingInstance>
CoopRelatedGroup::FindSuitableBrowsingInstanceForCoopPolicy(
    const std::optional<url::Origin>& common_coop_origin,
    const WebExposedIsolationInfo& web_exposed_isolation_info) {
  for (BrowsingInstance* current_browsing_instance :
       coop_related_browsing_instances_) {
    // Note: We don't need to know if the common_coop_origin value is the result
    // of COOP: same-origin or COOP: restrict-properties. We will only ever
    // reach this function when doing a swap within the CoopRelatedGroup, so it
    // is necessarily for COOP: restrict-properties. WebExposedIsolationInfo is
    // used to know if COEP was set together with it or not.
    if ((current_browsing_instance->common_coop_origin() ==
         common_coop_origin) &&
        (current_browsing_instance->web_exposed_isolation_info() ==
         web_exposed_isolation_info)) {
      return base::WrapRefCounted<BrowsingInstance>(current_browsing_instance);
    }
  }

  return nullptr;
}

scoped_refptr<BrowsingInstance>
CoopRelatedGroup::GetOrCreateBrowsingInstanceForCoopPolicy(
    const std::optional<url::Origin>& common_coop_origin,
    const WebExposedIsolationInfo& web_exposed_isolation_info) {
  scoped_refptr<BrowsingInstance> browsing_instance =
      FindSuitableBrowsingInstanceForCoopPolicy(common_coop_origin,
                                                web_exposed_isolation_info);

  if (browsing_instance.get()) {
    return browsing_instance;
  }

  return base::WrapRefCounted<BrowsingInstance>(new BrowsingInstance(
      browser_context_, web_exposed_isolation_info, is_guest_, is_fenced_,
      is_fixed_storage_partition_, base::WrapRefCounted<CoopRelatedGroup>(this),
      common_coop_origin));
}

void CoopRelatedGroup::RegisterBrowsingInstance(
    BrowsingInstance* browsing_instance) {
  // We should never register the same BrowsingInstance twice. If that happens,
  // we're not reusing the BrowsingInstance via GetBrowsingInstanceForCoop()
  // somewhere when we should be doing so.
  auto it = find(coop_related_browsing_instances_.begin(),
                 coop_related_browsing_instances_.end(), browsing_instance);
  CHECK(it == coop_related_browsing_instances_.end());

  // We should also never record a second BrowsingInstance with the same Policy
  // as an existing BrowsingInstance.
  scoped_refptr<BrowsingInstance> duplicated_policy_browsing_instance =
      FindSuitableBrowsingInstanceForCoopPolicy(
          browsing_instance->common_coop_origin(),
          browsing_instance->web_exposed_isolation_info());
  CHECK(duplicated_policy_browsing_instance.get() == nullptr);

  CHECK(browsing_instance->is_fixed_storage_partition() ==
        is_fixed_storage_partition_);

  coop_related_browsing_instances_.push_back(browsing_instance);
}

void CoopRelatedGroup::UnregisterBrowsingInstance(
    BrowsingInstance* browsing_instance) {
  auto it = find(coop_related_browsing_instances_.begin(),
                 coop_related_browsing_instances_.end(), browsing_instance);
  CHECK(it != coop_related_browsing_instances_.end());

  coop_related_browsing_instances_.erase(it);
}

scoped_refptr<SiteInstanceImpl>
CoopRelatedGroup::GetCoopRelatedSiteInstanceForURL(const UrlInfo& url_info,
                                                   bool allow_default_si) {
  // Fenced frames should never be able to request other SiteInstances in the
  // same CoopRelatedGroup, as they cannot open popups without noopener and COOP
  // is not enforced within the frame.
  DCHECK(!is_fenced_);
  scoped_refptr<BrowsingInstance> target_browsing_instance =
      GetOrCreateBrowsingInstanceForCoopPolicy(
          url_info.common_coop_origin,
          url_info.web_exposed_isolation_info.value_or(
              WebExposedIsolationInfo::CreateNonIsolated()));
  return target_browsing_instance->GetSiteInstanceForURL(url_info,
                                                         allow_default_si);
}

}  // namespace content
