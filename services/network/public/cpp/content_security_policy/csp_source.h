// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CSP_SOURCE_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CSP_SOURCE_H_

#include <string>
#include "base/component_export.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"

class GURL;

namespace network {

// We now use CSPSource matching for both Content Security Policies and
// Permissions Policies. This emum is used to differentiate between them.
enum class CSPSourceContext {
  // This is the default context for which the code path was written.
  ContentSecurityPolicy,

  // Unlike the ContentSecurityPolicy context, this one prevents 'upgrade'
  // matching (e.g., an https URL matching an http CSP Source).
  PermissionsPolicy
};

// Check if a CSP |source| matches the scheme-source grammar.
bool CSPSourceIsSchemeOnly(const mojom::CSPSource& source);

// Check if a |url| matches with a CSP |source| matches.
COMPONENT_EXPORT(NETWORK_CPP)
bool CheckCSPSource(const mojom::CSPSource& source,
                    const GURL& url,
                    const mojom::CSPSource& self_source,
                    CSPSourceContext context,
                    bool has_followed_redirect = false,
                    bool is_opaque_fenced_frame = false);

// Compute the source intersection of |source_a| and |source_b|.
// https://w3c.github.io/webappsec-cspee/#intersection-source-expressions
COMPONENT_EXPORT(NETWORK_CPP)
mojom::CSPSourcePtr CSPSourcesIntersect(const mojom::CSPSource& source_a,
                                        const mojom::CSPSource& source_b);

// Check if |source_a| subsumes |source_b| according to
// https://w3c.github.io/webappsec-cspee/#subsume-source-expressions
COMPONENT_EXPORT(NETWORK_CPP)
bool CSPSourceSubsumes(const mojom::CSPSource& source_a,
                       const mojom::CSPSource& source_b);

// Serialize the CSPSource |source| as a string. This is used for reporting
// violations.
COMPONENT_EXPORT(NETWORK_CPP)
std::string ToString(const mojom::CSPSource& source);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CSP_SOURCE_H_
