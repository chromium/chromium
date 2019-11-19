// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_H_
#define NET_HTTP_HTTP_AUTH_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/http/http_auth.h"
#include "net/log/net_log_with_source.h"

namespace net {

class HttpAuthChallengeTokenizer;
struct HttpRequestInfo;
class SSLInfo;

// HttpAuthHandler is the interface for the authentication schemes
// (basic, digest, NTLM, Negotiate).
// HttpAuthHandler objects are typically created by an HttpAuthHandlerFactory.
//
// HttpAuthHandlers and generally created and managed by an HttpAuthController,
// which is the interaction point between the rest of net and the HTTP auth
// code.
//
// For connection-based authentication, an HttpAuthHandler handles all rounds
// related to using a single identity. If the identity is rejected, a new
// HttpAuthHandler must be created.
class NET_EXPORT_PRIVATE HttpAuthHandler {
 public:
  HttpAuthHandler();
  virtual ~HttpAuthHandler();

  // Initializes the handler using a challenge issued by a server.
  //
  // |challenge| must be non-nullptr and have already tokenized the
  //      authentication scheme, but none of the tokens occurring after the
  //      authentication scheme.
  // |target| and |origin| are both stored for later use, and are not part of
  //      the initial challenge.
  // |ssl_info| must be valid if the underlying connection used a certificate.
  // |net_log| to be used for logging.
  bool InitFromChallenge(HttpAuthChallengeTokenizer* challenge,
                         HttpAuth::Target target,
                         const SSLInfo& ssl_info,
                         const GURL& origin,
                         const NetLogWithSource& net_log);

  // Determines how the previous authorization attempt was received.
  //
  // This is called when the server/proxy responds with a 401/407 after an
  // earlier authorization attempt. Although this normally means that the
  // previous attempt was rejected, in multi-round schemes such as
  // NTLM+Negotiate it may indicate that another round of challenge+response
  // is required. For Digest authentication it may also mean that the previous
  // attempt used a stale nonce (and nonce-count) and that a new attempt should
  // be made with a different nonce provided in the challenge.
  //
  // |challenge| must be non-nullptr and have already tokenized the
  // authentication scheme, but none of the tokens occurring after the
  // authentication scheme.
  HttpAuth::AuthorizationResult HandleAnotherChallenge(
      HttpAuthChallengeTokenizer* challenge);

  // Generates an authentication token, potentially asynchronously.
  //
  // When |credentials| is nullptr, the default credentials for the currently
  // logged in user are used. |AllowsDefaultCredentials()| MUST be true in this
  // case.
  //
  // |request|, |callback|, and |auth_token| must be non-nullptr.
  //
  // The return value is a net error code.
  //
  // If |OK| is returned, |*auth_token| is filled in with an authentication
  // token which can be inserted in the HTTP request.
  //
  // If |ERR_IO_PENDING| is returned, |*auth_token| will be filled in
  // asynchronously and |callback| will be invoked. The lifetime of
  // |request|, |callback|, and |auth_token| must last until |callback| is
  // invoked, but |credentials| is only used during the initial call.
  //
  // All other return codes indicate that there was a problem generating a
  // token, and the value of |*auth_token| is unspecified.
  int GenerateAuthToken(const AuthCredentials* credentials,
                        const HttpRequestInfo* request,
                        CompletionOnceCallback callback,
                        std::string* auth_token);

  // The authentication scheme as an enumerated value.
  HttpAuth::Scheme auth_scheme() const {
    return auth_scheme_;
  }

  // The realm, encoded as UTF-8. This may be empty.
  const std::string& realm() const {
    return realm_;
  }

  // The challenge which was issued when creating the handler.
  const std::string& challenge() const { return auth_challenge_; }

  // Numeric rank based on the challenge's security level. Higher
  // numbers are better. Used by HttpAuth::ChooseBestChallenge().
  int score() const {
    return score_;
  }

  HttpAuth::Target target() const {
    return target_;
  }

  // Returns the proxy or server which issued the authentication challenge
  // that this HttpAuthHandler is handling. The URL includes scheme, host, and
  // port, but does not include path.
  const GURL& origin() const {
    return origin_;
  }

