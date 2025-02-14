// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_COMPUTE_ENGINE_SERVICE_CLIENT_H_
#define REMOTING_BASE_COMPUTE_ENGINE_SERVICE_CLIENT_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "net/http/http_status_code.h"
#include "remoting/base/http_status.h"

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

// A service client that communicates with the Compute Engine API. Note that the
// methods must be called from code running within a GCE VM Instance as the API
// does not exist in other contexts.
class ComputeEngineServiceClient {
 public:
  using ResponseCallback = base::OnceCallback<void(const HttpStatus&)>;

  explicit ComputeEngineServiceClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ComputeEngineServiceClient(const ComputeEngineServiceClient&) = delete;
  ComputeEngineServiceClient& operator=(const ComputeEngineServiceClient&) =
      delete;

  ~ComputeEngineServiceClient();

  // Must be called from code running within a Compute Engine Instance.
  // Uses the default service account associated with the Instance.
  // More information on this request can be found at:
  // https://cloud.google.com/compute/docs/instances/verifying-instance-identity
  void GetInstanceIdentityToken(std::string_view audience,
                                ResponseCallback callback);

  // Must be called from code running within a Compute Engine Instance.
  // Retrieves an OAuth access token for the default service account associated
  // with the Compute Engine Instance.
  void GetServiceAccountAccessToken(ResponseCallback callback);

  // Must be called from code running within a Compute Engine Instance.
  // Retrieves the set of OAuth scopes present in access tokens generated for
  // the default service account associated with the Compute Engine Instance.
  void GetServiceAccountScopes(ResponseCallback callback);

  void CancelPendingRequests();

 private:
  void ExecuteRequest(
      std::string_view url,
      const net::NetworkTrafficAnnotationTag& network_annotation,
      ResponseCallback callback);

  void OnRequestComplete(ResponseCallback callback,
                         std::optional<std::string> response_body);

  // |url_loader_| is non-null when a request is in-flight.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<ComputeEngineServiceClient> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_BASE_COMPUTE_ENGINE_SERVICE_CLIENT_H_
