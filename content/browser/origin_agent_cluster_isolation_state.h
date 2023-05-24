// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ORIGIN_AGENT_CLUSTER_ISOLATION_STATE_H_
#define CONTENT_BROWSER_ORIGIN_AGENT_CLUSTER_ISOLATION_STATE_H_

#include "content/common/content_export.h"

namespace content {

class BrowserContext;

// This class contains the OAC isolation state applied to an origin. If
// `is_origin_agent_cluster` is false, then there's no OAC isolation. If it's
// true, but `requires_origin_keyed_process` is false, then the origin has
// logical (within renderer) isolation, but not process isolation. If
// `requires_origin_keyed_process` is true, then the origin has process
// isolation as well.
class CONTENT_EXPORT OriginAgentClusterIsolationState {
 public:
  // With the OriginAgentCluster-by-default flag controlling whether default
  // isolation is non-isolated (flag off) or OriginAgentCluster but without
  // origin-keyed process (flag on), this function is used to get the correct
  // default state without having to know the flag setting.
  static OriginAgentClusterIsolationState CreateForDefaultIsolation(
      BrowserContext* context);

  static OriginAgentClusterIsolationState CreateNonIsolated() {
    return OriginAgentClusterIsolationState(false, false);
  }

  static OriginAgentClusterIsolationState CreateForOriginAgentCluster(
      bool requires_origin_keyed_process) {
    return OriginAgentClusterIsolationState(true,
                                            requires_origin_keyed_process);
  }

  bool is_origin_agent_cluster() const { return is_origin_agent_cluster_; }
  bool requires_origin_keyed_process() const {
    return requires_origin_keyed_process_;
  }

  bool operator==(const OriginAgentClusterIsolationState&) const = default;

 private:
  OriginAgentClusterIsolationState(bool is_origin_agent_cluster,
                                   bool requires_origin_keyed_process)
      : is_origin_agent_cluster_(is_origin_agent_cluster),
        requires_origin_keyed_process_(requires_origin_keyed_process) {}

  // Whether this uses an origin-keyed agent cluster in the renderer process,
  // affecting web visible behavior. See
  // https://html.spec.whatwg.org/multipage/origin.html#origin-keyed-agent-clusters.
  bool is_origin_agent_cluster_;
  // Whether this uses an origin-keyed process in the browser's process model.
  // When this is true, `is_origin_agent_cluster_` must be true as well.
  bool requires_origin_keyed_process_;
};

}  // namespace content
#endif  // CONTENT_BROWSER_ORIGIN_AGENT_CLUSTER_ISOLATION_STATE_H_
