// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/cross_thread_global_scope_creation_params_copier.h"

#include "services/network/public/mojom/content_security_policy.mojom-blink.h"

namespace WTF {

namespace {

network::mojom::blink::CSPSourcePtr CSPSourceIsolatedCopy(
    const network::mojom::blink::CSPSourcePtr& in) {
  if (!in)
    return nullptr;
  return network::mojom::blink::CSPSource::New(
      CrossThreadCopier<String>::Copy(in->scheme),
      CrossThreadCopier<String>::Copy(in->host), in->port,
      CrossThreadCopier<String>::Copy(in->path), in->is_host_wildcard,
      in->is_port_wildcard);
}

network::mojom::blink::CSPHashSourcePtr CSPHashSourceIsolatedCopy(
    const network::mojom::blink::CSPHashSourcePtr& in) {
  if (!in)
    return nullptr;
  return network::mojom::blink::CSPHashSource::New(
      in->algorithm, CrossThreadCopier<Vector<uint8_t>>::Copy(in->value));
}

HashMap<network::mojom::blink::CSPDirectiveName, String>
RawDirectivesIsolatedCopy(
    const HashMap<network::mojom::blink::CSPDirectiveName, String>& in) {
  HashMap<network::mojom::blink::CSPDirectiveName, String> out;
  for (const auto& element : in) {
    out.insert(element.key, CrossThreadCopier<String>::Copy(element.value));
  }
  return out;
}

network::mojom::blink::CSPSourceListPtr CSPSourceListIsolatedCopy(
    const network::mojom::blink::CSPSourceListPtr& in) {
  if (!in)
    return nullptr;
  Vector<network::mojom::blink::CSPSourcePtr> sources;
  for (const auto& source : in->sources)
    sources.push_back(CSPSourceIsolatedCopy(source));

  Vector<network::mojom::blink::CSPHashSourcePtr> hashes;
  for (const auto& hash : in->hashes)
    hashes.push_back(CSPHashSourceIsolatedCopy(hash));

  return network::mojom::blink::CSPSourceList::New(
      std::move(sources), CrossThreadCopier<Vector<String>>::Copy(in->nonces),
      std::move(hashes), in->allow_self, in->allow_star, in->allow_inline,
      in->allow_inline_speculation_rules, in->allow_eval, in->allow_wasm_eval,
      in->allow_wasm_unsafe_eval, in->allow_dynamic, in->allow_unsafe_hashes,
      in->report_sample);
}

HashMap<network::mojom::blink::CSPDirectiveName,
        network::mojom::blink::CSPSourceListPtr>
DirectivesIsolatedCopy(
    const HashMap<network::mojom::blink::CSPDirectiveName,
                  network::mojom::blink::CSPSourceListPtr>& in) {
  HashMap<network::mojom::blink::CSPDirectiveName,
          network::mojom::blink::CSPSourceListPtr>
      out;
  for (const auto& element : in) {
    out.insert(element.key, CSPSourceListIsolatedCopy(element.value));
  }
  return out;
}

network::mojom::blink::ContentSecurityPolicyPtr
ContentSecurityPolicyIsolatedCopy(
    const network::mojom::blink::ContentSecurityPolicyPtr& csp) {
  if (!csp)
    return nullptr;
  return network::mojom::blink::ContentSecurityPolicy::New(
      CSPSourceIsolatedCopy(csp->self_origin),
      RawDirectivesIsolatedCopy(csp->raw_directives),
      DirectivesIsolatedCopy(csp->directives), csp->upgrade_insecure_requests,
      csp->treat_as_public_address, csp->block_all_mixed_content, csp->sandbox,
      network::mojom::blink::ContentSecurityPolicyHeader::New(
          CrossThreadCopier<String>::Copy(csp->header->header_value),
          csp->header->type, csp->header->source),
      csp->use_reporting_api,
      CrossThreadCopier<Vector<String>>::Copy(csp->report_endpoints),
      csp->require_trusted_types_for,
      csp->trusted_types ? network::mojom::blink::CSPTrustedTypes::New(
                               CrossThreadCopier<Vector<String>>::Copy(
                                   csp->trusted_types->list),
                               csp->trusted_types->allow_any,
                               csp->trusted_types->allow_duplicates)
                         : nullptr,
      CrossThreadCopier<Vector<String>>::Copy(csp->parsing_errors));
}

}  // namespace

CrossThreadCopier<Vector<network::mojom::blink::ContentSecurityPolicyPtr>>::Type
CrossThreadCopier<Vector<network::mojom::blink::ContentSecurityPolicyPtr>>::
    Copy(const Vector<network::mojom::blink::ContentSecurityPolicyPtr>&
             list_in) {
  Vector<network::mojom::blink::ContentSecurityPolicyPtr> list_out;
  for (const auto& element : list_in)
    list_out.emplace_back(ContentSecurityPolicyIsolatedCopy(element));
  return list_out;
}

CrossThreadCopier<std::unique_ptr<blink::GlobalScopeCreationParams>>::Type
CrossThreadCopier<std::unique_ptr<blink::GlobalScopeCreationParams>>::Copy(
    std::unique_ptr<blink::GlobalScopeCreationParams> pointer) {
  pointer->outside_content_security_policies = CrossThreadCopier<
      Vector<network::mojom::blink::ContentSecurityPolicyPtr>>::
      Copy(pointer->outside_content_security_policies);
  pointer->response_content_security_policies = CrossThreadCopier<
      Vector<network::mojom::blink::ContentSecurityPolicyPtr>>::
      Copy(pointer->response_content_security_policies);
  return pointer;
}

}  // namespace WTF
