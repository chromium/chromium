// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CLIENT_AUTHENTICATION_CONFIG_H_
#define REMOTING_PROTOCOL_CLIENT_AUTHENTICATION_CONFIG_H_

#include <string>

#include "base/callback.h"
#include "remoting/protocol/token_validator.h"

namespace remoting::protocol {

typedef base::RepeatingCallback<void(const std::string& secret)>
    SecretFetchedCallback;
typedef base::RepeatingCallback<void(
    bool pairing_supported,
    const SecretFetchedCallback& secret_fetched_callback)>
    FetchSecretCallback;

// Callback passed to |FetchTokenCallback|, and called once the client
// authentication finishes. |token| is an opaque string that should be sent
// directly to the host. |validation_result.success()| should be used by the
// client to create a V2Authenticator. In case of failure, the callback is
// called with an empty |token| and |validation_result.is_error()| is true.
typedef base::RepeatingCallback<void(
    const std::string& token,
    const TokenValidator::ValidationResult& validation_result)>
    ThirdPartyTokenFetchedCallback;

// Fetches a third party token from |token_url|. |host_public_key| is sent to
// the server so it can later authenticate the host. |scope| is a string with a
// space-separated list of attributes for this connection (e.g.
// "hostjid:abc@example.com/123 clientjid:def@example.org/456".
// |token_fetched_callback| is called when the client authentication ends, on
// the same thread on which FetchThirdPartyTokenCallback was originally called.
typedef base::RepeatingCallback<void(
    const std::string& token_url,
    const std::string& scope,
    const ThirdPartyTokenFetchedCallback& token_fetched_callback)>
    FetchThirdPartyTokenCallback;

struct ClientAuthenticationConfig {
  ClientAuthenticationConfig();
  ClientAuthenticationConfig(const ClientAuthenticationConfig& other);
  ~ClientAuthenticationConfig();

  // Used for all authenticators.
  std::string host_id;

  // Used for pairing authenticators
  std::string pairing_client_id;
  std::string pairing_secret;

  // Used for shared secret authenticators.
  FetchSecretCallback fetch_secret_callback;

  // Used for third party authenticators.
  FetchThirdPartyTokenCallback fetch_third_party_token_callback;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_CLIENT_AUTHENTICATION_CONFIG_H_
