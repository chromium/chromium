// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_ANCESTOR_THROTTLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_ANCESTOR_THROTTLE_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "content/public/browser/navigation_throttle.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "services/network/public/mojom/parsed_headers.mojom-forward.h"

class GURL;

namespace url {
class Origin;
}

namespace net {
class HttpResponseHeaders;
}

namespace content {
class NavigationHandle;

// An AncestorThrottle is responsible for enforcing a resource's embedding
// rules, and blocking requests which violate them.
class CONTENT_EXPORT AncestorThrottle : public NavigationThrottle {
 public:
  enum class HeaderDisposition {
    NONE = 0,
    DENY,
    SAMEORIGIN,
    ALLOWALL,
    INVALID,
    CONFLICT
  };

  static std::unique_ptr<NavigationThrottle> MaybeCreateThrottleFor(
      NavigationHandle* handle);

  ~AncestorThrottle() override;

  NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override;
  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  enum class LoggingDisposition { LOG_TO_CONSOLE, DO_NOT_LOG_TO_CONSOLE };
  enum class CheckResult { BLOCK, PROCEED };

  FRIEND_TEST_ALL_PREFIXES(AncestorThrottleTest, ParsingXFrameOptions);
  FRIEND_TEST_ALL_PREFIXES(AncestorThrottleTest, ErrorsParsingXFrameOptions);
  FRIEND_TEST_ALL_PREFIXES(AncestorThrottleTest,
                           IgnoreWhenFrameAncestorsPresent);
  FRIEND_TEST_ALL_PREFIXES(AncestorThrottleTest,
                           AllowsBlanketEnforcementOfRequiredCSP);

  explicit AncestorThrottle(NavigationHandle* handle);
  NavigationThrottle::ThrottleCheckResult ProcessResponseImpl(
      LoggingDisposition logging,
      bool is_response_check);
  void ParseXFrameOptionsError(const std::string& value,
                               HeaderDisposition disposition);
  void ConsoleErrorXFrameOptions(HeaderDisposition disposition);
  CheckResult EvaluateXFrameOptions(LoggingDisposition logging);
  CheckResult EvaluateFrameAncestors(
      const std::vector<network::mojom::ContentSecurityPolicyPtr>&
          content_security_policy);
  CheckResult EvaluateCSPEmbeddedEnforcement();
  static bool AllowsBlanketEnforcementOfRequiredCSP(
      const url::Origin& request_origin,
      const GURL& response_url,
      const network::mojom::AllowCSPFromHeaderValuePtr& allow_csp_from);

  // Parses an 'X-Frame-Options' header. If the result is either CONFLICT
  // or INVALID, |header_value| will be populated with the value which caused
  // the parse error.
  HeaderDisposition ParseXFrameOptionsHeader(
      const net::HttpResponseHeaders* headers,
      std::string* header_value);

  DISALLOW_COPY_AND_ASSIGN(AncestorThrottle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_ANCESTOR_THROTTLE_H_
