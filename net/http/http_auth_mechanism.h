// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_MECHANISM_H_
#define NET_HTTP_HTTP_AUTH_MECHANISM_H_

#include <memory>

#include "base/callback_forward.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/http/http_auth.h"

namespace net {

class AuthCredentials;
class HttpAuthChallengeTokenizer;
class HttpAuthPreferences;
class NetLogWithSource;

class NET_EXPORT_PRIVATE HttpAuthMechanism {
 public:
  virtual ~HttpAuthMechanism() = default;

  virtual bool Init(const NetLogWithSource& net_log) = 0;

  // True if authentication needs the identity of the user from Chrome.
  virtual bool NeedsIdentity() const = 0;

  // True authentication can use explicit credentials included in the URL.
  virtual bool AllowsExplicitCredentials() const = 0;

  // Parse a received Negotiate challenge.
  virtual HttpAuth::AuthorizationResult ParseChallenge(
      HttpAuthChallengeTokenizer* tok) = 0;

  // Generates an authentication token.
  //
  // The return value is an error code. The authentication token will be
  // returned in |*auth_token|. If the result code is not |OK|, the value of
  // |*auth_token| is unspecified.
  //
  // If the operation cannot be completed synchronously, |ERR_IO_PENDING| will
  // be returned and the real result code will be passed to the completion
  // callback.  Otherwise the result code is returned immediately from this
  // call.
  //
  // If the AndroidAuthNegotiate object is deleted before completion then the
  // callback will not be called.
  //
  // If no immediate result is returned then |auth_token| must remain valid
  // until the callback has been called.
  //
  // |spn| is the Service Principal Name of the server that the token is
  // being generated for.
  //
  // If this is the first round of a multiple round scheme, credentials are
  // obtained using |*credentials|. If |credentials| is nullptr, the default
  // credentials are used instead.
  virtual int GenerateAuthToken(const AuthCredentials* credentials,
                                const std::string& spn,
                                const std::string& channel_bindings,
                                std::string* auth_token,
                                const NetLogWithSource& net_log,
                                CompletionOnceCallback callback) = 0;

  // Sets the delegation type allowed on the Kerberos ticket. This allows
  // certain servers to act as the user, such as an IIS server retrieving data
  // from a Kerberized MSSQL server.
  virtual void SetDelegation(HttpAuth::DelegationType delegation_type) = 0;
};

// Factory is just a callback that returns a unique_ptr.
using HttpAuthMechanismFactory =
    base::RepeatingCallback<std::unique_ptr<HttpAuthMechanism>(
        const HttpAuthPreferences*)>;

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_MECHANISM_H_
