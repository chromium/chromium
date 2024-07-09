// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CLIENT_AUTHENTICATION_CONFIG_H_
#define REMOTING_PROTOCOL_CLIENT_AUTHENTICATION_CONFIG_H_

#include <string>

#include "base/functional/callback.h"

namespace remoting::protocol {

typedef base::RepeatingCallback<void(const std::string& secret)>
    SecretFetchedCallback;
typedef base::RepeatingCallback<void(
    bool pairing_supported,
    const SecretFetchedCallback& secret_fetched_callback)>
    FetchSecretCallback;

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
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_CLIENT_AUTHENTICATION_CONFIG_H_
