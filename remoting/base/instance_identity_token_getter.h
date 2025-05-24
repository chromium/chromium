// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_INSTANCE_IDENTITY_TOKEN_GETTER_H_
#define REMOTING_BASE_INSTANCE_IDENTITY_TOKEN_GETTER_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "remoting/base/compute_engine_service_client.h"
#include "remoting/base/http_status.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

// InstanceIdentityTokenGetter caches instance identity tokens for Compute
// Engine service requests and refreshes them as needed.
class InstanceIdentityTokenGetter {
 public:
  using TokenCallback = base::OnceCallback<void(std::string_view)>;

  InstanceIdentityTokenGetter(
      std::string_view audience,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  InstanceIdentityTokenGetter(const InstanceIdentityTokenGetter&) = delete;
  InstanceIdentityTokenGetter& operator=(const InstanceIdentityTokenGetter&) =
      delete;

  ~InstanceIdentityTokenGetter();

  // Calls |on_token| with an identity token, or empty in the case the request
  // fails. The token returned has a lifetime of at least 10 minutes and should
  // not be cached.
  void RetrieveToken(TokenCallback on_token);

 private:
  void OnTokenRetrieved(const HttpStatus& response);

  // An identifier which is embedded in the identity token. Typically this will
  // be the URL of the API which the token used to access but could be an
  // arbitrary identifier or string.
  const std::string audience_;

  // The instance identity token from the last successful fetch operation.
  std::string identity_token_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The point at which the token should be refreshed.
  base::Time token_expiration_time_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The set of callbacks to run after fetching an updated identity token.
  std::vector<TokenCallback> queued_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Used to request an instance identity token.
  ComputeEngineServiceClient compute_engine_service_client_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<InstanceIdentityTokenGetter> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_BASE_INSTANCE_IDENTITY_TOKEN_GETTER_H_
