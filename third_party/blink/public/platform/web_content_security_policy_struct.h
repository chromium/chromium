/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CONTENT_SECURITY_POLICY_STRUCT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CONTENT_SECURITY_POLICY_STRUCT_H_

#include "services/network/public/mojom/content_security_policy.mojom-shared.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
enum WebWildcardDisposition {
  kWebWildcardDispositionNoWildcard,
  kWebWildcardDispositionHasWildcard
};

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
struct WebContentSecurityPolicySourceExpression {
  WebString scheme;
  WebString host;
  WebWildcardDisposition is_host_wildcard;
  int port;
  WebWildcardDisposition is_port_wildcard;
  WebString path;
};

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
struct WebContentSecurityPolicySourceList {
  bool allow_self;
  bool allow_star;
  bool allow_redirects;
  WebVector<WebContentSecurityPolicySourceExpression> sources;
};

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
struct WebContentSecurityPolicyDirective {
  WebString name;
  WebContentSecurityPolicySourceList source_list;
};

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
struct WebCSPTrustedTypes {
  WebVector<WebString> list;
  bool allow_any;
  bool allow_duplicates;
};

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
struct WebContentSecurityPolicy {
  network::mojom::ContentSecurityPolicyType disposition;
  network::mojom::ContentSecurityPolicySource source;
  WebVector<WebContentSecurityPolicyDirective> directives;
  bool upgrade_insecure_requests;
  WebVector<WebString> report_endpoints;
  WebString header;
  bool use_reporting_api;
  network::mojom::CSPRequireTrustedTypesFor require_trusted_types_for;
  base::Optional<WebCSPTrustedTypes> trusted_types;
};

}  // namespace blink

#endif
