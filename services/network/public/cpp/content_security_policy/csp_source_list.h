// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CSP_SOURCE_LIST_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CSP_SOURCE_LIST_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"

class GURL;

namespace network {

COMPONENT_EXPORT(NETWORK_CPP)
std::string ToString(const mojom::CSPSourceListPtr& source_list);

// Return a CSPCheckResult that allows when at least one source in the
// |source_list| matches the |url|.
COMPONENT_EXPORT(NETWORK_CPP)
CSPCheckResult CheckCSPSourceList(mojom::CSPDirectiveName directive_name,
                                  const mojom::CSPSourceList& source_list,
                                  const GURL& url,
                                  const mojom::CSPSource& self_source,
                                  bool has_followed_redirect = false,
                                  bool is_opaque_fenced_frame = false);

// Check if |source_list_a| subsumes |source_list_b| with origin |origin_b| for
// directive |directive| according to
// https://w3c.github.io/webappsec-cspee/#subsume-source-list
COMPONENT_EXPORT(NETWORK_CPP)
bool CSPSourceListSubsumes(
    const mojom::CSPSourceList& source_list_a,
    const std::vector<const mojom::CSPSourceList*>& source_list_b,
    mojom::CSPDirectiveName directive,
    const mojom::CSPSource* origin_b);

}  // namespace network
#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CSP_SOURCE_LIST_H_
