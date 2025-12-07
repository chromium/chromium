// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_REGISTRATION_FETCHER_H_
#define NET_DEVICE_BOUND_SESSIONS_REGISTRATION_FETCHER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "net/base/isolation_info.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/device_bound_sessions/registration_fetcher_param.h"
#include "net/device_bound_sessions/registration_result.h"
#include "net/device_bound_sessions/session.h"
#include "net/device_bound_sessions/session_error.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log_source.h"
#include "url/gurl.h"

namespace net {
class URLRequestContext;
}

namespace unexportable_keys {
class UnexportableKeyService;
}

namespace net::device_bound_sessions {

class SessionService;
class RegistrationRequestParam;

// This class creates a new unexportable key, creates a registration JWT and
// signs it with the new key, and makes the network request to the DBSC
// registration endpoint with this signed JWT to get the registration
// instructions. It is also used for calling the refresh endpoint. It delegates
// most of the validation to `Session::CreateIfValid`, and returns a full
// `Session`, a request to leave the session config unchanged, or an error.
class NET_EXPORT RegistrationFetcher {
 public:
  using RegistrationCompleteCallback =
      base::OnceCallback<void(RegistrationFetcher*, RegistrationResult)>;

  using FetcherType =
      base::RepeatingCallback<void(RegistrationCompleteCallback)>;

  using RegistrationToken = std::string;

  // Creates a fetcher that can be used to do registration or refresh.
  static std::unique_ptr<RegistrationFetcher> CreateFetcher(
      RegistrationRequestParam& request_params,
      SessionService& session_service,
      unexportable_keys::UnexportableKeyService& key_service,
      const URLRequestContext* context,
      const IsolationInfo& isolation_info,
      std::optional<NetLogSource> net_log_source,
      const std::optional<url::Origin>& original_request_initiator);

  // Creates an unexportable key from the key service, creates a registration
  // JWT and signs it with the new key. Starts the network request to the DBSC
  // registration endpoint with the signed JWT in the header. `callback`
  // is called with the fetch results upon completion.
  // This can fail during key creation, signing and during the network request,
  // and if so it the callback with be called with a std::nullopt.
  virtual void StartCreateTokenAndFetch(
      RegistrationRequestParam& registration_params,
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          supported_algos,
      RegistrationCompleteCallback callback) = 0;

  // Starts the network request to the DBSC refresh endpoint with existing key
  // id. `callback` is called with the fetch results upon completion. This can
  // fail during signing and during the network request, and if so the callback
  // will be called with a std::nullopt.
  virtual void StartFetchWithExistingKey(
      RegistrationRequestParam& request_params,
      unexportable_keys::UnexportableKeyId key_id,
      RegistrationCompleteCallback callback) = 0;

  // Starts the network request to the DBSC registration endpoint for a
  // federated session. `callback` is called with the fetch results upon
  // completion.
  virtual void StartFetchWithFederatedKey(
      RegistrationRequestParam& request_params,
      unexportable_keys::UnexportableKeyId key_id,
      const GURL& provider_url,
      RegistrationCompleteCallback callback) = 0;

  // Helper function for generating a new binding key and a registration token
  // to bind the key on the server. unexportable_key_service must outlive the
  // callback result
  static void CreateRegistrationTokenAsyncForTesting(
      unexportable_keys::UnexportableKeyService& unexportable_key_service,
      std::string challenge,
      std::optional<std::string> authorization,
      base::OnceCallback<void(std::optional<RegistrationToken>)> callback);

  static void SetFetcherForTesting(FetcherType* fetcher);

  virtual ~RegistrationFetcher() = default;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_REGISTRATION_FETCHER_H_
