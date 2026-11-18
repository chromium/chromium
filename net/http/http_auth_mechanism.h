// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_MECHANISM_H_
#define NET_HTTP_HTTP_AUTH_MECHANISM_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
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

  // True if authentication can use explicit credentials included in the URL or
  // the user may be prompted for credentials.
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
  // If the mechanism object is deleted before completion then the callback
  // will not be called.
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

  // Reasons why `GetTargetPrincipalHost()` doesn't return a host.
  enum class TargetPrincipalError {
    // The mechanism doesn't report a Kerberos-style target principal
    // (including incomplete SPNEGO contexts with `GSS_C_NO_NAME` or
    // `GSS_S_NO_CONTEXT`, and NTLMSSP negotiated via SPNEGO).
    kNotReported,
    // Querying or displaying the target principal of an established context
    // failed, or returned an empty name.
    kQueryFailed,
    // The target principal isn't an `HTTP` or `host` service principal (for
    // example, a user principal name or another service).
    kNonHttpServicePrincipal,
    // The target principal contains characters that can't be parsed
    // unambiguously (NULs or backslash escapes), or its host part is malformed
    // or not already in canonical form.
    kUnparsableHost,
  };

  // Returns the canonical host part of the `HTTP` or `host` service principal
  // that the security context was established with (for example, `<host>` in
  // `HTTP/<host>@REALM`, `host/<host>@REALM`, or `HTTP@<host>` for hostbased
  // service names), which can differ from the host in the SPN passed to
  // `GenerateAuthToken()`. Otherwise, returns why no host is available.
  //
  // Only `HttpAuthGSSAPI` (POSIX) overrides this: GSSAPI libraries such as MIT
  // krb5 can canonicalize a `GSS_C_NT_HOSTBASED_SERVICE` name through DNS
  // (CNAME and/or reverse PTR lookups) inside `gss_init_sec_context()`, and
  // `gss_inquire_context()` reports the resulting principal. `HttpAuthSSPI`
  // (Windows) doesn't override it yet, though SSPI can report the server name
  // through `SECPKG_ATTR_NATIVE_NAMES` (currently only logged to the NetLog).
  // `HttpAuthNegotiateAndroid` passes the SPN to an external authenticator
  // app that only returns an opaque token, so it can't report the principal.
  virtual base::expected<std::string, TargetPrincipalError>
  GetTargetPrincipalHost() const;

  // Discards the security context so that the next `GenerateAuthToken()` call
  // starts a new handshake, for example to regenerate an unsent first-round
  // token after `SetDelegation()`. Returns false if the mechanism doesn't
  // support this; only `HttpAuthGSSAPI` (POSIX) does, since it is the only
  // mechanism that overrides `GetTargetPrincipalHost()`.
  virtual bool ResetSecurityContext();
};

// Factory is just a callback that returns a unique_ptr.
using HttpAuthMechanismFactory =
    base::RepeatingCallback<std::unique_ptr<HttpAuthMechanism>(
        const HttpAuthPreferences*)>;

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_MECHANISM_H_
