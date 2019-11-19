// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_SECURITY_POLICY_HEADER_
#define CONTENT_COMMON_CONTENT_SECURITY_POLICY_HEADER_

#include <string>

#include "content/common/content_export.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "third_party/blink/public/platform/web_content_security_policy.h"

namespace content {

// Represents a single Content Security Policy header (i.e. coming from
// a single Content-Security-Policy header in an HTTP response, or from
// a single <meta http-equiv="Content-Security-Policy"...> element).
struct CONTENT_EXPORT ContentSecurityPolicyHeader {
  ContentSecurityPolicyHeader();
  ContentSecurityPolicyHeader(const std::string& header_value,
                              network::mojom::ContentSecurityPolicyType type,
                              blink::WebContentSecurityPolicySource source);

  std::string header_value;
  network::mojom::ContentSecurityPolicyType type;
  blink::WebContentSecurityPolicySource source;
};

}  // namespace content

#endif  // CONTENT_COMMON_CONTENT_SECURITY_POLICY_HEADER_
