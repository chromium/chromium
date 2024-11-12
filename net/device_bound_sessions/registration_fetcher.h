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
#include "net/device_bound_sessions/session_params.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace net {
class URLRequestContext;
}

namespace unexportable_keys {
class UnexportableKeyService;
}

namespace net::device_bound_sessions {

class RegistrationRequestParam;

// This class creates a new unexportable key, creates a registration JWT and
// signs it with the new key, and makes the network request to the DBSC
// registration endpoint with this signed JWT to get the registration
// instructions.
class NET_EXPORT RegistrationFetcher {
 public:
  struct NET_EXPORT RegistrationCompleteParams {
    RegistrationCompleteParams(
        SessionParams params,
        unexportable_keys::UnexportableKeyId key_id,
        const GURL& url,
        std::optional<std::string> referral_session_identifier);
    RegistrationCompleteParams(RegistrationCompleteParams&& other) noexcept;
    RegistrationCompleteParams& operator=(
        RegistrationCompleteParams&& other) noexcept;

    ~RegistrationCompleteParams();

    SessionParams params;
    unexportable_keys::UnexportableKeyId key_id;
    GURL url;
    // The session identifier which initiated the registration request.
    // It is `std::nullopt` for first time registration.
    std::optional<std::string> referral_session_identifier;
  };

  using RegistrationCompleteCallback =
      base::OnceCallback<void(std::optional<RegistrationCompleteParams>)>;

  using FetcherType = std::optional<RegistrationCompleteParams> (*)();

  // TODO(kristianm): Add more parameters when the returned JSON is parsed.
  struct NET_EXPORT RegistrationTokenResult {
    std::string registration_token;
    unexportable_keys::UnexportableKeyId key_id;
  };

  // Creates an unexportable key from the key service, creates a registration
  // JWT and signs it with the new key. Starts the network request to the DBSC
  // registration endpoint with the signed JWT in the header. `callback`
  // is called with the fetch results upon completion.
  // This can fail during key creation, signing and during the network request,
  // and if so it the callback with be called with a std::nullopt.
  static void StartCreateTokenAndFetch(
      RegistrationFetcherParam registration_params,
      unexportable_keys::UnexportableKeyService& key_service,
      const URLRequestContext* context,
      const IsolationInfo& isolation_info,
      RegistrationCompleteCallback callback);

  // Starts the network request to the DBSC refresh endpoint with existing key
  // id. `callback` is called with the fetch results upon completion. This can
  // fail during signing and during the network request, and if so the callback
  // will be called with a std::nullopt.
  static void StartFetchWithExistingKey(
      RegistrationRequestParam request_params,
      unexportable_keys::UnexportableKeyService& key_service,
      const URLRequestContext* context,
      const IsolationInfo& isolation_info,
      RegistrationCompleteCallback callback,
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
          key_id);

  // Helper function for generating a new binding key and a registration token
  // to bind the key on the server. unexportable_key_service must outlive the
  // callback result
  static void CreateTokenAsyncForTesting(
      unexportable_keys::UnexportableKeyService& unexportable_key_service,
      std::string challenge,
      const GURL& registration_url,
      std::optional<std::string> authorization,
      base::OnceCallback<
          void(std::optional<RegistrationFetcher::RegistrationTokenResult>)>
          callback);

  static void SetFetcherForTesting(FetcherType);
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_REGISTRATION_FETCHER_H_
