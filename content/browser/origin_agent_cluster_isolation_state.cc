// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/origin_agent_cluster_isolation_state.h"

#include "base/feature_list.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features.h"

namespace content {

// static
OriginAgentClusterIsolationState
OriginAgentClusterIsolationState::CreateForDefaultIsolation(
    BrowserContext* context) {
  if (SiteIsolationPolicy::AreOriginAgentClustersEnabledByDefault(context)) {
    // If OAC-by-default is enabled, we also check to see if origin-keyed
    // processes have been enabled as the default.
    bool requires_origin_keyed_process =
        SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault();
    return CreateForOriginAgentCluster(/*had_oac_request=*/false,
                                       requires_origin_keyed_process);
  }
  return CreateNonIsolatedByDefault();
}

// static
OriginAgentClusterIsolationState
OriginAgentClusterIsolationState::CreateForOriginAgentCluster(
    bool had_oac_request,
    bool requires_origin_keyed_process) {
  AgentClusterKey::OACStatus logical_oac_status =
      had_oac_request ? AgentClusterKey::OACStatus::kOriginKeyedByHeader
                      : AgentClusterKey::OACStatus::kOriginKeyedByDefault;
  AgentClusterKey::OACStatus process_isolation_oac_status =
      requires_origin_keyed_process
          ? logical_oac_status
          : AgentClusterKey::OACStatus::kSiteKeyedByDefault;
  return OriginAgentClusterIsolationState(logical_oac_status,
                                          process_isolation_oac_status);
}

OriginAgentClusterIsolationState::OriginAgentClusterIsolationState(
    AgentClusterKey::OACStatus logical_oac_status,
    AgentClusterKey::OACStatus process_isolation_oac_status)
    : logical_oac_status_(logical_oac_status),
      process_isolation_oac_status_(process_isolation_oac_status) {
  // Process isolation can only be requested if logical isolation is enabled.
  CHECK((logical_oac_status_ ==
             AgentClusterKey::OACStatus::kOriginKeyedByHeader ||
         logical_oac_status_ ==
             AgentClusterKey::OACStatus::kOriginKeyedByDefault) ||
        (process_isolation_oac_status_ !=
             AgentClusterKey::OACStatus::kOriginKeyedByHeader &&
         process_isolation_oac_status_ !=
             AgentClusterKey::OACStatus::kOriginKeyedByDefault));

  // Process isolation can only be requested if process isolation for
  // origin-keyed agent clusters is enabled.
  // TODO(crbug.com/40176090): Once SiteInstanceGroups are fully implemented, we
  // should be able to give all OAC origins their own SiteInstance.
  // OriginAgentClusterIsolationState might not need to track process isolation
  // state on top of logical isolation state.
  CHECK(SiteIsolationPolicy::IsProcessIsolationForOriginAgentClusterEnabled() ||
        (process_isolation_oac_status_ !=
             AgentClusterKey::OACStatus::kOriginKeyedByHeader &&
         process_isolation_oac_status_ !=
             AgentClusterKey::OACStatus::kOriginKeyedByDefault));
}

}  // namespace content
