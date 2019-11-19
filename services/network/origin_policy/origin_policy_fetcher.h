// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_FETCHER_H_
#define SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "net/url_request/redirect_info.h"
#include "services/network/origin_policy/origin_policy_header_values.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/origin_policy_manager.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/origin.h"

namespace network {

class OriginPolicyManager;

class COMPONENT_EXPORT(NETWORK_SERVICE) OriginPolicyFetcher {
 public:
  // Constructs a fetcher that attempts to retrieve the policy of the specified
  // origin using the specified policy_version.
  OriginPolicyFetcher(
      OriginPolicyManager* owner_policy_manager,
      const OriginPolicyHeaderValues& header_info,
      const url::Origin& origin,
      mojom::URLLoaderFactory* factory,
      mojom::OriginPolicyManager::RetrieveOriginPolicyCallback callback);

  // Constructs a fetcher that attempts to retrieve the current policy for
  // the specified origin by fetching from /.well-known/origin-policy
  // Spec: https://wicg.github.io/origin-policy/#origin-policy-well-known
  OriginPolicyFetcher(
      OriginPolicyManager* owner_policy_manager,
      const url::Origin& origin,
      mojom::URLLoaderFactory* factory,
      mojom::OriginPolicyManager::RetrieveOriginPolicyCallback callback);

  ~OriginPolicyFetcher();

  static GURL GetPolicyURL(const std::string& version,
                           const url::Origin& origin);
  static GURL GetDefaultPolicyURL(const url::Origin& origin);

  // ForTesting methods.
  bool IsValidRedirectForTesting(const net::RedirectInfo& redirect_info) const;

 private:
  using FetchCallback = base::OnceCallback<void(std::unique_ptr<std::string>)>;
  using RedirectCallback =
      base::RepeatingCallback<void(const net::RedirectInfo&,
                                   const mojom::URLResponseHead&,
                                   std::vector<std::string>*)>;

  void OnPolicyHasArrived(std::unique_ptr<std::string> policy_content);
  void OnPolicyRedirect(const net::RedirectInfo& redirect_info,
                        const mojom::URLResponseHead& response_head,
                        std::vector<std::string>* to_be_removed_headers);
  void FetchPolicy(mojom::URLLoaderFactory* factory);

  void WorkDone(std::unique_ptr<std::string> policy_content,
                OriginPolicyState state);

  bool IsValidRedirect(const net::RedirectInfo& redirect_info) const;

  // The owner of this object. When it is destroyed, this is destroyed too.
  OriginPolicyManager* const owner_policy_manager_;

  // We may need the SimpleURLLoader to download the policy. The loader must
  // be kept alive while the load is ongoing.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Used for testing if a redirect is valid.
  GURL fetch_url_;

  // Called back with policy fetch result.
  mojom::OriginPolicyManager::RetrieveOriginPolicyCallback callback_;

  // Will be true if we started a fetch at <origin>/well-known/origin-policy
  // which must redirect to the latest origin policy.
  bool must_redirect_;

  // The header info parsed from the received `Sec-Origin-Policy` header. Empty
  // if no header is present.
  OriginPolicyHeaderValues header_info_;

  DISALLOW_COPY_AND_ASSIGN(OriginPolicyFetcher);
};

}  // namespace network

#endif  // SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_FETCHER_H_
