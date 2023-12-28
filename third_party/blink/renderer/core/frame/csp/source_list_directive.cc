// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/source_list_directive.h"

#include "base/feature_list.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/csp_source.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace {

struct SupportedPrefixesStruct {
  const char* prefix;
  network::mojom::blink::CSPHashAlgorithm type;
};

}  // namespace

namespace blink {

namespace {

bool HasSourceMatchInList(
    const Vector<network::mojom::blink::CSPSourcePtr>& list,
    const String& self_protocol,
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status) {
  for (const auto& source : list) {
    if (CSPSourceMatches(*source, self_protocol, url, redirect_status)) {
      return true;
    }
  }
  return false;
}

bool IsScriptDirective(CSPDirectiveName directive_type) {
  return (directive_type == CSPDirectiveName::ScriptSrc ||
          directive_type == CSPDirectiveName::ScriptSrcAttr ||
          directive_type == CSPDirectiveName::ScriptSrcElem ||
          directive_type == CSPDirectiveName::DefaultSrc);
}

bool IsStyleDirective(CSPDirectiveName directive_type) {
  return (directive_type == CSPDirectiveName::StyleSrc ||
          directive_type == CSPDirectiveName::StyleSrcAttr ||
          directive_type == CSPDirectiveName::StyleSrcElem ||
          directive_type == CSPDirectiveName::DefaultSrc);
}

}  // namespace

CSPCheckResult CSPSourceListAllows(
    const network::mojom::blink::CSPSourceList& source_list,
    const network::mojom::blink::CSPSource& self_source,
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status) {
  // Wildcards match network schemes ('http', 'https', 'ftp', 'ws', 'wss'), and
  // the scheme of the protected resource:
  // https://w3c.github.io/webappsec-csp/#match-url-to-source-expression. Other
  // schemes, including custom schemes, must be explicitly listed in a source
  // list.
  if (source_list.allow_star) {
    if (url.ProtocolIsInHTTPFamily() ||
        (!url.Protocol().empty() &&
         EqualIgnoringASCIICase(url.Protocol(), self_source.scheme))) {
      return CSPCheckResult::Allowed();
    }
  }

  if (source_list.allow_self && CSPSourceMatchesAsSelf(self_source, url)) {
    return CSPCheckResult::Allowed();
  }

  if (HasSourceMatchInList(source_list.sources, self_source.scheme, url,
                           redirect_status)) {
    return CSPCheckResult::Allowed();
  }

  if (source_list.allow_star) {
    if (url.ProtocolIs("ws") || url.ProtocolIs("wss")) {
      return CSPCheckResult::AllowedOnlyIfWildcardMatchesWs();
    }
    if (url.ProtocolIs("ftp") &&
        !base::FeatureList::IsEnabled(
            network::features::kCspStopMatchingWildcardDirectivesToFtp)) {
      return CSPCheckResult::AllowedOnlyIfWildcardMatchesFtp();
    }
  }

  return CSPCheckResult::Blocked();
}

bool CSPSourceListAllowNonce(
    const network::mojom::blink::CSPSourceList& source_list,
    const String& nonce) {
  String nonce_stripped = nonce.StripWhiteSpace();
  return !nonce_stripped.IsNull() &&
         source_list.nonces.Contains(nonce_stripped);
}

bool CSPSourceListAllowHash(
    const network::mojom::blink::CSPSourceList& source_list,
    const network::mojom::blink::CSPHashSource& hash_value) {
  for (const network::mojom::blink::CSPHashSourcePtr& hash :
       source_list.hashes) {
    if (*hash == hash_value)
      return true;
  }
  return false;
}

bool CSPSourceListIsNone(
    const network::mojom::blink::CSPSourceList& source_list) {
  return !source_list.sources.size() && !source_list.allow_self &&
         !source_list.allow_star && !source_list.allow_inline &&
         !source_list.allow_unsafe_hashes && !source_list.allow_eval &&
         !source_list.allow_wasm_eval && !source_list.allow_wasm_unsafe_eval &&
         !source_list.allow_dynamic && !source_list.nonces.size() &&
         !source_list.hashes.size();
}

bool CSPSourceListIsSelf(
    const network::mojom::blink::CSPSourceList& source_list) {
  return source_list.allow_self && !source_list.sources.size() &&
         !source_list.allow_star && !source_list.allow_inline &&
         !source_list.allow_unsafe_hashes && !source_list.allow_eval &&
         !source_list.allow_wasm_eval && !source_list.allow_wasm_unsafe_eval &&
         !source_list.allow_dynamic && !source_list.nonces.size() &&
         !source_list.hashes.size();
}

bool CSPSourceListIsHashOrNoncePresent(
    const network::mojom::blink::CSPSourceList& source_list) {
  return !source_list.nonces.empty() || !source_list.hashes.empty();
}

bool CSPSourceListAllowsURLBasedMatching(
    const network::mojom::blink::CSPSourceList& source_list) {
  return !source_list.allow_dynamic &&
         (source_list.sources.size() || source_list.allow_star ||
          source_list.allow_self);
}

bool CSPSourceListAllowAllInline(
    CSPDirectiveName directive_type,
    ContentSecurityPolicy::InlineType inline_type,
    const network::mojom::blink::CSPSourceList& source_list) {
  if (!IsScriptDirective(directive_type) &&
      !IsStyleDirective(directive_type)) {
    return false;
  }

  bool allow_inline = source_list.allow_inline;
  if (inline_type ==
      ContentSecurityPolicy::InlineType::kScriptSpeculationRules) {
    allow_inline |= source_list.allow_inline_speculation_rules;
  }

  return allow_inline && !CSPSourceListIsHashOrNoncePresent(source_list) &&
         (!IsScriptDirective(directive_type) || !source_list.allow_dynamic);
}

}  // namespace blink
