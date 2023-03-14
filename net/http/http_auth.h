// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_H_
#define NET_HTTP_HTTP_AUTH_H_

#include <memory>
#include <set>
#include <string>

#include "base/values.h"
#include "net/base/auth.h"
#include "net/base/net_export.h"

template <class T> class scoped_refptr;

namespace url {
class SchemeHostPort;
}

namespace net {

class HttpAuthHandler;
class HttpAuthHandlerFactory;
class HttpResponseHeaders;
class HostResolver;
class NetLogWithSource;
class NetworkAnonymizationKey;
class SSLInfo;

// Utility class for http authentication.
class NET_EXPORT_PRIVATE HttpAuth {
 public:
  // Http authentication can be done to the proxy server, origin server,
  // or both. This enum tracks who the target is.
  enum Target {
    AUTH_NONE = -1,
    // We depend on the valid targets (!= AUTH_NONE) being usable as indexes
    // in an array, so start from 0.
    AUTH_PROXY = 0,
    AUTH_SERVER = 1,
    AUTH_NUM_TARGETS = 2,
  };

  // What the HTTP WWW-Authenticate/Proxy-Authenticate headers indicate about
  // the previous authorization attempt.
  enum AuthorizationResult {
    AUTHORIZATION_RESULT_ACCEPT,   // The authorization attempt was accepted,
                                   // although there still may be additional
                                   // rounds of challenges.

    AUTHORIZATION_RESULT_REJECT,   // The authorization attempt was rejected.

    AUTHORIZATION_RESULT_STALE,    // (Digest) The nonce used in the
                                   // authorization attempt is stale, but
                                   // otherwise the attempt was valid.

    AUTHORIZATION_RESULT_INVALID,  // The authentication challenge headers are
                                   // poorly formed (the authorization attempt
                                   // itself may have been fine).

    AUTHORIZATION_RESULT_DIFFERENT_REALM,  // The authorization
                                           // attempt was rejected,
                                           // but the realm associated
                                           // with the new challenge
                                           // is different from the
                                           // previous attempt.
  };

  // Describes where the identity used for authentication came from.
  enum IdentitySource {
    // Came from nowhere -- the identity is not initialized.
    IDENT_SRC_NONE,

    // The identity came from the auth cache, by doing a path-based
    // lookup (premptive authorization).
    IDENT_SRC_PATH_LOOKUP,

    // The identity was extracted from a URL of the form:
    // http://<username>:<password>@host:port
    IDENT_SRC_URL,

    // The identity was retrieved from the auth cache, by doing a
    // realm lookup.
    IDENT_SRC_REALM_LOOKUP,

    // The identity was provided by RestartWithAuth -- it likely
    // came from a prompt (or maybe the password manager).
    IDENT_SRC_EXTERNAL,

    // The identity used the default credentials for the computer,
    // on schemes that support single sign-on.
    IDENT_SRC_DEFAULT_CREDENTIALS,
  };

  // Identifier for auth scheme.
  //
  // The values are used for calculating UMA buckets. Add but don't remove or
  // reuse.
  enum Scheme {
    AUTH_SCHEME_BASIC = 0,
    AUTH_SCHEME_DIGEST,
    AUTH_SCHEME_NTLM,
    AUTH_SCHEME_NEGOTIATE,
    AUTH_SCHEME_SPDYPROXY,  // No longer used.
    AUTH_SCHEME_MOCK,
    AUTH_SCHEME_MAX,
  };

  // Type of Kerberos credentials delegation to be performed during
  // authentication.
  enum class DelegationType {
    // Disallow delegation.
    kNone,
    // Delegate if approved by KDC policy. Implemented in GSSAPI.
    kByKdcPolicy,
    // Unconstrained delegation. On Windows, both kByKdcPolicy and
    // kUnconstrained will check KDC policy.
    kUnconstrained,
  };

  // Helper structure used by HttpNetworkTransaction to track
  // the current identity being used for authorization.
  struct Identity {
    Identity();

    IdentitySource source = IDENT_SRC_NONE;
    bool invalid = true;
    AuthCredentials credentials;
  };

  // Get the name of the header containing the auth challenge
  // (either WWW-Authenticate or Proxy-Authenticate).
  static std::string GetChallengeHeaderName(Target target);

  // Get the name of the header where the credentials go
  // (either Authorization or Proxy-Authorization).
  static std::string GetAuthorizationHeaderName(Target target);

  // Returns a string representation of a Target value that can be used in log
  // messages.
  static std::string GetAuthTargetString(Target target);

  // Returns a string representation of an authentication Scheme.
  static const char* SchemeToString(Scheme scheme);

  // Returns an authentication Scheme from a string which was produced by
  // SchemeToString().
  static Scheme StringToScheme(const std::string& str);

  // Returns a string representation of an authorization result.
  static const char* AuthorizationResultToString(
      AuthorizationResult authorization_result);

  // Returns a value for logging an authorization result to a NetLog.
  static base::Value::Dict NetLogAuthorizationResultParams(
      const char* name,
      AuthorizationResult authorization_result);

  // Iterate through |response_headers|, and pick the best one that we support.
  // Obtains the implementation class for handling the challenge, and passes it
  // back in |*handler|. If no supported challenge was found, |*handler| is set
  // to nullptr.
  //
  // |disabled_schemes| is the set of schemes that we should not use.
  //
  // |scheme_host_port| is used by the NTLM and Negotiation authentication
  // scheme to construct the service principal name. It is ignored by other
  // schemes.
  //
  // |ssl_info| is passed through to the scheme specific authentication handlers
  // to use as appropriate.
  static void ChooseBestChallenge(
      HttpAuthHandlerFactory* http_auth_handler_factory,
      const HttpResponseHeaders& response_headers,
      const SSLInfo& ssl_info,
      const NetworkAnonymizationKey& network_anonymization_key,
      Target target,
      const url::SchemeHostPort& scheme_host_port,
      const std::set<Scheme>& disabled_schemes,
      const NetLogWithSource& net_log,
      HostResolver* host_resolver,
      std::unique_ptr<HttpAuthHandler>* handler);

  // Handle a 401/407 response from a server/proxy after a previous
  // authentication attempt. For connection-based authentication schemes, the
  // new response may be another round in a multi-round authentication sequence.
  // For request-based schemes, a 401/407 response is typically treated like a
  // rejection of the previous challenge, except in the Digest case when a
  // "stale" attribute is present.
  //
  // |handler| must be non-nullptr, and is the HttpAuthHandler from the previous
  // authentication round.
  //
  // |response_headers| must contain the new HTTP response.
  //
  // |target| specifies whether the authentication challenge response came
  // from a server or a proxy.
  //
  // |disabled_schemes| are the authentication schemes to ignore.
  //
  // |challenge_used| is the text of the authentication challenge used in
  // support of the returned AuthorizationResult. If no headers were used for
  // the result (for example, all headers have unknown authentication schemes),
  // the value is cleared.
  static AuthorizationResult HandleChallengeResponse(
      HttpAuthHandler* handler,
      const HttpResponseHeaders& response_headers,
      Target target,
      const std::set<Scheme>& disabled_schemes,
      std::string* challenge_used);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_H_
