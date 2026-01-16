// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/window_agent_factory.h"

#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/renderer/core/execution_context/window_agent.h"
#include "third_party/blink/renderer/platform/heap/disallow_new_wrapper.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

WindowAgentFactory::WindowAgentFactory(
    AgentGroupScheduler& agent_group_scheduler)
    : agent_group_scheduler_(agent_group_scheduler) {}

WindowAgent* WindowAgentFactory::GetAgentForAgentClusterKey(
    bool has_potential_universal_access_privilege,
    const AgentClusterKey& agent_cluster_key) {
  if (has_potential_universal_access_privilege) {
    if (!universal_access_agent_) {
      universal_access_agent_ =
          MakeGarbageCollected<WindowAgent>(*agent_group_scheduler_);
    }
    return universal_access_agent_.Get();
  }

  scoped_refptr<const SecurityOrigin> origin =
      agent_cluster_key.IsOriginKeyed()
          ? scoped_refptr<const SecurityOrigin>(agent_cluster_key.GetOrigin())
          : SecurityOrigin::Create(agent_cluster_key.GetURL());

  // For `file:` scheme origins.
  if (origin->IsLocal()) {
    if (!file_url_agent_) {
      // We create the |file_url_agent_| with the passed AgentClusterKey as it
      // ensures that any document created by a document using the
      // |file_url_agent_| will be inherit an AgentClusterKey with a local
      // origin, which will allow to put it in the |file_url_agent_|. Do note
      // that because the |file_url_agent_| is shared by all local origins,
      // there can be a discrepency between the origin recorded in the
      // AgentClusterKey of the |file_url_agent_| and that of the documents that
      // use it.
      file_url_agent_ = MakeGarbageCollected<WindowAgent>(
          *agent_group_scheduler_, agent_cluster_key);
    }
    return file_url_agent_.Get();
  }

  auto* agents = &agents_map_;

  // All chrome extensions need to share the same agent because they can
  // access each other's windows directly.
  if (CommonSchemeRegistry::IsExtensionScheme(origin->Protocol().Ascii())) {
    using AgentsMapHolder = DisallowNewWrapper<AgentsMap>;
    DEFINE_STATIC_LOCAL(Persistent<AgentsMapHolder>, static_agents_map,
                        (MakeGarbageCollected<AgentsMapHolder>()));
    agents = &static_agents_map->Value();
  }

  // For all other cases, we rely on the provided AgentClusterKey.
  auto inserted = agents->insert(agent_cluster_key, nullptr);
  if (inserted.is_new_entry) {
    inserted.stored_value->value = MakeGarbageCollected<WindowAgent>(
        *agent_group_scheduler_, agent_cluster_key);
  }
  return inserted.stored_value->value.Get();
}

void WindowAgentFactory::Trace(Visitor* visitor) const {
  visitor->Trace(universal_access_agent_);
  visitor->Trace(file_url_agent_);
  visitor->Trace(agents_map_);
  visitor->Trace(agent_group_scheduler_);
}

}  // namespace blink
