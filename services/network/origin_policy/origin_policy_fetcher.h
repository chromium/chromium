// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_FETCHER_H_
#define SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "net/base/isolation_info.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/origin_policy_manager.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/origin.h"

namespace network {

class OriginPolicyManager;

class COMPONENT_EXPORT(NETWORK_SERVICE) OriginPolicyFetcher {
 public:
  // Constructs a fetcher that attempts to retrieve the current policy for
  // the specified origin by fetching from /.well-known/origin-policy
  // Spec: https://wicg.github.io/origin-policy/#update-an-origins-origin-policy
  // although we are currently updating the implementation (crbug.com/751996)
  // and so for now only implement a few steps of that.
  OriginPolicyFetcher(
      OriginPolicyManager* owner_policy_manager,
      const url::Origin& origin,
      const net::IsolationInfo& isolation_info,
      mojom::URLLoaderFactory* factory,
      mojom::OriginPolicyManager::RetrieveOriginPolicyCallback callback);

  ~OriginPolicyFetcher();

  static GURL GetPolicyURL(const url::Origin& origin);

 private:
  using FetchCallback = base::OnceCallback<void(std::unique_ptr<std::string>)>;

  void OnResponseStarted(const GURL& final_url,
                         const mojom::URLResponseHead& response_head);

  void OnPolicyHasArrived(std::unique_ptr<std::string> policy_content);
  void FetchPolicy(mojom::URLLoaderFactory* factory);

  // The owner of this object. When it is destroyed, this is destroyed too.
  OriginPolicyManager* const owner_policy_manager_;

  // We may need the SimpleURLLoader to download the policy. The loader must
  // be kept alive while the load is ongoing.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  GURL fetch_url_;
  net::IsolationInfo isolation_info_;

  // Called back with policy fetch result.
  mojom::OriginPolicyManager::RetrieveOriginPolicyCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(OriginPolicyFetcher);
};

}  // namespace network

#endif  // SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_FETCHER_H_
