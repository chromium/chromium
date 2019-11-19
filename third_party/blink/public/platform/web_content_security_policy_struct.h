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
#include "third_party/blink/public/platform/web_content_security_policy.h"
#include "third_party/blink/public/platform/web_source_location.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

enum WebWildcardDisposition {
  kWebWildcardDispositionNoWildcard,
  kWebWildcardDispositionHasWildcard
};

struct WebContentSecurityPolicySourceExpression {
  WebString scheme;
  WebString host;
  WebWildcardDisposition is_host_wildcard;
  int port;
  WebWildcardDisposition is_port_wildcard;
  WebString path;
};

struct WebContentSecurityPolicySourceList {
  bool allow_self;
  bool allow_star;
  bool allow_redirects;
  WebVector<WebContentSecurityPolicySourceExpression> sources;
};

struct WebContentSecurityPolicyDirective {
  WebString name;
  WebContentSecurityPolicySourceList source_list;
};

struct WebContentSecurityPolicy {
  network::mojom::ContentSecurityPolicyType disposition;
  WebContentSecurityPolicySource source;
  WebVector<WebContentSecurityPolicyDirective> directives;
  WebVector<WebString> report_endpoints;
  WebString header;
  bool use_reporting_api;
};

struct WebContentSecurityPolicyList {
  WebVector<WebContentSecurityPolicy> policies;
  base::Optional<WebContentSecurityPolicySourceExpression> self_source;
};

struct WebContentSecurityPolicyViolation {
  // The name of the directive that violates the policy. |directive| might be a
  // directive that serves as a fallback to the |effective_directive|.
  WebString directive;

  // The name the effective directive that was checked against.
  WebString effective_directive;

  // The console message to be displayed to the user.
  WebString console_message;

  // The URL that was blocked by the policy.
  WebURL blocked_url;

  // The set of endpoints where a report of the violation should be sent.
  // Based on 'use_reporting_api' it can be either a set of group_names (when
  // 'use_reporting_api' = true) or a set of URLs. This means that it's not
  // possible to use both methods of reporting. This is by design.
  WebVector<WebString> report_endpoints;

  // Whether to use the reporting api or not.
  bool use_reporting_api;

  // The raw content security policy header that was infringed.
  WebString header;

  // Each policy has an associated disposition, which is either "enforce" or
  // "report".
  network::mojom::ContentSecurityPolicyType disposition;

  // Whether or not the violation happens after a redirect.
  bool after_redirect;

  // The source code location that triggered the blocked navigation.
  WebSourceLocation source_location;
};

}  // namespace blink

#endif
