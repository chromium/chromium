// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/origin_agent_cluster_isolation_state.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"

namespace content {

// static
OriginAgentClusterIsolationState
OriginAgentClusterIsolationState::CreateForDefaultIsolation() {
  if (base::FeatureList::IsEnabled(
          blink::features::kOriginAgentClusterDefaultEnabled)) {
    return CreateForOriginAgentCluster(
        false /* requires_origin_keyed_process */);
  }
  return CreateNonIsolated();
}

}  // namespace content
