// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_WINDOW_AGENT_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_WINDOW_AGENT_FACTORY_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/execution_context/agent_cluster_key.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class AgentGroupScheduler;
class WindowAgent;

// This is a helper class to assign WindowAgent to Document.
// The instance should be created for each group of Documents that are mutually
// reachable via `window.opener`, `window.frames` or others. These Documents
// may have different origins.
//
// The instance is intended to have the same granularity to the group of
// browsing context, that contains auxiliary browsing contexts.
// https://html.spec.whatwg.org/C#tlbc-group
// https://html.spec.whatwg.org/C#auxiliary-browsing-context
class WindowAgentFactory final : public GarbageCollected<WindowAgentFactory> {
 public:
  explicit WindowAgentFactory(AgentGroupScheduler& agent_group_scheduler);

  // Returns an instance of WindowAgent for `agent_cluster_key`.
  //
  // Set |has_potential_universal_access_privilege| if an agent may be able to
  // access all other agents synchronously.
  // I.e. pass true to if either:
  //   * --disable-web-security is set,
  //   * --run-web-tests is set,
  //   * or, the Blink instance is running for Android WebView.
  WindowAgent* GetAgentForAgentClusterKey(
      bool has_potential_universal_access_privilege,
      const AgentClusterKey& agent_cluster_key);

  void Trace(Visitor*) const;

 private:
  // Use a shared instance of Agent for all frames if a frame may have the
  // universal access privilege.
  WeakMember<WindowAgent> universal_access_agent_;

  // `file:` scheme URLs are hard for tracking the equality. Use a shared
  // Agent for them.
  WeakMember<WindowAgent> file_url_agent_;

  using AgentsMap = HeapHashMap<AgentClusterKey, WeakMember<WindowAgent>>;
  // Maps of Agents based on their AgentClusterKey.
  AgentsMap agents_map_;

  Member<AgentGroupScheduler> agent_group_scheduler_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_WINDOW_AGENT_FACTORY_H_
