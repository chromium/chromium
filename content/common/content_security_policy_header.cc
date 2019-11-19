// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_security_policy_header.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace content {

ContentSecurityPolicyHeader::ContentSecurityPolicyHeader()
    : header_value(std::string()),
      type(network::mojom::ContentSecurityPolicyType::kEnforce),
      source(blink::kWebContentSecurityPolicySourceHTTP) {}

ContentSecurityPolicyHeader::ContentSecurityPolicyHeader(
    const std::string& header_value,
    network::mojom::ContentSecurityPolicyType type,
    blink::WebContentSecurityPolicySource source)
    : header_value(header_value), type(type), source(source) {}

}  // namespace content
