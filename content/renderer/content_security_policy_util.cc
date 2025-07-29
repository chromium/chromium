// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/content_security_policy_util.h"

#include "base/containers/to_vector.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/integrity_metadata.mojom.h"

namespace content {

namespace {

std::vector<std::string> BuildVectorOfStrings(
    const std::vector<blink::WebString>& list_in) {
  return base::ToVector(list_in,
                        [](const blink::WebString& s) { return s.Utf8(); });
}

network::mojom::CSPSourcePtr BuildCSPSource(const blink::WebCSPSource& source) {
  return network::mojom::CSPSource::New(
      source.scheme.Utf8(), source.host.Utf8(), source.port, source.path.Utf8(),
      source.is_host_wildcard, source.is_port_wildcard);
}

network::mojom::CSPSourceListPtr BuildCSPSourceList(
    const blink::WebCSPSourceList& source_list) {
  return network::mojom::CSPSourceList::New(
      base::ToVector(source_list.sources, BuildCSPSource),
      BuildVectorOfStrings(source_list.nonces), source_list.hashes,
      source_list.url_hashes, source_list.eval_hashes, source_list.allow_self,
      source_list.allow_star, source_list.allow_inline,
      source_list.allow_inline_speculation_rules, source_list.allow_eval,
      source_list.allow_wasm_eval, source_list.allow_wasm_unsafe_eval,
      source_list.allow_dynamic, source_list.allow_dynamic_url,
      source_list.allow_unsafe_hashes, source_list.report_sample,
      source_list.report_hash_algorithm);
}

std::vector<blink::WebString> ToVectorOfWebStrings(
    std::vector<std::string> list_in) {
  return base::ToVector(list_in, &blink::WebString::FromUTF8);
}

// The parameter is a const reference instead of CSPSourcePtr so that the
// function can be used directly as a projection in ToVector.
blink::WebCSPSource ToWebCSPSource(const network::mojom::CSPSourcePtr& source) {
  return {blink::WebString::FromUTF8(std::move(source->scheme)),
          blink::WebString::FromUTF8(std::move(source->host)),
          source->port,
          blink::WebString::FromUTF8(std::move(source->path)),
          source->is_host_wildcard,
          source->is_port_wildcard};
}

blink::WebCSPSourceList ToWebCSPSourceList(
    network::mojom::CSPSourceListPtr source_list) {
  return {base::ToVector(std::move(source_list->sources), ToWebCSPSource),
          ToVectorOfWebStrings(std::move(source_list->nonces)),
          base::ToVector(std::move(source_list->hashes)),
          base::ToVector(std::move(source_list->url_hashes)),
          base::ToVector(std::move(source_list->eval_hashes)),
          source_list->allow_self,
          source_list->allow_star,
          source_list->allow_inline,
          source_list->allow_inline_speculation_rules,
          source_list->allow_eval,
          source_list->allow_wasm_eval,
          source_list->allow_wasm_unsafe_eval,
          source_list->allow_dynamic,
          source_list->allow_dynamic_url,
          source_list->allow_unsafe_hashes,
          source_list->report_sample,
          source_list->report_hash_algorithm};
}

std::optional<blink::WebCSPTrustedTypes> ToOptionalWebCSPTrustedTypes(
    network::mojom::CSPTrustedTypesPtr trusted_types) {
  if (!trusted_types)
    return std::nullopt;
  return blink::WebCSPTrustedTypes{
      ToVectorOfWebStrings(std::move(trusted_types->list)),
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
  std::vector<blink::WebContentSecurityPolicyDirective> directives =
      base::ToVector(std::move(policy_in->directives), [](auto&& directive) {
        return blink::WebContentSecurityPolicyDirective{
            directive.first, ToWebCSPSourceList(std::move(directive.second))};
      });
  std::vector<blink::WebContentSecurityPolicyRawDirective> raw_directives =
      base::ToVector(
          std::move(policy_in->raw_directives), [](const auto& directive) {
            return blink::WebContentSecurityPolicyRawDirective{
                directive.first, blink::WebString::FromUTF8(directive.second)};
          });

  return {ToWebCSPSource(std::move(policy_in->self_origin)),
          std::move(raw_directives),
          std::move(directives),
          policy_in->upgrade_insecure_requests,
          policy_in->treat_as_public_address,
          policy_in->block_all_mixed_content,
          policy_in->sandbox,
          ToWebContentSecurityPolicyHeader(std::move(policy_in->header)),
          policy_in->use_reporting_api,
          ToVectorOfWebStrings(std::move(policy_in->report_endpoints)),
          policy_in->require_trusted_types_for,
          ToOptionalWebCSPTrustedTypes(std::move(policy_in->trusted_types)),
          ToVectorOfWebStrings(std::move(policy_in->parsing_errors))};
}

std::vector<blink::WebContentSecurityPolicy> ToWebContentSecurityPolicies(
    std::vector<network::mojom::ContentSecurityPolicyPtr> list_in) {
  return base::ToVector(list_in, [](auto&& s) {
    return ToWebContentSecurityPolicy(std::move(s));
  });
}

}  // namespace content
