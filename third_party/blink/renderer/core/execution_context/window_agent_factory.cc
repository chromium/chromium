// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/window_agent_factory.h"

#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/renderer/core/execution_context/window_agent.h"
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

WindowAgent* WindowAgentFactory::GetAgentForOrigin(
    bool has_potential_universal_access_privilege,
    const SecurityOrigin* origin,
    bool is_origin_agent_cluster,
    bool origin_agent_cluster_left_as_default) {
  if (has_potential_universal_access_privilege) {
    // We shouldn't have OAC turned on in this case, since we're sharing a
    // WindowAgent for all file access. This code block must be kept in sync
    // with DocumentLoader::InitializeWindow().
    DCHECK(!is_origin_agent_cluster);
    if (!universal_access_agent_) {
      universal_access_agent_ =
          MakeGarbageCollected<WindowAgent>(*agent_group_scheduler_);
    }
    return universal_access_agent_.Get();
  }

  // For `file:` scheme origins.
  if (origin->IsLocal()) {
    // We shouldn't have OAC turned on for files, since we're sharing a
    // WindowAgent for all file access. This code block must be kept in sync
    // with DocumentLoader::InitializeWindow().
    DCHECK(!is_origin_agent_cluster);
    if (!file_url_agent_) {
      file_url_agent_ =
          MakeGarbageCollected<WindowAgent>(*agent_group_scheduler_);
    }
    return file_url_agent_.Get();
  }

  // For opaque origins.
  if (origin->IsOpaque()) {
    auto inserted = opaque_origin_agents_.insert(origin, nullptr);
    if (inserted.is_new_entry) {
      inserted.stored_value->value =
          MakeGarbageCollected<WindowAgent>(*agent_group_scheduler_);
    }
    return inserted.stored_value->value.Get();
  }

  // For origin-keyed agent cluster origins.
  // Note: this map is specific to OAC, and does not represent origin-keyed
  // agents specified via Coop/Coep.
  if (is_origin_agent_cluster) {
    auto inserted = origin_keyed_agent_cluster_agents_.insert(origin, nullptr);
    if (inserted.is_new_entry) {
      inserted.stored_value->value = MakeGarbageCollected<WindowAgent>(
          *agent_group_scheduler_, is_origin_agent_cluster,
          origin_agent_cluster_left_as_default);
    }
    return inserted.stored_value->value.Get();
  }

  // For tuple origins.
  String registrable_domain = origin->RegistrableDomain();
  if (registrable_domain.IsNull())
    registrable_domain = origin->Host();

  TupleOriginAgents* tuple_origin_agents = &tuple_origin_agents_;

  // All chrome extensions need to share the same agent because they can
  // access each other's windows directly.
  if (CommonSchemeRegistry::IsExtensionScheme(origin->Protocol().Ascii())) {
    DEFINE_STATIC_LOCAL(Persistent<TupleOriginAgents>, static_origin_agents,
                        (MakeGarbageCollected<TupleOriginAgents>()));
    tuple_origin_agents = static_origin_agents;
  }

  SchemeAndRegistrableDomain key(origin->Protocol(), registrable_domain);
  auto inserted = tuple_origin_agents->insert(key, nullptr);
  if (inserted.is_new_entry) {
    inserted.stored_value->value = MakeGarbageCollected<WindowAgent>(
        *agent_group_scheduler_, is_origin_agent_cluster,
        origin_agent_cluster_left_as_default);
  }
  return inserted.stored_value->value.Get();
}

void WindowAgentFactory::Trace(Visitor* visitor) const {
  visitor->Trace(universal_access_agent_);
  visitor->Trace(file_url_agent_);
  visitor->Trace(opaque_origin_agents_);
  visitor->Trace(origin_keyed_agent_cluster_agents_);
  visitor->Trace(tuple_origin_agents_);
  visitor->Trace(agent_group_scheduler_);
}

}  // namespace blink
