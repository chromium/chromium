// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_ANCESTOR_THROTTLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_ANCESTOR_THROTTLE_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_throttle.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "services/network/public/mojom/x_frame_options.mojom-forward.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"

namespace net {
class HttpResponseHeaders;
}

namespace content {
class NavigationHandle;

// An AncestorThrottle is responsible for enforcing a resource's embedding
// rules, and blocking requests which violate them.
class CONTENT_EXPORT AncestorThrottle : public NavigationThrottle {
 public:
  static std::unique_ptr<NavigationThrottle> MaybeCreateThrottleFor(
      NavigationHandle* handle);

  AncestorThrottle(const AncestorThrottle&) = delete;
  AncestorThrottle& operator=(const AncestorThrottle&) = delete;

  ~AncestorThrottle() override;

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

  explicit AncestorThrottle(NavigationHandle* handle);
  NavigationThrottle::ThrottleCheckResult ProcessResponseImpl(
      LoggingDisposition logging,
      bool is_response_check);
  void ParseXFrameOptionsError(const net::HttpResponseHeaders* headers,
                               network::mojom::XFrameOptionsValue disposition);
  void ConsoleErrorXFrameOptions(
      network::mojom::XFrameOptionsValue disposition);
  void ConsoleErrorEmbeddingRequiresOptIn();
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           std::string message);
  CheckResult EvaluateXFrameOptions(LoggingDisposition logging);
  CheckResult EvaluateFrameAncestors(
      const std::vector<network::mojom::ContentSecurityPolicyPtr>&
          content_security_policy);
  CheckResult EvaluateEmbeddingOptIn(LoggingDisposition logging);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_ANCESTOR_THROTTLE_H_
