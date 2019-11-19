// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_WINDOW_AGENT_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_WINDOW_AGENT_FACTORY_H_

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace v8 {
class Isolate;
}

namespace blink {

class SecurityOrigin;
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
  WindowAgentFactory();

  // Returns an instance of WindowAgent for |origin|.
  // This returns the same instance for origin A and origin B if either:
  //  - |has_potential_universal_access_privilege| is true,
  //  - both A and B have `file:` scheme,
  //  - or, they have the same scheme and the same registrable origin.
  //
  // Set |has_potential_universal_access_privilege| if an agent may be able to
  // access all other agents synchronously.
  // I.e. pass true to if either:
  //   * --disable-web-security is set,
  //   * --run-web-tests is set,
  //   * or, the Blink instance is running for Android WebView.
  WindowAgent* GetAgentForOrigin(bool has_potential_universal_access_privilege,
                                 v8::Isolate* isolate,
                                 const SecurityOrigin* origin);

  void Trace(blink::Visitor*);

 private:
  struct SchemeAndRegistrableDomain {
    String scheme;
    String registrable_domain;

    SchemeAndRegistrableDomain(const String& scheme,
                               const String& registrable_domain)
        : scheme(scheme), registrable_domain(registrable_domain) {}
  };

  struct SchemeAndRegistrableDomainHash {
    STATIC_ONLY(SchemeAndRegistrableDomainHash);
    static const bool safe_to_compare_to_empty_or_deleted = false;

    static unsigned GetHash(const SchemeAndRegistrableDomain&);
    static bool Equal(const SchemeAndRegistrableDomain&,
                      const SchemeAndRegistrableDomain&);
  };

  struct SchemeAndRegistrableDomainTraits
      : SimpleClassHashTraits<SchemeAndRegistrableDomain> {
    STATIC_ONLY(SchemeAndRegistrableDomainTraits);
    static const bool kHasIsEmptyValueFunction = true;

    static bool IsEmptyValue(const SchemeAndRegistrableDomain&);
    static bool IsDeletedValue(const SchemeAndRegistrableDomain& value);
    static void ConstructDeletedValue(SchemeAndRegistrableDomain& slot,
                                      bool zero_value);
  };

  // Use a shared instance of Agent for all frames if a frame may have the
  // universal access privilege.
  WeakMember<WindowAgent> universal_access_agent_;

  // `file:` scheme URLs are hard for tracking the equality. Use a shared
  // Agent for them.
  WeakMember<WindowAgent> file_url_agent_;

  // Use the SecurityOrigin itself as the key for opaque origins.
  HeapHashMap<scoped_refptr<const SecurityOrigin>,
              WeakMember<WindowAgent>,
              SecurityOriginHash>
      opaque_origin_agents_;

  // Use registerable domain as the key for general tuple origins.
  using TupleOriginAgents = HeapHashMap<SchemeAndRegistrableDomain,
                                        WeakMember<WindowAgent>,
                                        SchemeAndRegistrableDomainHash,
                                        SchemeAndRegistrableDomainTraits>;
  TupleOriginAgents tuple_origin_agents_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_WINDOW_AGENT_FACTORY_H_
