// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_H_

#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace network {

// This class is a thin wrapper around mojom::ContentSecurityPolicy.
class COMPONENT_EXPORT(NETWORK_CPP) ContentSecurityPolicy {
 public:
  ContentSecurityPolicy();
  ~ContentSecurityPolicy();

  explicit ContentSecurityPolicy(
      mojom::ContentSecurityPolicyPtr content_security_policy_ptr);
  ContentSecurityPolicy(const ContentSecurityPolicy& other);
  ContentSecurityPolicy(ContentSecurityPolicy&& other);
  ContentSecurityPolicy& operator=(const ContentSecurityPolicy& other);

  // TODO(lfg): Temporary until network::ResourceResponseHead is converted to
  // mojom.
  operator mojom::ContentSecurityPolicyPtr() const;

  // Parses the Content-Security-Policy headers specified in |headers| while
  // requesting |request_url|. The |request_url| is used for violation
  // reporting, as specified in
  // https://w3c.github.io/webappsec-csp/#report-violation.
  bool Parse(const GURL& request_url, const net::HttpResponseHeaders& headers);

  // Parses a Content-Security-Policy |header|.
  bool Parse(const GURL& base_url, base::StringPiece header);

  const mojom::ContentSecurityPolicyPtr& content_security_policy_ptr() {
    return content_security_policy_ptr_;
  }
  mojom::ContentSecurityPolicyPtr TakeContentSecurityPolicy() {
    return std::move(content_security_policy_ptr_);
  }

 private:

  // Parses the frame-ancestor directive of a Content-Security-Policy header.
  bool ParseFrameAncestors(base::StringPiece header_value);

  // Parses the report-uri directive of a Content-Security-Policy header.
  bool ParseReportEndpoint(const GURL& base_url,
                           base::StringPiece header_value);

  mojom::ContentSecurityPolicyPtr content_security_policy_ptr_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_H_
