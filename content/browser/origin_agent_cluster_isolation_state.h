// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ORIGIN_AGENT_CLUSTER_ISOLATION_STATE_H_
#define CONTENT_BROWSER_ORIGIN_AGENT_CLUSTER_ISOLATION_STATE_H_

#include "content/browser/agent_cluster_key.h"
#include "content/common/content_export.h"

namespace content {

class BrowserContext;

// This class contains the OAC isolation state applied to an origin. If
// `logical_oac_status` is kSiteKeyedByHeader or kSiteKeyedByDefault, then
// there's no OAC isolation. If it's kOriginKeyedByHeader or
// kOriginKeyedByDefault, but `process_isolation_oac_status` is
// kSiteKeyedByHeader or kSiteKeyedByDefault, then the origin has logical
// (within renderer) isolation, but not process isolation. If
// `process_isolation_oac_status` is kOriginKeyedByHeader or
// kOriginKeyedByDefault, then the origin has process isolation as well.
class CONTENT_EXPORT OriginAgentClusterIsolationState {
 public:
  // With the OriginAgentCluster-by-default flag controlling whether default
  // isolation is non-isolated (flag off) or OriginAgentCluster but without
  // origin-keyed process (flag on), this function is used to get the correct
  // default state without having to know the flag setting.
  static OriginAgentClusterIsolationState CreateForDefaultIsolation(
      BrowserContext* context);

  // This creates an OriginAgentClusterIsolationState with both logical and
  // process-isolation values site-keyed. This should be used when a document
  // has explicitly requested site-keyed agent clusters through the OAC header.
  static OriginAgentClusterIsolationState CreateNonIsolatedByHeader() {
    return OriginAgentClusterIsolationState(
        AgentClusterKey::OACStatus::kSiteKeyedByHeader,
        AgentClusterKey::OACStatus::kSiteKeyedByHeader);
  }

  // This creates an OriginAgentClusterIsolationState with both logical and
  // process-isolation values site-keyed. This should be used for a default
  // site-keyed OAC state for a document without OAC headers. For regular
  // contexts, prefer using CreateForDefaultIsolation, which will accurately
  // make the OAC origin-keyed when Origin Isolation by default is enabled.
  static OriginAgentClusterIsolationState CreateNonIsolatedByDefault() {
    return OriginAgentClusterIsolationState(
        AgentClusterKey::OACStatus::kSiteKeyedByDefault,
        AgentClusterKey::OACStatus::kSiteKeyedByDefault);
  }

  // This creates an OriginIsolationClusterIsolationState for a context with
  // either logical or process-isolated origin isolation enabled via
  // Origin-Agent-Cluster. |had_oac_request| is true when the context had an OAC
  // header requesting an origin-keyed agent cluster. When it is false, it means
  // that the agent cluster was made origin-keyed by default.
  // |requires_origin_keyed_process| indicates whether the agent cluster should
  // also be process isolated, on top of logical isolation in the renderer
  // process.
  static OriginAgentClusterIsolationState CreateForOriginAgentCluster(
      bool had_oac_request,
      bool requires_origin_keyed_process);

  AgentClusterKey::OACStatus logical_oac_status() const {
    return logical_oac_status_;
  }
  AgentClusterKey::OACStatus process_isolation_oac_status() const {
    return process_isolation_oac_status_;
  }

  // Whether this OriginAgentClusterIsolationState corresponds to an
  // origin-keyed agent cluster with logical isolation.
  bool is_origin_agent_cluster() const {
    return logical_oac_status_ ==
               AgentClusterKey::OACStatus::kOriginKeyedByHeader ||
           logical_oac_status_ ==
               AgentClusterKey::OACStatus::kOriginKeyedByDefault;
  }

  // Whether this OriginAgentClusterIsolationState corresponds to an
  // origin-keyed agent cluster backed by process isolation.
  bool requires_origin_keyed_process() const {
    return process_isolation_oac_status_ ==
               AgentClusterKey::OACStatus::kOriginKeyedByHeader ||
           process_isolation_oac_status_ ==
               AgentClusterKey::OACStatus::kOriginKeyedByDefault;
  }

  bool operator==(const OriginAgentClusterIsolationState&) const = default;

 private:
  OriginAgentClusterIsolationState(
      AgentClusterKey::OACStatus logical_oac_status,
      AgentClusterKey::OACStatus process_isolation_oac_status);

  // Whether this uses an origin-keyed agent cluster in the renderer process,
  // affecting web visible behavior. See
  // https://html.spec.whatwg.org/multipage/origin.html#origin-keyed-agent-clusters.
  AgentClusterKey::OACStatus logical_oac_status_;
  // Whether this uses an origin-keyed process in the browser's process model.
  // Note that if this has an origin-keyed value, `logical_oac_status_` must
  // have an origin-keyed value as well.
  AgentClusterKey::OACStatus process_isolation_oac_status_;
};

}  // namespace content
#endif  // CONTENT_BROWSER_ORIGIN_AGENT_CLUSTER_ISOLATION_STATE_H_
