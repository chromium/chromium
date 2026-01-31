// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/test_util.h"

#include "base/containers/to_vector.h"

namespace blink {

namespace {

WebCSPSource ConvertSource(const network::mojom::blink::CSPSourcePtr& source) {
  return {source->scheme,
          source->host,
          source->port,
          source->path,
          source->is_host_wildcard,
          source->is_port_wildcard};
}

WebCSPSourceList ConvertSourceList(
    const network::mojom::blink::CSPSourceListPtr& source_list) {
  return {base::ToVector(source_list->sources, ConvertSource),
          base::ToVector(source_list->nonces, ToWebString),
          base::ToVector(source_list->hashes),
          base::ToVector(source_list->url_hashes),
          base::ToVector(source_list->eval_hashes),
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
          source_list->allow_trusted_types_eval,
          source_list->report_hash_algorithm};
}

std::optional<WebCSPTrustedTypes> ConvertTrustedTypes(
    const network::mojom::blink::CSPTrustedTypesPtr& trusted_types) {
  if (!trusted_types) {
    return std::nullopt;
  }
  return WebCSPTrustedTypes{base::ToVector(trusted_types->list, ToWebString),
                            trusted_types->allow_any,
                            trusted_types->allow_duplicates};
}

}  // namespace

WebContentSecurityPolicy ConvertToPublic(
    network::mojom::blink::ContentSecurityPolicyPtr policy) {
  return {ConvertSource(policy->self_origin),
          base::ToVector(policy->raw_directives,
                         [](const auto& directive) {
                           return WebContentSecurityPolicyRawDirective{
                               directive.key, directive.value};
                         }),
          base::ToVector(policy->directives,
                         [](const auto& directive) {
                           return WebContentSecurityPolicyDirective{
                               directive.key,
                               ConvertSourceList(directive.value)};
                         }),
          policy->upgrade_insecure_requests,
          policy->treat_as_public_address,
          policy->block_all_mixed_content,
          policy->sandbox,
          {policy->header->header_value, policy->header->type,
           policy->header->source},
          policy->use_reporting_api,
          base::ToVector(policy->report_endpoints, ToWebString),
          policy->require_trusted_types_for,
          ConvertTrustedTypes(policy->trusted_types),
          base::ToVector(policy->parsing_errors, ToWebString)};
}

}  // namespace blink