  // Returns true if the authentication scheme does not send the username and
  // password in the clear.
  bool encrypts_identity() const {
    return (properties_ & ENCRYPTS_IDENTITY) != 0;
  }

  // Returns true if the authentication scheme is connection-based, for
  // example, NTLM.  A connection-based authentication scheme does not support
  // preemptive authentication, and must use the same handler object
  // throughout the life of an HTTP transaction.
  bool is_connection_based() const {
    return (properties_ & IS_CONNECTION_BASED) != 0;
  }

  // This HttpAuthHandler's bound NetLog.
  const NetLogWithSource& net_log() const { return net_log_; }

  // If NeedsIdentity() returns true, then a subsequent call to
  // GenerateAuthToken() must indicate which identity to use. This can be done
  // either by passing in a non-empty set of credentials, or an empty set to
  // force the handler to use the default credentials. The latter is only an
  // option if AllowsDefaultCredentials() returns true.
  //
  // If NeedsIdentity() returns false, then the handler is already bound to an
  // identity and GenerateAuthToken() will ignore any credentials that are
  // passed in.
  //
  // TODO(wtc): Find a better way to handle a multi-round challenge-response
  // sequence used by a connection-based authentication scheme.
  virtual bool NeedsIdentity();

  // Returns whether the default credentials may be used for the |origin| passed
  // into |InitFromChallenge|. If true, the user does not need to be prompted
  // for username and password to establish credentials.
  // NOTE: SSO is a potential security risk.
  // TODO(cbentzel): Add a pointer to Firefox documentation about risk.
  virtual bool AllowsDefaultCredentials();

  // Returns whether explicit credentials can be used with this handler.  If
  // true the user may be prompted for credentials if an implicit identity
  // cannot be determined.
  virtual bool AllowsExplicitCredentials();

 protected:
  enum Property {
    ENCRYPTS_IDENTITY = 1 << 0,
    IS_CONNECTION_BASED = 1 << 1,
  };

  // Initializes the handler using a challenge issued by a server.
  // |challenge| must be non-nullptr and have already tokenized the
  // authentication scheme, but none of the tokens occurring after the
  // authentication scheme.
  //
  // If the request was sent over an encrypted connection, |ssl_info| is valid
  // and describes the connection.
  //
  // Implementations are expected to initialize the following members:
  // scheme_, realm_, score_, properties_
  virtual bool Init(HttpAuthChallengeTokenizer* challenge,
                    const SSLInfo& ssl_info) = 0;

  // |GenerateAuthTokenImpl()} is the auth-scheme specific implementation
  // of generating the next auth token. Callers should use |GenerateAuthToken()|
  // which will in turn call |GenerateAuthTokenImpl()|
  virtual int GenerateAuthTokenImpl(const AuthCredentials* credentials,
                                    const HttpRequestInfo* request,
                                    CompletionOnceCallback callback,
                                    std::string* auth_token) = 0;

  // See HandleAnotherChallenge() above. HandleAuthChallengeImpl is the
  // scheme-specific implementation. Callers should use HandleAnotherChallenge()
  // instead.
  virtual HttpAuth::AuthorizationResult HandleAnotherChallengeImpl(
      HttpAuthChallengeTokenizer* challenge) = 0;

  // The auth-scheme as an enumerated value.
  HttpAuth::Scheme auth_scheme_;

  // The realm, encoded as UTF-8. Used by "basic" and "digest".
  std::string realm_;

  // The auth challenge.
  std::string auth_challenge_;

  // The {scheme, host, port} for the authentication target.  Used by "ntlm"
  // and "negotiate" to construct the service principal name.
  GURL origin_;

  // The score for this challenge. Higher numbers are better.
  int score_;

  // Whether this authentication request is for a proxy server, or an
  // origin server.
  HttpAuth::Target target_;

  // A bitmask of the properties of the authentication scheme.
  int properties_;

 private:
  void OnGenerateAuthTokenComplete(int rv);
  void FinishGenerateAuthToken(int rv);

  // NetLog that should be used for logging events generated by this
  // HttpAuthHandler.
  NetLogWithSource net_log_;

  CompletionOnceCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(HttpAuthHandler);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_H_
