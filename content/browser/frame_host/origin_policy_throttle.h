// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_ORIGIN_POLICY_THROTTLE_H_
#define CONTENT_BROWSER_FRAME_HOST_ORIGIN_POLICY_THROTTLE_H_

#include <map>
#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "content/public/browser/navigation_throttle.h"

class GURL;

namespace url {
class Origin;
}
namespace net {
struct RedirectInfo;
}  // namespace net
namespace network {
struct ResourceResponseHead;
class SimpleURLLoader;
}  // namespace network

namespace content {
class NavigationHandle;
enum class OriginPolicyErrorReason;

// The OriginPolicyThrottle is responsible for deciding whether an origin
// policy should be fetched, and doing so when that is positive.
//
// The intended use is that the navigation request will
// - call OriginPolicyThrottle::ShouldRequestOriginPolicy to determine whether
//   a policy should be requested and which version, and should add the
//   appropriate SecOriginPolicy: header.
// - call OriginPolicyThrottle::MaybeCreateThrottleFor a given navigation.
//   This will use presence of the header to decide whether to create a
//   throttle or not.
class CONTENT_EXPORT OriginPolicyThrottle : public NavigationThrottle {
 public:
  // Determine whether to request a policy (or advertise origin policy
  // support) and which version.
  // Returns whether the policy header should be sent. It it returns true,
  // |version| will contain the policy version to use.
  static bool ShouldRequestOriginPolicy(const GURL& url, std::string* version);

  // Create a throttle (if the request contains the appropriate header.
  // The throttle will handle fetching of the policy and updating the
  // navigation request with the result.
  static std::unique_ptr<NavigationThrottle> MaybeCreateThrottleFor(
      NavigationHandle* handle);

  ~OriginPolicyThrottle() override;

  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

  using KnownVersionMap = std::map<url::Origin, std::string>;
  static KnownVersionMap& GetKnownVersionsForTesting();

  void InjectPolicyForTesting(const std::string& policy_content);

 private:
  using FetchCallback = base::OnceCallback<void(std::unique_ptr<std::string>)>;
  using RedirectCallback =
      base::RepeatingCallback<void(const net::RedirectInfo&,
                                   const network::ResourceResponseHead&,
                                   std::vector<std::string>*)>;

  explicit OriginPolicyThrottle(NavigationHandle* handle);

  static KnownVersionMap& GetKnownVersions();

  const url::Origin GetRequestOrigin();
  void FetchPolicy(const GURL& url,
                   FetchCallback done,
                   RedirectCallback redirect);
  void OnTheGloriousPolicyHasArrived(
      std::unique_ptr<std::string> policy_content);
  void OnRedirect(const net::RedirectInfo& redirect_info,
                  const network::ResourceResponseHead& response_head,
                  std::vector<std::string>* to_be_removed_headers);
  void CancelNavigation(OriginPolicyErrorReason reason);

  // We may need the SimpleURLLoader to download the policy. The loader must
  // be kept alive while the load is ongoing.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  DISALLOW_COPY_AND_ASSIGN(OriginPolicyThrottle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_ORIGIN_POLICY_THROTTLE_H_
