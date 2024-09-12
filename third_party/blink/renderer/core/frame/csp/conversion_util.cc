// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/conversion_util.h"
#include "services/network/public/mojom/content_security_policy.mojom-blink.h"

namespace blink {

namespace {

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
WebCSPSource ConvertToPublic(network::mojom::blink::CSPSourcePtr source) {
  return {source->scheme,
          source->host,
          source->port,
          source->path,
          source->is_host_wildcard,
          source->is_port_wildcard};
}

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
WebCSPHashSource ConvertToPublic(
    network::mojom::blink::CSPHashSourcePtr hash_source) {
  return {hash_source->algorithm, std::move(hash_source->value)};
}

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
WebCSPSourceList ConvertToPublic(
    network::mojom::blink::CSPSourceListPtr source_list) {
  WebVector<WebCSPSource> sources(source_list->sources.size());
  for (wtf_size_t i = 0; i < source_list->sources.size(); ++i)
    sources[i] = ConvertToPublic(std::move(source_list->sources[i]));
  WebVector<WebCSPHashSource> hashes(source_list->hashes.size());
  for (wtf_size_t i = 0; i < source_list->hashes.size(); ++i)
    hashes[i] = ConvertToPublic(std::move(source_list->hashes[i]));
  return {std::move(sources),
          std::move(source_list->nonces),
          std::move(hashes),
          source_list->allow_self,
          source_list->allow_star,
          source_list->allow_inline,
          source_list->allow_inline_speculation_rules,
          source_list->allow_eval,
          source_list->allow_wasm_eval,
          source_list->allow_wasm_unsafe_eval,
          source_list->allow_dynamic,
          source_list->allow_unsafe_hashes,
          source_list->report_sample};
}

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
std::optional<WebCSPTrustedTypes> ConvertToPublic(
    network::mojom::blink::CSPTrustedTypesPtr trusted_types) {
  if (!trusted_types)
    return std::nullopt;
  return WebCSPTrustedTypes{std::move(trusted_types->list),
                            trusted_types->allow_any,
                            trusted_types->allow_duplicates};
}

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
WebContentSecurityPolicyHeader ConvertToPublic(
    network::mojom::blink::ContentSecurityPolicyHeaderPtr header) {
  return {header->header_value, header->type, header->source};
}

Vector<String> ConvertToWTF(const WebVector<blink::WebString>& list_in) {
  Vector<String> list_out;
  for (const auto& element : list_in)
    list_out.emplace_back(element);
  return list_out;
}

network::mojom::blink::CSPSourcePtr ConvertToMojoBlink(
    const WebCSPSource& source) {
  return network::mojom::blink::CSPSource::New(
      source.scheme, source.host, source.port, source.path,
      source.is_host_wildcard, source.is_port_wildcard);
}

network::mojom::blink::CSPHashSourcePtr ConvertToMojoBlink(
    const WebCSPHashSource& hash_source) {
  Vector<uint8_t> hash_value;
  for (uint8_t el : hash_source.value)
    hash_value.emplace_back(el);
  return network::mojom::blink::CSPHashSource::New(hash_source.algorithm,
                                                   std::move(hash_value));
}

network::mojom::blink::CSPSourceListPtr ConvertToMojoBlink(
    const WebCSPSourceList& source_list) {
  Vector<network::mojom::blink::CSPSourcePtr> sources;
  for (const auto& source : source_list.sources)
    sources.push_back(ConvertToMojoBlink(source));

  Vector<network::mojom::blink::CSPHashSourcePtr> hashes;
  for (const auto& hash : source_list.hashes)
    hashes.push_back(ConvertToMojoBlink(hash));

  return network::mojom::blink::CSPSourceList::New(
      std::move(sources), ConvertToWTF(source_list.nonces), std::move(hashes),
      source_list.allow_self, source_list.allow_star, source_list.allow_inline,
      source_list.allow_inline_speculation_rules, source_list.allow_eval,
      source_list.allow_wasm_eval, source_list.allow_wasm_unsafe_eval,
      source_list.allow_dynamic, source_list.allow_unsafe_hashes,
      source_list.report_sample);
}

}  // namespace

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
WebContentSecurityPolicy ConvertToPublic(
    network::mojom::blink::ContentSecurityPolicyPtr policy) {
  WebVector<WebContentSecurityPolicyDirective> directives(
      policy->directives.size());
  size_t i = 0;
  for (auto& directive : policy->directives) {
    directives[i++] = {directive.key,
                       ConvertToPublic(std::move(directive.value))};
  }

  WebVector<WebContentSecurityPolicyRawDirective> raw_directives(
      policy->raw_directives.size());
  i = 0;
  for (auto& directive : policy->raw_directives) {
    raw_directives[i++] = {directive.key, std::move(directive.value)};
  }

  return {ConvertToPublic(std::move(policy->self_origin)),
          std::move(raw_directives),
          std::move(directives),
          policy->upgrade_insecure_requests,
          policy->treat_as_public_address,
          policy->block_all_mixed_content,
          policy->sandbox,
          ConvertToPublic(std::move(policy->header)),
          policy->use_reporting_api,
          std::move(policy->report_endpoints),
          policy->require_trusted_types_for,
          ConvertToPublic(std::move(policy->trusted_types)),
          std::move(policy->parsing_errors)};
}

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
    directives.insert(directive.name,
                      ConvertToMojoBlink(directive.source_list));
  }

  return network::mojom::blink::ContentSecurityPolicy::New(
      ConvertToMojoBlink(policy_in.self_origin), std::move(raw_directives),
      std::move(directives), policy_in.upgrade_insecure_requests,
      policy_in.treat_as_public_address, policy_in.block_all_mixed_content,
      policy_in.sandbox,
      network::mojom::blink::ContentSecurityPolicyHeader::New(
          policy_in.header.header_value, policy_in.header.type,
          policy_in.header.source),
      policy_in.use_reporting_api, ConvertToWTF(policy_in.report_endpoints),
      policy_in.require_trusted_types_for,
      policy_in.trusted_types ? network::mojom::blink::CSPTrustedTypes::New(
                                    ConvertToWTF(policy_in.trusted_types->list),
                                    policy_in.trusted_types->allow_any,
                                    policy_in.trusted_types->allow_duplicates)
                              : nullptr,
      ConvertToWTF(policy_in.parsing_errors));
}

Vector<network::mojom::blink::ContentSecurityPolicyPtr> ConvertToMojoBlink(
    const WebVector<WebContentSecurityPolicy>& list_in) {
  Vector<network::mojom::blink::ContentSecurityPolicyPtr> list_out;
  for (const auto& element : list_in)
    list_out.emplace_back(ConvertToMojoBlink(element));
  return list_out;
}

}  // namespace blink
