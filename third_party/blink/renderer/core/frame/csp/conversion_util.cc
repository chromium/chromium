// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/conversion_util.h"

#include "services/network/public/mojom/content_security_policy.mojom-blink.h"

namespace blink {

namespace {

network::mojom::blink::CSPSourcePtr ConvertSource(const WebCSPSource& source) {
  return network::mojom::blink::CSPSource::New(
      source.scheme, source.host, source.port, source.path,
      source.is_host_wildcard, source.is_port_wildcard);
}

network::mojom::blink::CSPSourceListPtr ConvertSourceList(
    const WebCSPSourceList& source_list) {
  return network::mojom::blink::CSPSourceList::New(
      ToVector(source_list.sources, ConvertSource),
      Vector<String>(source_list.nonces),
      Vector<network::IntegrityMetadata>(source_list.hashes),
      Vector<network::IntegrityMetadata>(source_list.url_hashes),
      Vector<network::IntegrityMetadata>(source_list.eval_hashes),
      source_list.allow_self, source_list.allow_star, source_list.allow_inline,
      source_list.allow_inline_speculation_rules, source_list.allow_eval,
      source_list.allow_wasm_eval, source_list.allow_wasm_unsafe_eval,
      source_list.allow_dynamic, source_list.allow_dynamic_url,
      source_list.allow_unsafe_hashes, source_list.report_sample,
      source_list.allow_trusted_types_eval, source_list.report_hash_algorithm);
}

}  // namespace

network::mojom::blink::ContentSecurityPolicyPtr ConvertToMojoBlink(
    const WebContentSecurityPolicy& policy_in) {
  HashMap<network::mojom::CSPDirectiveName, String> raw_directives;
  for (const auto& directive : policy_in.raw_directives) {
    raw_directives.insert(directive.name, directive.value);
  }

  HashMap<network::mojom::CSPDirectiveName,
          network::mojom::blink::CSPSourceListPtr>
      directives;
  for (const auto& directive : policy_in.directives) {
    directives.insert(directive.name, ConvertSourceList(directive.source_list));
  }

  return network::mojom::blink::ContentSecurityPolicy::New(
      ConvertSource(policy_in.self_origin), std::move(raw_directives),
      std::move(directives), policy_in.upgrade_insecure_requests,
      policy_in.treat_as_public_address, policy_in.block_all_mixed_content,
      policy_in.sandbox,
      network::mojom::blink::ContentSecurityPolicyHeader::New(
          policy_in.header.header_value, policy_in.header.type,
          policy_in.header.source),
      policy_in.use_reporting_api, Vector<String>(policy_in.report_endpoints),
      policy_in.require_trusted_types_for,
      policy_in.trusted_types
          ? network::mojom::blink::CSPTrustedTypes::New(
                Vector<String>(policy_in.trusted_types->list),
                policy_in.trusted_types->allow_any,
                policy_in.trusted_types->allow_duplicates)
          : nullptr,
      Vector<String>(policy_in.parsing_errors));
}

Vector<network::mojom::blink::ContentSecurityPolicyPtr> ConvertToMojoBlink(
    const std::vector<WebContentSecurityPolicy>& list_in) {
  return Vector<network::mojom::blink::ContentSecurityPolicyPtr>(
      list_in, [](const auto& policy) { return ConvertToMojoBlink(policy); });
}

}  // namespace blink
