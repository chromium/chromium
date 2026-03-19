// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/ad_tagging_utils.h"

#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"

namespace blink {

std::unique_ptr<protocol::Network::AdAncestry> CreateAdAncestryProtocolObject(
    const AdTracker::AdScriptAncestry& ad_ancestry) {
  auto ancestry_chain = std::make_unique<
      protocol::Array<protocol::Network::AdScriptIdentifier>>();
  for (const auto& ad_script : ad_ancestry.ancestry_chain) {
    ancestry_chain->emplace_back(
        protocol::Network::AdScriptIdentifier::create()
            .setScriptId(String::Number(ad_script.id.value()))
            .setDebuggerId(
                ToCoreString(ad_script.context_id.toString()->string()))
            .setName(ad_script.name)
            .build());
  }

  auto ad_ancestry_protocol = protocol::Network::AdAncestry::create()
                                  .setAncestryChain(std::move(ancestry_chain))
                                  .build();

  if (ad_ancestry.root_script_filterlist_rule.IsValid()) {
    ad_ancestry_protocol->setRootScriptFilterlistRule(
        String::FromUTF8(ad_ancestry.root_script_filterlist_rule.ToString()));
  }

  return ad_ancestry_protocol;
}

std::unique_ptr<protocol::Network::AdProvenance>
CreateAdProvenanceProtocolObject(const Node& node,
                                 const AdProvenance& ad_provenance) {
  auto protocol_provenance = protocol::Network::AdProvenance::create().build();

  if (auto* rule =
          std::get_if<subresource_filter::ScopedRule>(&ad_provenance)) {
    protocol_provenance->setFilterlistRule(String(rule->ToString()));
  } else if (auto* script_id = std::get_if<V8ScriptId>(&ad_provenance)) {
    if (auto* ad_tracker =
            AdTracker::FromExecutionContext(node.GetExecutionContext())) {
      auto ad_script_ancestry = ad_tracker->GetAncestry(*script_id);
      if (!ad_script_ancestry.ancestry_chain.empty()) {
        protocol_provenance->setAdScriptAncestry(
            CreateAdAncestryProtocolObject(ad_script_ancestry));
      }
    }
  }

  return protocol_provenance;
}

}  // namespace blink
