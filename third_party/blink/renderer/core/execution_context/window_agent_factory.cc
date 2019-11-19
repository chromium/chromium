// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/window_agent_factory.h"
#include "third_party/blink/renderer/core/execution_context/window_agent.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin_hash.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

WindowAgentFactory::WindowAgentFactory() = default;

WindowAgent* WindowAgentFactory::GetAgentForOrigin(
    bool has_potential_universal_access_privilege,
    v8::Isolate* isolate,
    const SecurityOrigin* origin) {
  if (has_potential_universal_access_privilege) {
    if (!universal_access_agent_) {
      universal_access_agent_ = MakeGarbageCollected<WindowAgent>(isolate);
    }
    return universal_access_agent_;
  }

  // For `file:` scheme origins.
  if (origin->IsLocal()) {
    if (!file_url_agent_)
      file_url_agent_ = MakeGarbageCollected<WindowAgent>(isolate);
    return file_url_agent_;
  }

  // For opaque origins.
  if (origin->IsOpaque()) {
    auto inserted = opaque_origin_agents_.insert(origin, nullptr);
    if (inserted.is_new_entry)
      inserted.stored_value->value = MakeGarbageCollected<WindowAgent>(isolate);
    return inserted.stored_value->value;
  }

  // For tuple origins.
  String registrable_domain = origin->RegistrableDomain();
  if (registrable_domain.IsNull())
    registrable_domain = origin->Host();

  TupleOriginAgents* tuple_origin_agents = &tuple_origin_agents_;

  // All chrome extensions need to share the same agent because they can
  // access each other's windows directly.
  if (origin->Protocol() == "chrome-extension") {
    DEFINE_STATIC_LOCAL(Persistent<TupleOriginAgents>, static_origin_agents,
                        (MakeGarbageCollected<TupleOriginAgents>()));
    tuple_origin_agents = static_origin_agents;
  }

  SchemeAndRegistrableDomain key(origin->Protocol(), registrable_domain);
  auto inserted = tuple_origin_agents->insert(key, nullptr);
  if (inserted.is_new_entry)
    inserted.stored_value->value = MakeGarbageCollected<WindowAgent>(isolate);
  return inserted.stored_value->value;
}

void WindowAgentFactory::Trace(blink::Visitor* visitor) {
  visitor->Trace(universal_access_agent_);
  visitor->Trace(file_url_agent_);
  visitor->Trace(opaque_origin_agents_);
  visitor->Trace(tuple_origin_agents_);
}

// static
unsigned WindowAgentFactory::SchemeAndRegistrableDomainHash::GetHash(
    const SchemeAndRegistrableDomain& value) {
  return WTF::HashInts(StringHash::GetHash(value.scheme),
                       StringHash::GetHash(value.registrable_domain));
}

// static
bool WindowAgentFactory::SchemeAndRegistrableDomainHash::Equal(
    const SchemeAndRegistrableDomain& x,
    const SchemeAndRegistrableDomain& y) {
  return x.scheme == y.scheme && x.registrable_domain == y.registrable_domain;
}

// static
bool WindowAgentFactory::SchemeAndRegistrableDomainTraits::IsEmptyValue(
    const SchemeAndRegistrableDomain& value) {
  return HashTraits<String>::IsEmptyValue(value.scheme);
}

// static
bool WindowAgentFactory::SchemeAndRegistrableDomainTraits::IsDeletedValue(
    const SchemeAndRegistrableDomain& value) {
  return HashTraits<String>::IsDeletedValue(value.scheme);
}

// static
void WindowAgentFactory::SchemeAndRegistrableDomainTraits::
    ConstructDeletedValue(SchemeAndRegistrableDomain& slot, bool zero_value) {
  HashTraits<String>::ConstructDeletedValue(slot.scheme, zero_value);
}

}  // namespace blink
