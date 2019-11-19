// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_MANAGER_H_
#define SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/component_export.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/origin_policy/origin_policy_constants.h"
#include "services/network/origin_policy/origin_policy_header_values.h"
#include "services/network/public/mojom/origin_policy_manager.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {

class NetworkContext;
class OriginPolicyFetcher;

// The OriginPolicyManager is the entry point for all Origin Policy related
// API calls. Spec: https://wicg.github.io/origin-policy/
// A client will likely call AddBinding (or use the NetworkContext function)
// and then do mojom::OriginPolicy related operations, like retrieving a policy
// (which could potentially trigger a fetch), or adding an exception.
class COMPONENT_EXPORT(NETWORK_SERVICE) OriginPolicyManager
    : public mojom::OriginPolicyManager {
 public:
  // The |owner_network_context| is the owner of this object and it needs to
  // always outlive this object.
  explicit OriginPolicyManager(NetworkContext* owner_network_context);
  ~OriginPolicyManager() override;

  // Bind a receiver to this object.  Mojo messages coming through the
  // associated pipe will be served by this object.
  void AddReceiver(mojo::PendingReceiver<mojom::OriginPolicyManager> receiver);

  // mojom::OriginPolicyManager
  void RetrieveOriginPolicy(const url::Origin& origin,
                            const std::string& header_value,
                            RetrieveOriginPolicyCallback callback) override;
  void AddExceptionFor(const url::Origin& origin) override;

  // To be called by fetcher when it has finished its work.
  // This removes the fetcher which results in the fetcher being destroyed.
  void FetcherDone(OriginPolicyFetcher* fetcher,
                   const OriginPolicy& origin_policy,
                   RetrieveOriginPolicyCallback callback);

  // Retrieves an origin's default origin policy by attempting to fetch it
  // from "<origin>/.well-known/origin-policy".
  void RetrieveDefaultOriginPolicy(const url::Origin& origin,
                                   RetrieveOriginPolicyCallback callback);

  // Attempts to report a policy retrieval failure. Does nothing if
  // `header_info` has an empty `report_to` member.
  void MaybeReport(OriginPolicyState state,
                   const OriginPolicyHeaderValues& header_info,
                   const GURL& policy_url);

  // ForTesting methods
  mojo::ReceiverSet<mojom::OriginPolicyManager>& GetReceiversForTesting() {
    return receivers_;
  }

  static OriginPolicyHeaderValues
  GetRequestedPolicyAndReportGroupFromHeaderStringForTesting(
      const std::string& header_value) {
    return GetRequestedPolicyAndReportGroupFromHeaderString(header_value);
  }

  // Get the version used for exempted policies. For testing purposes only.
  static const char* GetExemptedVersionForTesting();

 private:
  using KnownVersionMap = std::map<url::Origin, std::string>;

  // Parses a header and returns the result. If a parsed result does not contain
  // a non-empty policy version it means the `header_value` is invalid.
  static OriginPolicyHeaderValues
  GetRequestedPolicyAndReportGroupFromHeaderString(
      const std::string& header_value);

  // Returns an origin policy with the specified state. The contents is empty
  // and the `policy_url` is the default policy url for the specified origin.
  static void InvokeCallbackWithPolicyState(
      const url::Origin& origin,
      OriginPolicyState state,
      RetrieveOriginPolicyCallback callback);

  // Owner of this object. It needs to always outlive this object.
  // Used for queueing reports and creating a URLLoaderFactory.
  NetworkContext* const owner_network_context_;

  // In memory cache of current policy version per origin.
  // TODO(andypaicu): clear this when the disk cache is cleaned.
  KnownVersionMap latest_version_map_;

  // A list of fetchers owned by this object
  std::set<std::unique_ptr<OriginPolicyFetcher>, base::UniquePtrComparator>
      origin_policy_fetchers_;

  // Used for fetching requests
  mojo::Remote<mojom::URLLoaderFactory> url_loader_factory_;

  // This object's set of receivers.
  // This MUST be below origin_policy_fetchers_ to ensure it is destroyed before
  // it. Otherwise it's possible that un-invoked OnceCallbacks owned by members
  // of origin_policy_fetchers_ will be destroyed before the receiver they are
  // on is destroyed.
  mojo::ReceiverSet<mojom::OriginPolicyManager> receivers_;

  DISALLOW_COPY_AND_ASSIGN(OriginPolicyManager);
};

}  // namespace network

#endif  // SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_MANAGER_H_
