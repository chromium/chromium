// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/content_security_policy_util.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace content {

namespace {

std::vector<std::string> BuildVectorOfStrings(
    const blink::WebVector<blink::WebString>& list_in) {
  std::vector<std::string> list_out;
  for (const auto& element : list_in)
    list_out.emplace_back(element.Utf8());
  return list_out;
}

network::mojom::CSPSourcePtr BuildCSPSource(const blink::WebCSPSource& source) {
  return network::mojom::CSPSource::New(
      source.scheme.Utf8(), source.host.Utf8(), source.port, source.path.Utf8(),
      source.is_host_wildcard, source.is_port_wildcard);
}

network::mojom::CSPHashSourcePtr BuildCSPHashSource(
    const blink::WebCSPHashSource& hash_source) {
  std::vector<uint8_t> hash_value;
  hash_value.reserve(hash_source.value.size());
  for (uint8_t el : hash_source.value)
    hash_value.emplace_back(el);
  return network::mojom::CSPHashSource::New(hash_source.algorithm,
                                            std::move(hash_value));
}

network::mojom::CSPSourceListPtr BuildCSPSourceList(
    const blink::WebCSPSourceList& source_list) {
  std::vector<network::mojom::CSPSourcePtr> sources;
  for (const auto& source : source_list.sources)
    sources.push_back(BuildCSPSource(source));

  std::vector<network::mojom::CSPHashSourcePtr> hashes;
  for (const auto& hash : source_list.hashes)
    hashes.push_back(BuildCSPHashSource(hash));

  return network::mojom::CSPSourceList::New(
      std::move(sources), BuildVectorOfStrings(source_list.nonces),
      std::move(hashes), source_list.allow_self, source_list.allow_star,
      source_list.allow_inline, source_list.allow_inline_speculation_rules,
      source_list.allow_eval, source_list.allow_wasm_eval,
      source_list.allow_wasm_unsafe_eval, source_list.allow_dynamic,
      source_list.allow_unsafe_hashes, source_list.report_sample);
}

blink::WebVector<blink::WebString> ToWebVectorOfWebStrings(
    std::vector<std::string> list_in) {
  blink::WebVector<blink::WebString> list_out(list_in.size());
  size_t i = 0;
  for (auto& element : list_in)
    list_out[i++] = blink::WebString::FromUTF8(std::move(element));
  return list_out;
}

blink::WebCSPSource ToWebCSPSource(network::mojom::CSPSourcePtr source) {
  return {blink::WebString::FromUTF8(std::move(source->scheme)),
          blink::WebString::FromUTF8(std::move(source->host)),
          source->port,
          blink::WebString::FromUTF8(std::move(source->path)),
          source->is_host_wildcard,
          source->is_port_wildcard};
}

blink::WebCSPHashSource ToWebCSPHashSource(
    network::mojom::CSPHashSourcePtr hash_source) {
  return {hash_source->algorithm, std::move(hash_source->value)};
}

blink::WebCSPSourceList ToWebCSPSourceList(
    network::mojom::CSPSourceListPtr source_list) {
  blink::WebVector<blink::WebCSPSource> sources(source_list->sources.size());
  for (size_t i = 0; i < sources.size(); ++i)
    sources[i] = ToWebCSPSource(std::move(source_list->sources[i]));
  blink::WebVector<blink::WebCSPHashSource> hashes(source_list->hashes.size());
  for (size_t i = 0; i < hashes.size(); ++i)
    hashes[i] = ToWebCSPHashSource(std::move(source_list->hashes[i]));
  return {std::move(sources),
          ToWebVectorOfWebStrings(std::move(source_list->nonces)),
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

std::optional<blink::WebCSPTrustedTypes> ToOptionalWebCSPTrustedTypes(
    network::mojom::CSPTrustedTypesPtr trusted_types) {
  if (!trusted_types)
    return std::nullopt;
  return blink::WebCSPTrustedTypes{
      ToWebVectorOfWebStrings(std::move(trusted_types->list)),
      trusted_types->allow_any, trusted_types->allow_duplicates};
}

blink::WebContentSecurityPolicyHeader ToWebContentSecurityPolicyHeader(
    network::mojom::ContentSecurityPolicyHeaderPtr header) {
  return {blink::WebString::FromUTF8(std::move(header->header_value)),
          header->type, header->source};
}

}  // namespace

network::mojom::ContentSecurityPolicyPtr BuildContentSecurityPolicy(
    const blink::WebContentSecurityPolicy& policy_in) {
  base::flat_map<network::mojom::CSPDirectiveName, std::string> raw_directives;
  for (const auto& directive : policy_in.raw_directives) {
    raw_directives[directive.name] = directive.value.Utf8();
  }

  base::flat_map<network::mojom::CSPDirectiveName,
                 network::mojom::CSPSourceListPtr>
      directives;
  for (const auto& directive : policy_in.directives) {
    directives[directive.name] = BuildCSPSourceList(directive.source_list);
  }

  return network::mojom::ContentSecurityPolicy::New(
      BuildCSPSource(policy_in.self_origin), std::move(raw_directives),
      std::move(directives), policy_in.upgrade_insecure_requests,
      policy_in.treat_as_public_address, policy_in.block_all_mixed_content,
      policy_in.sandbox,
      network::mojom::ContentSecurityPolicyHeader::New(
          policy_in.header.header_value.Utf8(), policy_in.header.type,
          policy_in.header.source),
      policy_in.use_reporting_api,
      BuildVectorOfStrings(policy_in.report_endpoints),
      policy_in.require_trusted_types_for,
      policy_in.trusted_types
          ? network::mojom::CSPTrustedTypes::New(
                BuildVectorOfStrings(policy_in.trusted_types->list),
                policy_in.trusted_types->allow_any,
                policy_in.trusted_types->allow_duplicates)
          : nullptr,
      BuildVectorOfStrings(policy_in.parsing_errors));
}

blink::WebContentSecurityPolicy ToWebContentSecurityPolicy(
    network::mojom::ContentSecurityPolicyPtr policy_in) {
  blink::WebVector<blink::WebContentSecurityPolicyDirective> directives(
      policy_in->directives.size());
  size_t i = 0;
  for (auto& directive : policy_in->directives) {
    directives[i++] = {directive.first,
                       ToWebCSPSourceList(std::move(directive.second))};
  }

  blink::WebVector<blink::WebContentSecurityPolicyRawDirective> raw_directives(
      policy_in->raw_directives.size());
  i = 0;
  for (auto& directive : policy_in->raw_directives) {
    raw_directives[i++] = {directive.first, blink::WebString::FromUTF8(
                                                std::move(directive.second))};
  }

  return {ToWebCSPSource(std::move(policy_in->self_origin)),
          std::move(raw_directives),
          std::move(directives),
          policy_in->upgrade_insecure_requests,
          policy_in->treat_as_public_address,
          policy_in->block_all_mixed_content,
          policy_in->sandbox,
          ToWebContentSecurityPolicyHeader(std::move(policy_in->header)),
          policy_in->use_reporting_api,
          ToWebVectorOfWebStrings(std::move(policy_in->report_endpoints)),
          policy_in->require_trusted_types_for,
          ToOptionalWebCSPTrustedTypes(std::move(policy_in->trusted_types)),
          ToWebVectorOfWebStrings(std::move(policy_in->parsing_errors))};
}

blink::WebVector<blink::WebContentSecurityPolicy> ToWebContentSecurityPolicies(
    std::vector<network::mojom::ContentSecurityPolicyPtr> list_in) {
  blink::WebVector<blink::WebContentSecurityPolicy> list_out(list_in.size());
  size_t i = 0;
  for (auto& element : list_in)
    list_out[i++] = ToWebContentSecurityPolicy(std::move(element));
  return list_out;
}

}  // namespace content
